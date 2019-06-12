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

#include "wsiToDcm.h"
#include <dcmtk/dcmdata/dcuid.h>
#include <algorithm>
#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <boost/filesystem.hpp>
#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <boost/thread/thread.hpp>
#include <fstream>
#include <thread>
#include <utility>
#include <vector>
#include "dcmFileDraft.h"
#include "dcmTags.h"
#include "frame.h"
using namespace std;

inline void dimensionDownsampling(int64_t &frameWidhtDownsampled,
                                  int64_t &frameHeightDownsampled,
                                  int64_t &levelWidhtDownsampled,
                                  int64_t &levelHeightDownsampled,
                                  int64_t levelWidht, int64_t levelHeight,
                                  bool retile, int level,
                                  double downsampleOfLevel) {
  if (retile && level > 0) {
    frameWidhtDownsampled = frameWidhtDownsampled * downsampleOfLevel;
    frameHeightDownsampled = frameHeightDownsampled * downsampleOfLevel;
  }

  if (levelWidht <= frameWidhtDownsampled &&
      levelHeight <= frameHeightDownsampled) {
    if (levelWidht >= levelHeight) {
      double smallLevelDownsample =
          double(levelWidht) / double(frameWidhtDownsampled);
      frameHeightDownsampled = frameHeightDownsampled * smallLevelDownsample;
      frameWidhtDownsampled = levelWidht;
    } else {
      double smallLevelDownsample =
          double(levelHeight) / double(frameHeightDownsampled);
      frameWidhtDownsampled = frameWidhtDownsampled * smallLevelDownsample;
      frameHeightDownsampled = levelHeight;
    }
  }

  if (retile && level > 0) {
    levelWidhtDownsampled = levelWidht / downsampleOfLevel;
    levelHeightDownsampled = levelHeight / downsampleOfLevel;
  }
}

void sliceFrame(Frame *frame) { frame->sliceFrame(); }

void saveFile(dcmFileDraft *dcmdraft) {
  dcmdraft->saveFile();
  delete dcmdraft;
}

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

