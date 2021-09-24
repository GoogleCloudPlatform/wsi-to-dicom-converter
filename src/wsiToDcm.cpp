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
  int64_t cropSourceLevelWidth;
  int64_t cropSourceLevelHeight;
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
  int64_t cropSourceLevelWidth = 0;
  int64_t cropSourceLevelHeight = 0;
  if (smallestSlideLevel != NULL) {
    cropSourceLevelWidth = (levelWidth - initialX) %
                            smallestSlideLevel->levelWidthDownsampled;
    cropSourceLevelHeight = (levelHeight - initialY) %
                             smallestSlideLevel->levelHeightDownsampled;
  }
  int64_t frameWidthDownsampled;
  int64_t frameHeightDownsampled;
  int64_t levelWidthDownsampled;
  int64_t levelHeightDownsampled;
  int64_t levelFrameWidth;
  int64_t levelFrameHeight;
  dimensionDownsampling(frameWidth, frameHeight,
                        levelWidth - cropSourceLevelWidth - initialX,
                        levelHeight - cropSourceLevelHeight - initialY,
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
  slideLevelDim->cropSourceLevelWidth = cropSourceLevelWidth;
  slideLevelDim->cropSourceLevelHeight = cropSourceLevelHeight;
  slideLevelDim->levelCompression = levelCompression;
  slideLevelDim->readOpenslide = readOpenslide;
  return (std::move(slideLevelDim));
}

