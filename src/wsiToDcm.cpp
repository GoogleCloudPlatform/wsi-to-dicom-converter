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

#include "src/dcmFileDraft.h"
#include "src/dcmTags.h"
#include "src/frame.h"
#include "src/geometryUtils.h"

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
    int64_t frameHeight, DCM_Compression compression, int quality,
    int32_t startOnLevel, int32_t stopOnLevel, std::string imageName,
    std::string studyId, std::string seriesId, std::string jsonFile,
    int32_t retileLevels, std::vector<double> downsamples, bool tiled,
    int batchLimit, int8_t threads, bool dropFirstRowAndColumn,
    bool stop_down_sampleing_at_singleframe) {
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
  int64_t levelWidth;
  int64_t levelHeight;

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
  if (retile) {
    levels = retileLevels;
  }
  int64_t firstLevelWidth;
  int64_t firstLevelHeight;
  openslide_get_level_dimensions(osr, 0, &firstLevelWidth, &firstLevelHeight);

  double firstLevelWidthMm = 0;
  double firstLevelHeightMm = 0;
  const char *firstLevelMppX =
      openslide_get_property_value(osr, "openslide.mpp-x");
  const char *firstLevelMppY =
      openslide_get_property_value(osr, "openslide.mpp-y");
  if (firstLevelMppX != nullptr && firstLevelMppY != nullptr) {
    firstLevelWidthMm = firstLevelWidth * std::stod(firstLevelMppX) / 1000;
    firstLevelHeightMm = firstLevelHeight * std::stod(firstLevelMppY) / 1000;
  }

  int8_t threadsForPool = boost::thread::hardware_concurrency();
  if (threads > 0) {
    threadsForPool = threads;
  }

  BOOST_LOG_TRIVIAL(info) << "dicomization is started";
  std::vector<boost::thread> Pool;
  boost::asio::thread_pool pool(threadsForPool);
  int initialX = 0;
  int initialY = 0;
  if (dropFirstRowAndColumn) {
    initialX = 1;
    initialY = 1;
  }
  bool adapative_stop_downsampeling = false;
  for (int32_t level = startOnLevel;
       level < levels && (stopOnLevel < startOnLevel || level <= stopOnLevel) &&
       !adapative_stop_downsampeling;
       level++) {
    BOOST_LOG_TRIVIAL(debug) << " ";
    BOOST_LOG_TRIVIAL(debug) << "Starting Level " << level;
    uint32_t row = 1;
    uint32_t column = 1;
    int32_t levelToGet = level;
    double downsample = 1.;
    if (retile) {
      if (downsamples.size() > level && downsamples[level] >= 1) {
        downsample = downsamples[level];
      } else {
        downsample = ((int64_t)pow(2, level));
      }
      levelToGet = openslide_get_best_level_for_downsample(osr, downsample);
    }
    openslide_get_level_dimensions(osr, levelToGet, &levelWidth, &levelHeight);

    double multiplicator = openslide_get_level_downsample(osr, levelToGet);
    BOOST_LOG_TRIVIAL(debug) << "level size: " << levelWidth << ' '
                             << levelHeight << ' ' << multiplicator;
    int batchNumber = 0;
    int64_t frameWidthDownsampled;
    int64_t frameHeightDownsampled;
    int64_t levelWidthDownsampled;
    int64_t levelHeightDownsampled;
    int64_t x = initialX;
    int64_t y = initialY;
    uint32_t numberOfFrames = 0;
    uint32_t batch = 0;
    std::vector<std::unique_ptr<Frame> > framesData;
    double downsampleOfLevel = downsample / multiplicator;
    int64_t level_frameWidth;
    int64_t level_frameHeight;
    DCM_Compression level_compression = compression;

    // Adjust level size by starting position if skipping row and column.
    // levelHeightDownsampled and levelWidthDownsampled will reflect
    // new starting position.
    dimensionDownsampling(
        frameWidth, frameHeight, levelWidth - x, levelHeight - y, retile, level,
        downsampleOfLevel, &frameWidthDownsampled, &frameHeightDownsampled,
        &levelWidthDownsampled, &levelHeightDownsampled, &level_frameWidth,
        &level_frameHeight, &level_compression);

    BOOST_LOG_TRIVIAL(debug) << "levelToGet: " << levelToGet;
    BOOST_LOG_TRIVIAL(debug) << "multiplicator: " << multiplicator;
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
    while (y < levelHeight) {
      while (x < levelWidth) {
        assert(osr != nullptr && openslide_get_error(osr) == nullptr);
        std::unique_ptr<Frame> frameData = std::make_unique<Frame>(
            osr, x, y, levelToGet, frameWidthDownsampled,
            frameHeightDownsampled, multiplicator, level_frameWidth,
            level_frameHeight, level_compression, quality);
        boost::asio::post(
            pool, [frameData = frameData.get()]() { frameData->sliceFrame(); });
        framesData.push_back(std::move(frameData));
        numberOfFrames++;
        batch++;
        x += frameWidthDownsampled;

        if (batchLimit > 0 && batch >= batchLimit) {
          std::unique_ptr<DcmFileDraft> filedraft =
              std::make_unique<DcmFileDraft>(
                  std::move(framesData), outputFileMask, numberOfFrames,
                  levelWidthDownsampled, levelHeightDownsampled, level,
                  batchNumber, batch, row, column, level_frameWidth,
                  level_frameHeight, studyId, seriesId, imageName,
                  level_compression, tiled, tags.get(), firstLevelWidthMm,
                  firstLevelHeightMm);
          boost::asio::post(pool, [filedraft = std::move(filedraft)]() {
            filedraft->saveFile();
          });
          batchNumber++;
          row = static_cast<uint32_t>((y + frameHeightDownsampled + 1) /
                         (frameHeightDownsampled - 1));
          column = static_cast<uint32_t>((x + frameWidthDownsampled + 1) /
                            (frameWidthDownsampled - 1));
          batch = 0;
        }
      }
      y += frameHeightDownsampled;
      x = initialX;
    }
    if (framesData.size() > 0) {
      std::unique_ptr<DcmFileDraft> filedraft = std::make_unique<DcmFileDraft>(
          std::move(framesData), outputFileMask, numberOfFrames,
          levelWidthDownsampled, levelHeightDownsampled, level, batchNumber,
          batch, row, column, level_frameWidth, level_frameHeight, studyId,
          seriesId, imageName, level_compression, tiled, tags.get(),
          firstLevelWidthMm, firstLevelHeightMm);
      boost::asio::post(pool, [filedraft = std::move(filedraft)]() {
        filedraft->saveFile();
      });
    }
    if (stop_down_sampleing_at_singleframe && numberOfFrames <= 1) {
      adapative_stop_downsampeling = true;
    }
  }
  pool.join();
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
        std::vector<double>(
            wsiRequest.downsamples,
            wsiRequest.downsamples + wsiRequest.retileLevels + 1),
        wsiRequest.tiled, wsiRequest.batchLimit, wsiRequest.threads,
        wsiRequest.dropFirstRowAndColumn,
        wsiRequest.stopDownSampelingAtSingleFrame);
  } catch (int exception) {
    return 1;
  }
}

}  // namespace wsiToDicomConverter