inline DCM_Compression compressionFromString(string compressionStr) {
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

void WsiToDcm::checkArguments(string inputFile, string outputFileMask,
                              long frameSizeX, long frameSizeY,
                              DCM_Compression compression, int quality,
                              int32_t startOnLevel, int32_t stopOnLevel,
                              string imageName, string studyId, string seriesId,
                              int32_t retileLevels,
                              std::vector<double> downsamples, bool tiled,
                              int batchLimit, int threads, bool debug) {
  initLogger(debug);
  isFileExist(inputFile);
  isFileExist(outputFileMask);
  if (compression == UNKNOWN) {
    BOOST_LOG_TRIVIAL(error) << "can't find compression";
    throw 1;
  }

  if (studyId.size() < 1) {
    BOOST_LOG_TRIVIAL(warning) << "studyId is going to be generated";
  }

  if (seriesId.size() < 1) {
    BOOST_LOG_TRIVIAL(warning) << "seriesId is going to be generated";
  }

  if (threads < 1) {
    BOOST_LOG_TRIVIAL(warning)
        << "threads parameter is less than 1, consuming all avalible threads";
  }
  if (batchLimit < 1) {
    BOOST_LOG_TRIVIAL(warning)
        << "batch parameter is not set, batch is unlimited";
  }
}

int WsiToDcm::dicomizeTiff(string inputFile, string outputFileMask,
                           long frameWidht, long frameHeight,
                           DCM_Compression compression, int quality,
                           int32_t startOnLevel, int32_t stopOnLevel,
                           string imageName, string studyId, string seriesId,
                           string jsonFile, int32_t retileLevels,
                           vector<double> downsamples, bool tiled,
                           int batchLimit, int threads) {
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

  DcmTags *tags = new DcmTags();
  if (jsonFile.size() > 0) {
    tags->readJsonFile(jsonFile);
  }
  int64_t levelWidht;
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

  size_t threadsForPool = thread::hardware_concurrency();
  if (threads > 0) {
    threadsForPool = threads;
  }

  BOOST_LOG_TRIVIAL(info) << "dicomization is started";
  vector<thread> Pool;
  boost::asio::thread_pool pool(threadsForPool);
  for (int32_t level = startOnLevel;
       level < levels && (stopOnLevel < startOnLevel || level <= stopOnLevel);
       level++) {
    uint32_t row = 1;
    uint32_t column = 1;
    int32_t levelToGet = level;
    if (retile) {
      levelToGet = 0;
    }
    double downsample = 1.;
    if (retile) {
      if (downsamples.size() > level && downsamples[level] >= 1) {
        downsample = downsamples[level];
      } else {
        downsample = ((int64_t)pow(2, level));
      }
      levelToGet = openslide_get_best_level_for_downsample(osr, downsample);
    }
    openslide_get_level_dimensions(osr, levelToGet, &levelWidht, &levelHeight);
    double multiplicator = openslide_get_level_downsample(osr, levelToGet);
    BOOST_LOG_TRIVIAL(debug) << "level size: " << levelWidht << ' '
                             << levelHeight << ' ' << multiplicator << endl;
    int batchNumber = 0;
    int64_t frameWidhtDownsampled = frameWidht;
    int64_t frameHeightDownsampled = frameHeight;
    int64_t levelWidhtDownsampled = levelWidht;
    int64_t levelHeightDownsampled = levelHeight;
    int64_t x = 0;
    int64_t y = 0;
    uint32_t numberOfFrames = 0;
    uint32_t batch = 0;
    vector<Frame *> framesData;
    double downsampleOfLevel = downsample / multiplicator;
    dimensionDownsampling(frameWidhtDownsampled, frameHeightDownsampled,
                          levelWidhtDownsampled, levelHeightDownsampled,
                          levelWidht, levelHeight, retile, level,
                          downsampleOfLevel);
    while (y < levelHeight) {
      while (x < levelWidht) {
        assert(osr != nullptr && openslide_get_error(osr) == nullptr);
        BOOST_LOG_TRIVIAL(debug)
            << "x: " << x << " y: " << y << " level: " << level << endl;
        Frame *frameData =
            new Frame(osr, x, y, levelToGet, frameWidhtDownsampled,
                      frameHeightDownsampled, multiplicator, frameWidht,
                      frameHeight, compression, quality);
        framesData.push_back(frameData);
        boost::asio::post(pool, boost::bind(sliceFrame, frameData));
        numberOfFrames++;
        batch++;
        x += frameWidhtDownsampled;

        if (batchLimit > 0 && batch >= batchLimit) {
          dcmFileDraft *filedraft = new dcmFileDraft(
              framesData, outputFileMask, numberOfFrames, levelWidhtDownsampled,
              levelHeightDownsampled, level, batchNumber, batch, row, column,
              frameWidht, frameHeight, studyId, seriesId, imageName,
              compression, tiled, tags, firstLevelWidthMm, firstLevelHeightMm);
          boost::asio::post(pool, boost::bind(saveFile, filedraft));
          batchNumber++;
          row = uint32_t((y + frameHeightDownsampled + 1) /
                         (frameHeightDownsampled - 1));
          column = uint32_t((x + frameWidhtDownsampled + 1) /
                            (frameWidhtDownsampled - 1));
          framesData.clear();
          batch = 0;
        }
      }
      y += frameHeightDownsampled;
      x = 0;
    }
    if (framesData.size() > 0) {
      dcmFileDraft *filedraft = new dcmFileDraft(
          framesData, outputFileMask, numberOfFrames, levelWidhtDownsampled,
          levelHeightDownsampled, level, batchNumber, batch, row, column,
          frameWidht, frameHeight, studyId, seriesId, imageName, compression,
          tiled, tags, firstLevelWidthMm, firstLevelHeightMm);
      boost::asio::post(pool, boost::bind(saveFile, filedraft));
      framesData.clear();
    }
  }
  pool.join();
  openslide_close(osr);
  delete tags;
  BOOST_LOG_TRIVIAL(info) << "dicomization is done";
  return 0;
}

int WsiToDcm::wsi2dcm(string inputFile, string outputFileMask, long frameSizeX,
                      long frameSizeY, string compressionStr, int quality,
                      int32_t startOnLevel, int32_t stopOnLevel,
                      string imageName, string studyId, string seriesId,
                      string jsonFile, int32_t retileLevels,
                      double *downsamples, bool tiled, int batchLimit,
                      int threads, bool debug) {
  DCM_Compression compression = compressionFromString(compressionStr);
  try {
    checkArguments(inputFile, outputFileMask, frameSizeX, frameSizeY,
                   compression, quality, startOnLevel, stopOnLevel, imageName,
                   studyId, seriesId, retileLevels,
                   vector<double>(downsamples, downsamples + retileLevels + 1),
                   tiled, batchLimit, threads, debug);

    return dicomizeTiff(
        inputFile, outputFileMask, frameSizeX, frameSizeY, compression, quality,
        startOnLevel, stopOnLevel, imageName, studyId, seriesId, jsonFile,
        retileLevels,
        vector<double>(downsamples, downsamples + retileLevels + 1), tiled,
        batchLimit, threads);
  } catch (int exception) {
    return 1;
  }
}

int WsiToDcm::wsi2dcm(string inputFile, string outputFileMask, long frameSizeX,
                      long frameSizeY, string compression, int quality,
                      int32_t startOnLevel, int32_t stopOnLevel,
                      string imageName, string studyId, string seriesId,
                      int32_t retileLevels, double *downsamples, bool tiled,
                      int batchLimit, int threads, bool debug) {
  return wsi2dcm(inputFile, outputFileMask, frameSizeX, frameSizeY, compression,
                 quality, startOnLevel, stopOnLevel, imageName, studyId,
                 seriesId, "", retileLevels, downsamples, tiled, batchLimit,
                 threads, debug);
}

int WsiToDcm::wsi2dcm(string inputFile, string outputFileMask, long frameSizeX,
                      long frameSizeY, string compression, int quality,
                      int32_t startWith, int32_t stopOnLevel, bool tiled,
                      int batchLimit, int threads) {
  double *empty;
  return wsi2dcm(inputFile, outputFileMask, frameSizeX, frameSizeY, compression,
                 quality, startWith, stopOnLevel, "", "", "", 0, empty, tiled,
                 batchLimit, threads, false);
}