int WsiToDcm::dicomizeTiff(
    const std::string &inputFile,
    const std::string &outputFileMask,
    const int64_t frameWidth,
    const int64_t frameHeight,
    const DCM_Compression compression,
    const int quality,
    const int32_t startOnLevel, const int32_t stopOnLevel,
    const std::string &imageName,
    std::string studyId,
    std::string seriesId,
    const std::string &jsonFile,
    const int32_t retileLevels,
    const std::vector<int> &downsamples,
    const bool tiled,
    const int batchLimit, const int8_t threads,
    const bool dropFirstRowAndColumn,
    const bool stopDownsamplingAtSingleFrame,
    const bool useBilinearDownsampling,
    const bool floorCorrectDownsampling,
    const bool preferProgressiveDownsampling,
    const bool cropFrameToGenerateUniformPixelSpacing) {
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
  // Determine smallest_slide downsample dimensions to enable
  // slide pixel spacing normalization croping to ensure pixel
  // spacing across all downsample images is uniform.
  if (cropFrameToGenerateUniformPixelSpacing) {
    BOOST_LOG_TRIVIAL(debug) << "Finding smallest downsampled image to" <<
                              " generate images with uniform pixel spacing";
    for (int32_t level = startOnLevel;
        level < levels && (stopOnLevel < startOnLevel || level <= stopOnLevel);
        level++) {
      std::unique_ptr<SlideLevelDim> tempSlideLevelDim =
                          std::move(get_slide_level_dim(
                                                retile,
                                                downsamples,
                                                preferProgressiveDownsampling,
                                                floorCorrectDownsampling,
                                                osr,
                                                level,
                                                frameWidth,
                                                frameHeight,
                                                smallestSlideLevel.get(),
                                                compression,
                                                initialX,
                                                initialY,
                                                NULL));
      if (tempSlideLevelDim->levelWidthDownsampled == 0 ||
          tempSlideLevelDim->levelHeightDownsampled == 0) {
        // frame is being downsampled to nothing skip file.
        BOOST_LOG_TRIVIAL(debug) << "Layer has a 0 length dimension. Skipping "
                                    "dcm generation for layer.";
        break;
      }
      smallestSlideLevel = std::move(tempSlideLevelDim);
      BOOST_LOG_TRIVIAL(debug) << "Uncropped dimensions Level[" <<
                                  level << "]: " <<
                                  smallestSlideLevel->levelWidthDownsampled <<
                                   ", " <<
                                   smallestSlideLevel->levelHeightDownsampled;

      if (stopDownsamplingAtSingleFrame) {
        const int64_t frameX = std::ceil(
            static_cast<double>(smallestSlideLevel->levelWidthDownsampled) /
            static_cast<double>(smallestSlideLevel->levelFrameWidth));
        const int64_t frameY = std::ceil(
            static_cast<double>(smallestSlideLevel->levelHeightDownsampled) /
            static_cast<double>(smallestSlideLevel->levelFrameHeight));
        BOOST_LOG_TRIVIAL(debug) << "Level[" <<level << "] frames:" <<
                                    frameX << ", " << frameY;
        if (frameX * frameY <= 1) {
          break;
        }
      }
    }
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
  int64_t firstSlideWidthCrop = 0;
  int64_t firstSlideHeightCrop = 0;

  if (cropFrameToGenerateUniformPixelSpacing)  {
    std::unique_ptr<SlideLevelDim> firstSlideLevel =
                          std::move(get_slide_level_dim(
                                                retile,
                                                downsamples,
                                                preferProgressiveDownsampling,
                                                floorCorrectDownsampling,
                                                osr,
                                                0,
                                                frameWidth,
                                                frameHeight,
                                                NULL,
                                                compression,
                                                initialX,
                                                initialY,
                                                smallestSlideLevel.get()));
    firstSlideWidthCrop = firstSlideLevel->cropSourceLevelWidth;
    firstSlideHeightCrop = firstSlideLevel->cropSourceLevelHeight;
  }
  BOOST_LOG_TRIVIAL(debug) << "Cropping source image: " <<
                              firstSlideWidthCrop <<
                              ", " <<
                              firstSlideHeightCrop;

  if (firstSlideWidthCrop >= firstLevelWidth ||
        firstSlideHeightCrop >= firstLevelHeight) {
      BOOST_LOG_TRIVIAL(info) << "Error Pixel spacing normalization " <<
                         "cropped entire input image. Cannot generate DICOM.";
      return 0;
  }

  firstLevelMppX  *= static_cast<double>(firstLevelWidth -
                                         firstSlideHeightCrop) /
                                         static_cast<double>(firstLevelWidth);
  firstLevelMppY *= static_cast<double>(firstLevelHeight -
                                        firstSlideHeightCrop) /
                                        static_cast<double>(firstLevelHeight);

  DICOMFileFrameRegionReader higherMagnifcationDicomFiles;
  std::vector<std::unique_ptr<DcmFileDraft>> generatedDicomFiles;
  std::unique_ptr<SlideLevelDim> slideLevelDim = NULL;
  for (int32_t level = startOnLevel;
        level < levels && (stopOnLevel < startOnLevel || level <= stopOnLevel);
        level++) {
    slideLevelDim = std::move(get_slide_level_dim(
                                                retile,
                                                downsamples,
                                                preferProgressiveDownsampling,
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
    const int64_t cropSourceLevelHeight = slideLevelDim->cropSourceLevelHeight;
    const int64_t cropSourceLevelWidth = slideLevelDim->cropSourceLevelWidth;
    const DCM_Compression levelCompression = slideLevelDim->levelCompression;
    if (slideLevelDim->levelWidthDownsampled == 0 ||
        slideLevelDim->levelHeightDownsampled == 0) {
      // frame is being downsampled to nothing skip image generation.
      BOOST_LOG_TRIVIAL(debug) << "Layer has a 0 length dimension. Skipping "
                                  "dcm generation for layer.";
      break;
    }
    if (slideLevelDim->readOpenslide) {
      higherMagnifcationDicomFiles.clear_dicom_files();
    }
    boost::asio::thread_pool pool(threadsForPool);
    BOOST_LOG_TRIVIAL(debug) << " ";
    BOOST_LOG_TRIVIAL(debug) << "Starting Level " << level;
    BOOST_LOG_TRIVIAL(debug) << "level size: " << levelWidth << ' '
                             << levelHeight << ' ' << multiplicator;
    uint32_t row = 1;
    uint32_t column = 1;
    int64_t x = initialX;
    int64_t y = initialY;
    size_t numberOfFrames = 0;
    std::vector<std::unique_ptr<Frame>> framesData;
    const int64_t firstLevelDimX = firstLevelWidth - firstSlideWidthCrop -
                                    initialX;
    const int64_t firstLevelDimY = firstLevelHeight - firstSlideHeightCrop -
                                   initialY;
    const int64_t widthAdjustment = ((levelWidthDownsampled * downsample) -
                                      firstLevelDimX);
    const int64_t heightAdjustment = ((levelHeightDownsampled * downsample) -
                                                              firstLevelDimY);
    const double levelWidthAdjustment = static_cast<double>(firstLevelDimX +
                                                              widthAdjustment);
    const double levelHeightAdjustment = static_cast<double>(
                                            firstLevelDimY + heightAdjustment);
    const double levelWidthMM = levelWidthAdjustment * firstLevelMppX /
                                  1000;
    const double levelHeightMM = levelHeightAdjustment * firstLevelMppY /
                                   1000;


    BOOST_LOG_TRIVIAL(debug) << "level width & height adjustment (pixels): " <<
                                 widthAdjustment << ", " << heightAdjustment;
    BOOST_LOG_TRIVIAL(debug) << "initialX,Y: " <<
                                 initialX << ", " << initialY;
    BOOST_LOG_TRIVIAL(debug) << "downsample: " << downsample;
    BOOST_LOG_TRIVIAL(debug) << "levelDownsampled: " << levelWidthDownsampled <<
                                ", " << levelHeightDownsampled;
    BOOST_LOG_TRIVIAL(debug) << "firstLevel (width,height): " <<
                                firstLevelWidth <<
                                 ", " << firstLevelHeight;
    BOOST_LOG_TRIVIAL(debug) << "first_level_crop (width/height): " <<
                                firstSlideWidthCrop <<
                                ", " << firstSlideHeightCrop;

    BOOST_LOG_TRIVIAL(debug) << "multiplicator: " << multiplicator;
    BOOST_LOG_TRIVIAL(debug) << "levelToGet: " << levelToGet;
    BOOST_LOG_TRIVIAL(debug) << "downsample: " << downsample;
    BOOST_LOG_TRIVIAL(debug) << "downsampleOfLevel: " << downsampleOfLevel;
    BOOST_LOG_TRIVIAL(debug) << "frameDownsampled: " << frameWidthDownsampled
                             << ", " << frameHeightDownsampled;

    BOOST_LOG_TRIVIAL(debug) << "higherMagnifcationDicomFiles " <<
                          higherMagnifcationDicomFiles.dicom_file_count();
    BOOST_LOG_TRIVIAL(debug) << "Croping source image: " << cropSourceLevelWidth
                             << ", " << cropSourceLevelHeight;

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
    bool save_compressed_raw = preferProgressiveDownsampling;
    if ((downsample == 1) &&
        (0 == get_openslide_level_for_downsample(osr, 2))) {
      // Memory optimization, if processing highest resolution image with no
      // down samples and reading downsample at level 2 will also read the
      // highest resolution image.  Do not save compressed raw versions of
      // the highest resolution.  Start progressive downsampling from
      // downsample level 2 and on.
      save_compressed_raw = false;
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

    while (y < levelHeight - cropSourceLevelHeight) {
      while (x < levelWidth - cropSourceLevelWidth) {
        assert(osr != nullptr && openslide_get_error(osr) == nullptr);
        std::unique_ptr<Frame> frameData;
        if (useBilinearDownsampling) {
          frameData = std::make_unique<BilinearInterpolationFrame>(
              osr, x, y, levelToGet, frameWidthDownsampled,
              frameHeightDownsampled, levelFrameWidth, levelFrameHeight,
              levelCompression, quality, levelWidthDownsampled,
              levelHeightDownsampled, levelWidth, levelHeight, firstLevelWidth,
              firstLevelHeight, save_compressed_raw,
              higherMagnifcationDicomFiles);
        } else {
          frameData = std::make_unique<NearestNeighborFrame>(
              osr, x, y, levelToGet, frameWidthDownsampled,
              frameHeightDownsampled, multiplicator, levelFrameWidth,
              levelFrameHeight, levelCompression, quality,
              save_compressed_raw, higherMagnifcationDicomFiles);
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
    if (preferProgressiveDownsampling) {
      higherMagnifcationDicomFiles.set_dicom_files(
                                             std::move(generatedDicomFiles));
    } else {
      generatedDicomFiles.clear();
    }
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
        wsiRequest.preferProgressiveDownsampling,
        wsiRequest.cropFrameToGenerateUniformPixelSpacing);
  } catch (int exception) {
    return 1;
  }
}

}  // namespace wsiToDicomConverter
