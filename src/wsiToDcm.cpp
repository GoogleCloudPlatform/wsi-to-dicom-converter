// Copyright 2019 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "src/wsiToDcm.h"

#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/filesystem.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <boost/thread/thread.hpp>
#include <dcmtk/dcmdata/dcuid.h>

#include <algorithm>
#include <fstream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "src/bilinearinterpolationframe.h"
#include "src/dcmFileDraft.h"
#include "src/dcmTags.h"
#include "src/dicom_file_region_reader.h"
#include "src/geometryUtils.h"
#include "src/nearestneighborframe.h"

namespace wsiToDicomConverter {

class SlideLevelDim {
 public:
  int32_t levelToGet;
  int64_t downsample;
  double multiplicator;
  double downsampleOfLevel;
  int64_t levelWidth;
  int64_t levelHeight;
  int64_t frameWidthDownsampled;
  int64_t frameHeightDownsampled;
  int64_t levelWidthDownsampled;
  int64_t levelHeightDownsampled;
  int64_t levelFrameWidth;
  int64_t levelFrameHeight;
  DCM_Compression levelCompression;
  bool readOpenslide;
};

inline void isFileExist(const std::string &name) {
  if (!boost::filesystem::exists(name)) {
    BOOST_LOG_TRIVIAL(error) << "can't access " << name;
    throw 1;
  }
}

inline void initLogger(bool debug) {
  if (!debug) {
    boost::log::core::get()->set_filter(boost::log::trivial::severity >=
                                        boost::log::trivial::info);
  }
}

inline DCM_Compression compressionFromString(std::string compressionStr) {
  DCM_Compression compression = UNKNOWN;
  std::transform(compressionStr.begin(), compressionStr.end(),
                 compressionStr.begin(), ::tolower);
  if (compressionStr.find("jpeg") == 0) {
    compression = JPEG;
  }
  if (compressionStr.find("jpeg2000") == 0) {
    compression = JPEG2000;
  }
  if (compressionStr.find("none") == 0 || compressionStr.find("raw") == 0) {
    compression = RAW;
  }
  return compression;
}

void WsiToDcm::checkArguments(WsiRequest wsiRequest) {
  initLogger(wsiRequest.debug);
  isFileExist(wsiRequest.inputFile);
  isFileExist(wsiRequest.outputFileMask);
  if (wsiRequest.compression == UNKNOWN) {
    BOOST_LOG_TRIVIAL(error) << "can't find compression";
    throw 1;
  }

  if (wsiRequest.studyId.size() < 1) {
    BOOST_LOG_TRIVIAL(warning) << "studyId is going to be generated";
  }

  if (wsiRequest.seriesId.size() < 1) {
    BOOST_LOG_TRIVIAL(warning) << "seriesId is going to be generated";
  }

  if (wsiRequest.threads < 1) {
    BOOST_LOG_TRIVIAL(warning)
        << "threads parameter is less than 1, consuming all avalible threads";
  }
  if (wsiRequest.batchLimit < 1) {
    BOOST_LOG_TRIVIAL(warning)
        << "batch parameter is not set, batch is unlimited";
  }
}

int32_t get_openslide_level_for_downsample(openslide_t *osr,
                                           int64_t downsample) {
    /*
      Openslide API  identifies image closest to the downsampled image in size
      with the API call: openslide_get_best_level_for_downsample(osr,
      downsample); optimal level selection selects the level with
      magnification above required level. Downsample acquistions can result in
      image dimensions which are non-interger multiples of the highest
      magnification which can result in the openslide_get_level_downsample
      reporting level downsampling of a non-fixed multiple:

      Example: Aperio svs imaging,  E.g. (40x -> 10x reports the 10x image has
      having a downsampling factor of 4.00018818010427.)

      The code below, computes the desired frame dimensions and then selects
      the frame which is the best match.
    */
    int64_t firstLevelWidth;
    int64_t firstLevelHeight;
    openslide_get_level_dimensions(osr, 0, &firstLevelWidth, &firstLevelHeight);
    const int64_t tw = firstLevelWidth / downsample;
    const int64_t th = firstLevelHeight / downsample;
    const int32_t svsLevelCount = openslide_get_level_count(osr);
    int32_t levelToGet;
    for (levelToGet = 1; levelToGet < svsLevelCount; ++levelToGet) {
      int64_t lw, lh;
      openslide_get_level_dimensions(osr, levelToGet, &lw, &lh);
      if (lw < tw || lh < th) {
        break;
      }
    }
    return (levelToGet - 1);
}

std::unique_ptr<SlideLevelDim> get_slide_level_dim(
                                  const bool retile,
                                  const std::vector<int> &downsamples,
                                  const bool preferProgressiveDownsampling,
                                  const bool floorCorrectDownsampling,
                                  openslide_t *osr,
                                  const int32_t level,
                                  const int64_t frameWidth,
                                  const int64_t frameHeight,
                                  SlideLevelDim *prior_level,
                                  DCM_Compression levelCompression,
                                  const int64_t initialX,
                                  const int64_t initialY,
                                  SlideLevelDim * smallestSlideLevel) {
  int32_t levelToGet = level;
  int64_t downsample = 1;
  bool readOpenslide = false;
  if (retile) {
    if (downsamples.size() > level && downsamples[level] >= 1) {
      downsample = downsamples[level];
    } else {
      downsample = static_cast<int64_t>(pow(2, level));
    }
  }
  /*
     DICOM requires uniform pixel spacing across downsampled image
     for pixel spacing based metrics to produce images with compatiable
     coordinate systems across zoom levels.

     Downsampled acquistions can have in image dimensions which are
     non-interger multiples of the highest magnification. Example: Aperio svs
     imaging, E.g. (40x -> 10x reports the 10x image has having a
     downsampling  factor of 4.00018818010427. This results in non-uniform
     scaling of the pixels and can result in small, but signifcant
     mis-alignment in the downsampled imageing. Flooring, the multiplier
     returned by openslide_get_level_downsample corrects this by restoring
     consistent downsamping and pixel spacing across the image.
  */
  double multiplicator;
  double downsampleOfLevel;
  int64_t levelWidth;
  int64_t levelHeight;
  bool generateUsingOpenSlide = true;
  // ProgressiveDownsampling
  if (preferProgressiveDownsampling) {
    if (prior_level != NULL) {
      multiplicator = static_cast<double>(prior_level->downsample);
      downsampleOfLevel = static_cast<double>(downsample) / multiplicator;
      // check that downsampling is going from higher to lower magnification
      if (downsampleOfLevel >= 1.0) {
        levelWidth = prior_level->levelWidthDownsampled;
        levelHeight = prior_level->levelHeightDownsampled;
        generateUsingOpenSlide = false;
      }
    }
  }

  // if no higherMagnifcationDicomFiles then downsample from openslide
  if (generateUsingOpenSlide) {
    levelToGet = get_openslide_level_for_downsample(osr, downsample);

    multiplicator = openslide_get_level_downsample(osr, levelToGet);
    // Downsampling factor required to go from selected
    // downsampled level to the desired level of downsampling
    if (floorCorrectDownsampling) {
      multiplicator = floor(multiplicator);
    }
    downsampleOfLevel = static_cast<double>(downsample) / multiplicator;
    openslide_get_level_dimensions(osr, levelToGet,
                                   &levelWidth, &levelHeight);
    readOpenslide = true;
  }
  // Adjust level size by starting position if skipping row and column.
  // levelHeightDownsampled and levelWidthDownsampled will reflect
  // new starting position.
  int64_t frameWidthDownsampled;
  int64_t frameHeightDownsampled;
  int64_t levelWidthDownsampled;
  int64_t levelHeightDownsampled;
  int64_t levelFrameWidth;
  int64_t levelFrameHeight;
  dimensionDownsampling(frameWidth, frameHeight,
                        levelWidth - initialX,
                        levelHeight - initialY,
                        retile, level, downsampleOfLevel,
                        &frameWidthDownsampled,
                        &frameHeightDownsampled,
                        &levelWidthDownsampled,
                        &levelHeightDownsampled,
                        &levelFrameWidth,
                        &levelFrameHeight,
                        &levelCompression);

  std::unique_ptr<SlideLevelDim> slideLevelDim;
  slideLevelDim = std::make_unique<SlideLevelDim>();
  slideLevelDim->levelToGet = levelToGet;
  slideLevelDim->downsample = downsample;
  slideLevelDim->multiplicator = multiplicator;
  slideLevelDim->downsampleOfLevel = downsampleOfLevel;
  slideLevelDim->levelWidth = levelWidth;
  slideLevelDim->levelHeight = levelHeight;
  slideLevelDim->frameWidthDownsampled = frameWidthDownsampled;
  slideLevelDim->frameHeightDownsampled = frameHeightDownsampled;
  slideLevelDim->levelWidthDownsampled = levelWidthDownsampled;
  slideLevelDim->levelHeightDownsampled = levelHeightDownsampled;
  slideLevelDim->levelFrameWidth = levelFrameWidth;
  slideLevelDim->levelFrameHeight = levelFrameHeight;
  slideLevelDim->levelCompression = levelCompression;
  slideLevelDim->readOpenslide = readOpenslide;
  return (std::move(slideLevelDim));
}

int WsiToDcm::dicomizeTiff(
    std::string inputFile, std::string outputFileMask, int64_t frameWidth,
    int64_t frameHeight, DCM_Compression compression, int quality,
    int32_t startOnLevel, int32_t stopOnLevel, std::string imageName,
    std::string studyId, std::string seriesId, std::string jsonFile,
    int32_t retileLevels, std::vector<int> downsamples, bool tiled,
    int batchLimit, int8_t threads, bool dropFirstRowAndColumn,
    bool stopDownsamplingAtSingleFrame, bool useBilinearDownsampling,
    bool floorCorrectDownsampling, bool preferProgressiveDownsampling) {
  bool retile = retileLevels > 0;

  if (studyId.size() < 1) {
    char studyIdGenerated[100];
    dcmGenerateUniqueIdentifier(studyIdGenerated, SITE_STUDY_UID_ROOT);
    studyId = studyIdGenerated;
  }

  if (seriesId.size() < 1) {
    char seriesIdGenerated[100];
    dcmGenerateUniqueIdentifier(seriesIdGenerated, SITE_SERIES_UID_ROOT);
    seriesId = seriesIdGenerated;
  }

  std::unique_ptr<DcmTags> tags = std::make_unique<DcmTags>();
  if (jsonFile.size() > 0) {
    tags->readJsonFile(jsonFile);
  }

  const char *slideFile = inputFile.c_str();
  if (!openslide_detect_vendor(slideFile)) {
    BOOST_LOG_TRIVIAL(error) << "file format is not supported by openslide";
    throw 1;
  }
  openslide_t *osr = openslide_open(slideFile);
  if (openslide_get_error(osr)) {
    BOOST_LOG_TRIVIAL(error) << openslide_get_error(osr);
    throw 1;
  }
  int32_t levels = openslide_get_level_count(osr);
  const int32_t svsLevelCount = levels;
  if (retile) {
    levels = retileLevels;
  }
  int64_t firstLevelWidth;
  int64_t firstLevelHeight;
  openslide_get_level_dimensions(osr, 0, &firstLevelWidth, &firstLevelHeight);

  double firstLevelMppX = 0.0;
  double firstLevelMppY = 0.0;
  const char *openslideFirstLevelMppX =
      openslide_get_property_value(osr, "openslide.mpp-x");
  const char *openslideFirstLevelMppY =
      openslide_get_property_value(osr, "openslide.mpp-y");
  if (openslideFirstLevelMppX != nullptr &&
      openslideFirstLevelMppY != nullptr) {
    firstLevelMppX = std::stod(openslideFirstLevelMppX);
    firstLevelMppY = std::stod(openslideFirstLevelMppY);
  }

  int8_t threadsForPool = boost::thread::hardware_concurrency();
  if (threads > 0) {
    threadsForPool = threads;
  }

  BOOST_LOG_TRIVIAL(info) << "dicomization is started";
  int initialX = 0;
  int initialY = 0;
  if (dropFirstRowAndColumn) {
    initialX = 1;
    initialY = 1;
  }
  BOOST_LOG_TRIVIAL(debug) << " ";
  BOOST_LOG_TRIVIAL(debug) << "Level Count: " << svsLevelCount;
  std::unique_ptr<SlideLevelDim> smallestSlideLevel = NULL;


  DICOMFileFrameRegionReader higherMagnifcationDicomFiles;
  std::vector<std::unique_ptr<DcmFileDraft>> generatedDicomFiles;
  std::unique_ptr<SlideLevelDim> slideLevelDim = NULL;
  for (int32_t level = startOnLevel;
       level < levels && (stopOnLevel < startOnLevel || level <= stopOnLevel);
       level++) {
    const bool progressiveDownsample = preferProgressiveDownsampling &&
                           higherMagnifcationDicomFiles.dicom_file_count() > 0;
    slideLevelDim = std::move(get_slide_level_dim(
                                                retile,
                                                downsamples,
                                                progressiveDownsample,
                                                floorCorrectDownsampling,
                                                osr,
                                                level,
                                                frameWidth,
                                                frameHeight,
                                                slideLevelDim.get(),
                                                compression,
                                                initialX,
                                                initialY,
                                                smallestSlideLevel.get()));
    const int64_t downsample = slideLevelDim->downsample;
    const int32_t levelToGet  = slideLevelDim->levelToGet;
    const double multiplicator = slideLevelDim->multiplicator;
    const double downsampleOfLevel = slideLevelDim->downsampleOfLevel;
    const int64_t levelWidth = slideLevelDim->levelWidth;
    const int64_t levelHeight = slideLevelDim->levelHeight;
    const int64_t frameWidthDownsampled = slideLevelDim->frameWidthDownsampled;
    const int64_t frameHeightDownsampled =
                                       slideLevelDim->frameHeightDownsampled;
    const int64_t levelWidthDownsampled = slideLevelDim->levelWidthDownsampled;
    const int64_t levelHeightDownsampled =
                                       slideLevelDim->levelHeightDownsampled;
    const int64_t levelFrameWidth = slideLevelDim->levelFrameWidth;
    const int64_t levelFrameHeight = slideLevelDim->levelFrameHeight;
    const DCM_Compression levelCompression = slideLevelDim->levelCompression;
    if (levelWidthDownsampled == 0 || levelHeightDownsampled == 0) {
      // frame is being downsampled to nothing skip image generation.
      BOOST_LOG_TRIVIAL(debug) << "Layer has a 0 length dimension. Skipping "
                                  "dcm generation for layer.";
      break;
    }
    if (slideLevelDim->readOpenslide) {
      // If slide level was initalized from openslide
      // clear higherMagnifcationDicomFiles so
      // level is downsampled from openslide and not
      // prior level if progressiveDownsample is enabled.
      higherMagnifcationDicomFiles.clear_dicom_files();
    }
    boost::asio::thread_pool pool(threadsForPool);
    BOOST_LOG_TRIVIAL(debug) << " ";
    BOOST_LOG_TRIVIAL(debug) << "Starting Level " << level;
    uint32_t row = 1;
    uint32_t column = 1;
    BOOST_LOG_TRIVIAL(debug) << "level size: " << levelWidth << ' '
                             << levelHeight << ' ' << multiplicator;
    int64_t x = initialX;
    int64_t y = initialY;
    size_t numberOfFrames = 0;
    std::vector<std::unique_ptr<Frame>> framesData;
    const int64_t firstLevelDimX = firstLevelWidth - initialX;
    const int64_t firstLevelDimY = firstLevelHeight - initialY;
    const int64_t widthAdjustment = ((levelWidthDownsampled * downsample) -
                                      firstLevelDimX);
    const int64_t heightAdjustment = ((levelHeightDownsampled * downsample) -
                                                              firstLevelDimY);
    const double levelWidthAdjustment = static_cast<double>(firstLevelDimX +
                                                              widthAdjustment);
    const double levelHeightAdjustment = static_cast<double>(
                                            firstLevelDimY + heightAdjustment);
    const double levelWidthMM = levelWidthAdjustment * firstLevelMppX / 1000;
    const double levelHeightMM = levelHeightAdjustment * firstLevelMppY / 1000;

    BOOST_LOG_TRIVIAL(debug) << "level width & height adjustment (pixels): " <<
                                 widthAdjustment << ", " << heightAdjustment;

    BOOST_LOG_TRIVIAL(debug) << "multiplicator: " << multiplicator;
    BOOST_LOG_TRIVIAL(debug) << "levelToGet: " << levelToGet;
    BOOST_LOG_TRIVIAL(debug) << "downsample: " << downsample;
    BOOST_LOG_TRIVIAL(debug) << "downsampleOfLevel: " << downsampleOfLevel;
    BOOST_LOG_TRIVIAL(debug) << "frameDownsampled: " << frameWidthDownsampled
                             << ", " << frameHeightDownsampled;
    BOOST_LOG_TRIVIAL(debug) << "higherMagnifcationDicomFiles " <<
                          higherMagnifcationDicomFiles.dicom_file_count();

    // Preallocate vector space for frames
    const int frameX = std::ceil(static_cast<double>(levelWidthDownsampled) /
                                 static_cast<double>(levelFrameWidth));
    const int frameY = std::ceil(static_cast<double>(levelHeightDownsampled) /
                                 static_cast<double>(levelFrameHeight));
    if (batchLimit == 0) {
      framesData.reserve(frameX * frameY);
    } else {
      framesData.reserve(std::min(frameX * frameY, batchLimit));
    }
    bool saveCompressedRaw = preferProgressiveDownsampling;
    if ((downsample == 1) &&
        (0 == get_openslide_level_for_downsample(osr, 2))) {
      // Memory optimization, if processing highest resolution image with no
      // down samples and reading downsample at level 2 will also read the
      // highest resolution image.  Do not save compressed raw versions of
      // the highest resolution.  Start progressive downsampling from
      // downsample level 2 and on.
      saveCompressedRaw = false;
    }
    //  Walk through all frames in selected best layer. Extract frames from
    //  layer FrameDim = (frameWidthDownsampled, frameHeightDownsampled)
    //  which are dim of frame scaled up to the dimension of the layer being
    //  sampled from
    //
    //  Frame objects are processed via a thread pool.
    //  Method in Frame::sliceFrame () downsamples the imaging.
    //
    //  DcmFileDraft Joins threads and combines results and writes dcm file.

    while (y < levelHeight) {
      while (x < levelWidth) {
        assert(osr != nullptr && openslide_get_error(osr) == nullptr);
        std::unique_ptr<Frame> frameData;
        if (useBilinearDownsampling) {
          frameData = std::make_unique<BilinearInterpolationFrame>(
              osr, x, y, levelToGet, frameWidthDownsampled,
              frameHeightDownsampled, levelFrameWidth, levelFrameHeight,
              levelCompression, quality, levelWidthDownsampled,
              levelHeightDownsampled, levelWidth, levelHeight, firstLevelWidth,
              firstLevelHeight, saveCompressedRaw,
              higherMagnifcationDicomFiles);
        } else {
          frameData = std::make_unique<NearestNeighborFrame>(
              osr, x, y, levelToGet, frameWidthDownsampled,
              frameHeightDownsampled, multiplicator, levelFrameWidth,
              levelFrameHeight, levelCompression, quality,
              saveCompressedRaw, higherMagnifcationDicomFiles);
        }

        boost::asio::post(
            pool, [frameData = frameData.get()]() { frameData->sliceFrame(); });
        framesData.push_back(std::move(frameData));
        numberOfFrames++;
        x += frameWidthDownsampled;

        if (batchLimit > 0 && framesData.size() >= batchLimit) {
          std::unique_ptr<DcmFileDraft> filedraft =
              std::make_unique<DcmFileDraft>(
                  std::move(framesData), outputFileMask, levelWidthDownsampled,
                  levelHeightDownsampled, level, row, column, studyId,
                  seriesId, imageName, levelCompression, tiled, tags.get(),
                  levelWidthMM, levelHeightMM, downsample,
                  &generatedDicomFiles);
          boost::asio::post(pool, [th_filedraft = filedraft.get()]() {
            th_filedraft->saveFile();
          });
          generatedDicomFiles.push_back(std::move(filedraft));
          row = static_cast<uint32_t>((y + frameHeightDownsampled + 1) /
                         (frameHeightDownsampled - 1));
          column = static_cast<uint32_t>((x + frameWidthDownsampled + 1) /
                            (frameWidthDownsampled - 1));
        }
      }
      y += frameHeightDownsampled;
      x = initialX;
    }
    if (framesData.size() > 0) {
      std::unique_ptr<DcmFileDraft> filedraft = std::make_unique<DcmFileDraft>(
          std::move(framesData), outputFileMask, levelWidthDownsampled,
          levelHeightDownsampled, level, row, column, studyId, seriesId,
          imageName, levelCompression, tiled, tags.get(), levelWidthMM,
          levelHeightMM, downsample, &generatedDicomFiles);
      boost::asio::post(pool, [th_filedraft = filedraft.get()]() {
        th_filedraft->saveFile();
      });
      generatedDicomFiles.push_back(std::move(filedraft));
    }
    pool.join();
    if  (!saveCompressedRaw) {
      generatedDicomFiles.clear();
    }
    higherMagnifcationDicomFiles.set_dicom_files(
                                           std::move(generatedDicomFiles));
    if (stopDownsamplingAtSingleFrame && numberOfFrames <= 1) {
      break;
    }
  }
  openslide_close(osr);
  BOOST_LOG_TRIVIAL(info) << "dicomization is done";
  return 0;
}

int WsiToDcm::wsi2dcm(WsiRequest wsiRequest) {
  try {
    checkArguments(wsiRequest);

    return dicomizeTiff(
        wsiRequest.inputFile, wsiRequest.outputFileMask, wsiRequest.frameSizeX,
        wsiRequest.frameSizeY, wsiRequest.compression, wsiRequest.quality,
        wsiRequest.startOnLevel, wsiRequest.stopOnLevel, wsiRequest.imageName,
        wsiRequest.studyId, wsiRequest.seriesId, wsiRequest.jsonFile,
        wsiRequest.retileLevels,
        std::vector<int>(
            wsiRequest.downsamples,
            wsiRequest.downsamples + wsiRequest.retileLevels + 1),
        wsiRequest.tiled, wsiRequest.batchLimit, wsiRequest.threads,
        wsiRequest.dropFirstRowAndColumn,
        wsiRequest.stopDownsamplingAtSingleFrame,
        wsiRequest.useBilinearDownsampling,
        wsiRequest.floorCorrectDownsampling,
        wsiRequest.preferProgressiveDownsampling);
  } catch (int exception) {
    return 1;
  }
}

}  // namespace wsiToDicomConverter
