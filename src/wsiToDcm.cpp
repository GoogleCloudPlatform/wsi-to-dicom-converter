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

int WsiToDcm::dicomizeTiff(
    std::string inputFile, std::string outputFileMask, int64_t frameWidth,
    int64_t frameHeight, DCM_Compression compression, int32_t quality,
    int32_t startOnLevel, int32_t stopOnLevel, std::string imageName,
    std::string studyId, std::string seriesId, std::string jsonFile,
    int32_t retileLevels, std::vector<int> downsamples, bool tiled,
    int32_t batchLimit, int8_t threads, bool dropFirstRowAndColumn,
    bool stop_downsampling_at_singleframe, bool useBilinearDownsampling,
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
  const int32_t svs_level_count = levels;
  if (retile) {
    levels = retileLevels;
  }
  int64_t firstLevelWidth;
  int64_t firstLevelHeight;
  openslide_get_level_dimensions(osr, 0, &firstLevelWidth, &firstLevelHeight);

  double firstLevelMppX = 0.0;
  double firstLevelMppY = 0.0;
  const char *open_slide_firstLevelMppX =
      openslide_get_property_value(osr, "openslide.mpp-x");
  const char *open_slide_firstLevelMppY =
      openslide_get_property_value(osr, "openslide.mpp-y");
  if (open_slide_firstLevelMppX != nullptr &&
      open_slide_firstLevelMppY != nullptr) {
    firstLevelMppX = std::stod(open_slide_firstLevelMppX);
    firstLevelMppY = std::stod(open_slide_firstLevelMppY);
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
  BOOST_LOG_TRIVIAL(debug) << "Level Count: " << svs_level_count;
  bool adapative_stop_downsampling = false;

  DICOMFileFrameRegionReader higher_magnifcation_dicom_files;
  std::vector<std::unique_ptr<DcmFileDraft>> generated_dicom_files;
  for (int32_t level = startOnLevel;
       level < levels && (stopOnLevel < startOnLevel || level <= stopOnLevel) &&
       !adapative_stop_downsampling;
       level++) {
    boost::asio::thread_pool pool(threadsForPool);
    BOOST_LOG_TRIVIAL(debug) << " ";
    BOOST_LOG_TRIVIAL(debug) << "Starting Level " << level;
    uint32_t row = 1;
    uint32_t column = 1;
    int32_t levelToGet = level;
    int64_t downsample = 1;
    if (retile) {
      if (downsamples.size() > level && downsamples[level] >= 1) {
        downsample = downsamples[level];
      } else {
        downsample = static_cast<int64_t>(pow(2, level));
      }
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
      const int64_t tw = firstLevelWidth / downsample;
      const int64_t th = firstLevelHeight / downsample;
      for (levelToGet = 1; levelToGet < svs_level_count; ++levelToGet) {
        int64_t lw, lh;
        openslide_get_level_dimensions(osr, levelToGet, &lw, &lh);
        if (lw < tw || lh < th) {
          break;
        }
      }
      levelToGet -= 1;
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
    if (higher_magnifcation_dicom_files.dicom_file_count() > 0) {
      DcmFileDraft* dicom_file =
                            higher_magnifcation_dicom_files.get_dicom_file(0);
      if (dicom_file->get_downsample() < downsampleOfLevel) {
        higher_magnifcation_dicom_files.clear_dicom_files();
      }
    }
    int64_t levelWidth;
    int64_t levelHeight;
    if (higher_magnifcation_dicom_files.dicom_file_count() == 0) {
      multiplicator = floor(openslide_get_level_downsample(osr, levelToGet));
      // Downsampling factor required to go from selected
      // downsampled level to the desired level of downsampling
      downsampleOfLevel = static_cast<double>(downsample) /
                                              multiplicator;
      openslide_get_level_dimensions(osr, levelToGet,
                                     &levelWidth, &levelHeight);
    } else {
      multiplicator = -1;
      DcmFileDraft* dicom_file =
                             higher_magnifcation_dicom_files.get_dicom_file(0);
      downsampleOfLevel = static_cast<double>(downsample) /
                          static_cast<double>(dicom_file->get_downsample());
      levelWidth = dicom_file->get_image_width();
      levelHeight = dicom_file->get_image_height();
    }
    BOOST_LOG_TRIVIAL(debug) << "level size: " << levelWidth << ' '
                             << levelHeight << ' ' << multiplicator;
    int64_t frameWidthDownsampled;
    int64_t frameHeightDownsampled;
    int64_t levelWidthDownsampled;
    int64_t levelHeightDownsampled;
    int64_t x = initialX;
    int64_t y = initialY;
    uint32_t numberOfFrames = 0;
    std::vector<std::unique_ptr<Frame> > framesData;
    int64_t level_frameWidth;
    int64_t level_frameHeight;
    DCM_Compression level_compression = compression;

    // Adjust level size by starting position if skipping row and column.
    // levelHeightDownsampled and levelWidthDownsampled will reflect
    // new starting position.
    dimensionDownsampling(frameWidth, frameHeight, levelWidth - initialX,
                          levelHeight - initialY, retile, level,
                          downsampleOfLevel, &frameWidthDownsampled,
                          &frameHeightDownsampled, &levelWidthDownsampled,
                          &levelHeightDownsampled, &level_frameWidth,
                          &level_frameHeight, &level_compression);

    const int64_t width_adjustment = ((levelWidthDownsampled * downsample) -
                                      (firstLevelWidth-initialX));
    const int64_t height_adjustment = ((levelHeightDownsampled * downsample) -
                                       (firstLevelHeight-initialY));
    const double level_width_adjustment = static_cast<double>(
                                              (firstLevelWidth - initialX) +
                                               width_adjustment);
    const double level_height_adjustment = static_cast<double>(
                                              (firstLevelHeight - initialY) +
                                               height_adjustment);
    const double level_width_Mm = level_width_adjustment * firstLevelMppX /
                                  1000;
    const double level_height_Mm = level_height_adjustment * firstLevelMppY /
                                   1000;
    BOOST_LOG_TRIVIAL(debug) << "level width & height adjustment (pixels): " <<
                                 width_adjustment << ", " << height_adjustment;

    BOOST_LOG_TRIVIAL(debug) << "multiplicator: " << multiplicator;
    BOOST_LOG_TRIVIAL(debug) << "levelToGet: " << levelToGet;
    BOOST_LOG_TRIVIAL(debug) << "downsample: " << downsample;
    BOOST_LOG_TRIVIAL(debug) << "downsampleOfLevel: " << downsampleOfLevel;
    BOOST_LOG_TRIVIAL(debug) << "frameDownsampled: " << frameWidthDownsampled
                             << ", " << frameHeightDownsampled;

    BOOST_LOG_TRIVIAL(debug) << "levelDownsampled: " << levelWidthDownsampled
                             << ", " << levelHeightDownsampled;

    if (levelWidthDownsampled == 0 || levelHeightDownsampled == 0) {
      // frame is being downsampled to nothing skip file.
      BOOST_LOG_TRIVIAL(debug) << "Layer has a 0 length dimension. Skipping "
                                  "dcm generation for layer.";
      continue;
    }

    /*
      Walk through all frames in selected best layer.  Extract frames from layer
      FrameDim = (frameWidthDownsampled, frameHeightDownsampled) which are dim
      of frame scaled up to the dimension of the layer being sampled from

      Frame objects are processed via a thread pool.
      Method in Frame::sliceFrame () downsamples the imaging.

      DcmFileDraft Joins threads and combines results and writes dcm file.
    */
    BOOST_LOG_TRIVIAL(debug) << "higher_magnifcation_dicom_files " <<
                          higher_magnifcation_dicom_files.dicom_file_count();

    const int frameX = std::ceil(static_cast<double>(levelWidthDownsampled) /
                                 static_cast<double>(level_frameWidth));
    const int frameY = std::ceil(static_cast<double>(levelHeightDownsampled) /
                                 static_cast<double>(level_frameWidth));
    if (batchLimit == 0) {
      framesData.reserve(frameX * frameY);
    } else {
      framesData.reserve(std::min(frameX * frameY, batchLimit));
    }
    bool save_compressed_raw = preferProgressiveDownsampling && downsample > 1;

    while (y < levelHeight) {
      while (x < levelWidth) {
        assert(osr != nullptr && openslide_get_error(osr) == nullptr);
        std::unique_ptr<Frame> frameData;
        if (useBilinearDownsampling) {
          frameData = std::make_unique<BilinearInterpolationFrame>(
              osr, x, y, levelToGet, frameWidthDownsampled,
              frameHeightDownsampled, level_frameWidth, level_frameHeight,
              level_compression, quality, levelWidthDownsampled,
              levelHeightDownsampled, levelWidth, levelHeight, firstLevelWidth,
              firstLevelHeight, save_compressed_raw,
              higher_magnifcation_dicom_files);
        } else {
          frameData = std::make_unique<NearestNeighborFrame>(
              osr, x, y, levelToGet, frameWidthDownsampled,
              frameHeightDownsampled, multiplicator, level_frameWidth,
              level_frameHeight, level_compression, quality,
              save_compressed_raw, higher_magnifcation_dicom_files);
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
                  seriesId, imageName, level_compression, tiled, tags.get(),
                  level_width_Mm, level_height_Mm, downsample,
                  &generated_dicom_files);
          boost::asio::post(pool, [th_filedraft = filedraft.get()]() {
            th_filedraft->saveFile();
          });
          generated_dicom_files.push_back(std::move(filedraft));
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
          imageName, level_compression, tiled, tags.get(), level_width_Mm,
          level_height_Mm, downsample, &generated_dicom_files);
      boost::asio::post(pool, [th_filedraft = filedraft.get()]() {
        th_filedraft->saveFile();
      });
      generated_dicom_files.push_back(std::move(filedraft));
    }
    if (stop_downsampling_at_singleframe && numberOfFrames <= 1) {
      adapative_stop_downsampling = true;
    }
    pool.join();
    if (preferProgressiveDownsampling) {
      higher_magnifcation_dicom_files.set_dicom_files(
                                             std::move(generated_dicom_files));
    } else {
      generated_dicom_files.clear();
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
