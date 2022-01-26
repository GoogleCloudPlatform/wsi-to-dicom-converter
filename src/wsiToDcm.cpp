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

#include <absl/strings/string_view.h>
#include <boost/algorithm/string.hpp>
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
#include <tuple>
#include <utility>
#include <vector>

#include "src/dcmFileDraft.h"
#include "src/dcmTags.h"
#include "src/dicom_file_region_reader.h"
#include "src/geometryUtils.h"
#include "src/nearestneighborframe.h"
#include "src/opencvinterpolationframe.h"

#include "src/tiffFrame.h"

namespace wsiToDicomConverter {

inline void isFileExist(absl::string_view name) {
  std::string name_str = std::move(static_cast<std::string>(name));
  if (!boost::filesystem::exists(name_str)) {
    BOOST_LOG_TRIVIAL(error) << "can't access " << name_str;
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

WsiToDcm::WsiToDcm(WsiRequest *wsiRequest) : wsiRequest_(wsiRequest) {
  if (wsiRequest_->studyId.size() < 1) {
    char studyIdGenerated[100];
    dcmGenerateUniqueIdentifier(studyIdGenerated, SITE_STUDY_UID_ROOT);
    wsiRequest_->studyId = studyIdGenerated;
  }

  if (wsiRequest_->seriesId.size() < 1) {
    char seriesIdGenerated[100];
    dcmGenerateUniqueIdentifier(seriesIdGenerated, SITE_SERIES_UID_ROOT);
    wsiRequest_->seriesId = seriesIdGenerated;
  }

  const char *slideFile = wsiRequest_->inputFile.c_str();
  if (!openslide_detect_vendor(slideFile)) {
    BOOST_LOG_TRIVIAL(error) << "file format is not supported by openslide";
    return;
  }
  BOOST_LOG_TRIVIAL(info) << "dicomization is started";
  initialX_ = 0;
  initialY_ = 0;
  if (wsiRequest_->dropFirstRowAndColumn) {
    initialX_ = 1;
    initialY_ = 1;
  }
  const int64_t downsampleSize = wsiRequest_->downsamples.size();
  if (downsampleSize > 0) {
    if (wsiRequest_->retileLevels > 0 &&
        wsiRequest_->retileLevels+1 != downsampleSize) {
      BOOST_LOG_TRIVIAL(info) << "--levels command line parameter is " <<
                                 "unnecessary levels initialized to " <<
                                 downsampleSize << " from --downsamples.";
    }
    wsiRequest_->retileLevels = downsampleSize;
  }
  retile_ = wsiRequest_->retileLevels > 0;
  if (!retile_ && wsiRequest_->cropFrameToGenerateUniformPixelSpacing) {
    BOOST_LOG_TRIVIAL(error) << "Cropping the imaging to achieve uniform"
                                " pixel spacing requires image downsampling."
                                " Downsampling is specified using the"
                                " --downsamples or --levels command line"
                                " parameter.";
    throw 1;
  }
  customDownSampleFactorsDefined_ = false;
  for (const int ds : wsiRequest_->downsamples) {
    if (ds != 0) {
      customDownSampleFactorsDefined_ = true;
      break;
    }
  }
}

WsiToDcm::~WsiToDcm() {
}

void WsiToDcm::checkArguments() {
  if (wsiRequest_ == nullptr) {
    BOOST_LOG_TRIVIAL(error) << "request not initalized.";
    throw 1;
  }
  initLogger(wsiRequest_->debug);
  isFileExist(wsiRequest_->inputFile);
  isFileExist(wsiRequest_->outputFileMask);
  if (wsiRequest_->compression == UNKNOWN) {
    BOOST_LOG_TRIVIAL(error) << "can't find compression";
    throw 1;
  }
  if (wsiRequest_->studyId.size() < 1) {
    BOOST_LOG_TRIVIAL(warning) << "studyId is going to be generated";
  }

  if (wsiRequest_->seriesId.size() < 1) {
    BOOST_LOG_TRIVIAL(warning) << "seriesId is going to be generated";
  }

  if (wsiRequest_->threads < 1) {
    BOOST_LOG_TRIVIAL(warning)
        << "threads parameter is less than 1, consuming all avalible threads";
  }
  if (wsiRequest_->batchLimit < 1) {
    BOOST_LOG_TRIVIAL(warning)
        << "batch parameter is not set, batch is unlimited";
  }
}

std::unique_ptr<OpenSlidePtr> WsiToDcm::initOpenslide() {
  std::unique_ptr<OpenSlidePtr> osptr =
                        std::make_unique<OpenSlidePtr>(wsiRequest_->inputFile);
  svsLevelCount_ = openslide_get_level_count(osptr->osr);
  // Openslide API call 0 returns dimensions of highest resolution image.
  openslide_get_level_dimensions(osptr->osr, 0,
                                 &largestSlideLevelWidth_,
                                 &largestSlideLevelHeight_);
  tiffFile_ = nullptr;
  if (wsiRequest_->SVSImportPreferScannerTileingForAllLevels ||
      wsiRequest_->SVSImportPreferScannerTileingForLargestLevel) {
    bool useSVSTileing = false;
    if (boost::algorithm::iends_with(wsiRequest_->inputFile, ".svs")) {
      tiffFile_ = std::make_unique<TiffFile>(wsiRequest_->inputFile);
      if (tiffFile_->isLoaded()) {
          int32_t level = tiffFile_->getDirectoryIndexMatchingImageDimensions(
                            largestSlideLevelWidth_, largestSlideLevelHeight_);
          if (level != -1) {
            tiffFile_ = std::make_unique<TiffFile>(*tiffFile_, level);
            TiffFrame tiffFrame(tiffFile_.get(), 0, true);
            if (tiffFrame.canDecodeJpeg()) {
              const TiffDirectory * tiffDir = tiffFile_->directory(level);
              BOOST_LOG_TRIVIAL(info) << "Reading JPEG tiles from SVS with "
                                         "out decoding.";
              int oldX = wsiRequest_->frameSizeX;
              int oldY = wsiRequest_->frameSizeY;
              wsiRequest_->frameSizeX = tiffDir->tileWidth();
              wsiRequest_->frameSizeY = tiffDir->tileHeight();
              BOOST_LOG_TRIVIAL(info) << "Changing generated DICOM tile size "
                                        "to jpeg tile size defined in svs. "
                                        "Command line specified tile size: " <<
                                        oldX << ", " << oldY << ". Changed to"
                                        " svs jpeg tile size: " <<
                                        wsiRequest_->frameSizeX << ", " <<
                                        wsiRequest_->frameSizeY;
              useSVSTileing = true;
            }
            tiffFile_->close();
          }
      }
    }
    if (!useSVSTileing) {
      wsiRequest_->SVSImportPreferScannerTileingForLargestLevel = false;
      wsiRequest_->SVSImportPreferScannerTileingForAllLevels = false;
    }
  }

  openslideMPP_X_ = getOpenSlideDimensionMM(osptr.get(), "openslide.mpp-x");
  openslideMPP_Y_ = getOpenSlideDimensionMM(osptr.get(), "openslide.mpp-y");
  BOOST_LOG_TRIVIAL(debug) << " ";
  BOOST_LOG_TRIVIAL(debug) << "Level Count: " << svsLevelCount_;
  return std::move(osptr);
}

int32_t WsiToDcm::getOpenslideLevelForDownsample(OpenSlidePtr *osptr,
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
    const int64_t tw = largestSlideLevelWidth_ / downsample;
    const int64_t th = largestSlideLevelHeight_ / downsample;
    int32_t levelToGet;
    for (levelToGet = 1; levelToGet < svsLevelCount_; ++levelToGet) {
      int64_t lw, lh;
      openslide_get_level_dimensions(osptr->osr, levelToGet, &lw, &lh);
      if (lw < tw || lh < th) {
        break;
      }
    }
    return (levelToGet - 1);
}

std::unique_ptr<SlideLevelDim> WsiToDcm::getSlideLevelDim(int32_t level,
                                            SlideLevelDim *priorLevel,
                                            SlideLevelDim * smallestSlideDim,
                                            bool enableProgressiveDownsample,
                                            OpenSlidePtr *osptr) {
  int32_t levelToGet = std::max(level, 0);
  int64_t downsample = 1;
  bool readOpenslide = false;
  std::string sourceDescription = "";
  if (level < 0) {
     // if -1 passed in load largest level using openslide.
     level = 0;
     enableProgressiveDownsample = false;
     priorLevel = nullptr;
  } else if (retile_) {
    if (wsiRequest_->downsamples.size() > level &&
        wsiRequest_->downsamples[level] >= 1) {
      downsample = wsiRequest_->downsamples[level];
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
  std::unique_ptr<SlideLevelDim> slideLevelDim;
  slideLevelDim = std::make_unique<SlideLevelDim>();
  double multiplicator;
  double downsampleOfLevel;
  int64_t levelWidth;
  int64_t levelHeight;
  bool generateUsingOpenSlide = true;
  bool readFromTiff = false;

  if ((tiffFile_ != nullptr && tiffFile_->isInitalized()) &&
      ((levelToGet == 0 &&
        wsiRequest_->SVSImportPreferScannerTileingForLargestLevel) ||
       wsiRequest_->SVSImportPreferScannerTileingForAllLevels)) {
    levelWidth = largestSlideLevelWidth_ / downsample;
    levelHeight = largestSlideLevelHeight_ / downsample;
    levelToGet = tiffFile_->getDirectoryIndexMatchingImageDimensions(
                                                    levelWidth, levelHeight);
    if (levelToGet != -1) {
      multiplicator = static_cast<double>(downsample);
      downsampleOfLevel = 1.0;
      generateUsingOpenSlide = false;
      readFromTiff = true;
      sourceDescription =
                    std::string("Image frame/tiles extracted without decompression from ") +
                    tiffFile_->path() + ", file level: " +
                    std::to_string(levelToGet) + ", and ";
    }
  }
  // ProgressiveDownsampling
  if (!readFromTiff && wsiRequest_->preferProgressiveDownsampling &&
                                                 enableProgressiveDownsample) {
    if (priorLevel != nullptr) {
      multiplicator = static_cast<double>(priorLevel->downsample);
      downsampleOfLevel = static_cast<double>(downsample) / multiplicator;
      // check that downsampling is going from higher to lower magnification
      if (downsampleOfLevel >= 1.0) {
        levelToGet = priorLevel->level;
        levelWidth = priorLevel->levelWidthDownsampled;
        levelHeight = priorLevel->levelHeightDownsampled;
        generateUsingOpenSlide = false;
        if (downsampleOfLevel > 1.0) {
          sourceDescription =
            std::string("Image frame/tiles generated by downsampling, ") +
            std::to_string(downsampleOfLevel) + " times, "
            "raw pixel values extracted from previous image level, "
            "level: " + std::to_string(levelToGet) + ", and ";
        } else {
          sourceDescription =
            std::string("Image frame/tiles generated from the "
            "raw pixel values extracted from previous image level, "
            "level: ") + std::to_string(levelToGet) + ", and ";
        }
      }
    }
  }
  // if no higherMagnifcationDicomFiles then downsample from openslide
  if (generateUsingOpenSlide) {
    if (osptr == nullptr) {
      slideLevelDim->osptr =
                        std::make_unique<OpenSlidePtr>(wsiRequest_->inputFile);
      osptr = slideLevelDim->osptr.get();
    }
    levelToGet = getOpenslideLevelForDownsample(osptr, downsample);

    multiplicator = openslide_get_level_downsample(osptr->osr, levelToGet);
    // Downsampling factor required to go from selected
    // downsampled level to the desired level of downsampling
    if (wsiRequest_->floorCorrectDownsampling) {
      multiplicator = floor(multiplicator);
    }
    downsampleOfLevel = static_cast<double>(downsample) / multiplicator;
    openslide_get_level_dimensions(osptr->osr, levelToGet, &levelWidth,
                                   &levelHeight);
    if (downsampleOfLevel > 1.0) {
       sourceDescription =
          std::string("Image frame/tiles generated by downsampling, ") +
          std::to_string(downsampleOfLevel) + " times, "
          "pixel values extracted via OpenSlide(file: " +
          wsiRequest_->inputFile + ", level: " +
          std::to_string(levelToGet) + ") and ";
    } else {
      sourceDescription =
        std::string("Image frame/tiles generated from "
        "pixel values extracted via OpenSlide(file: ") +
        wsiRequest_->inputFile + ", level: " +
        std::to_string(levelToGet) + ") and ";
    }
    readOpenslide = true;
  }
  // Adjust level size by starting position if skipping row and column.
  // levelHeightDownsampled and levelWidthDownsampled will reflect
  // new starting position.
  int64_t cropSourceLevelWidth = 0;
  int64_t cropSourceLevelHeight = 0;
  if (smallestSlideDim != nullptr) {
    const int64_t upsampleFactor = smallestSlideDim->downsample /
                                                                multiplicator;
    cropSourceLevelWidth = (levelWidth - initialX_) - (upsampleFactor *
                            smallestSlideDim->levelWidthDownsampled);
    cropSourceLevelHeight = (levelHeight - initialY_) - (upsampleFactor *
                             smallestSlideDim->levelHeightDownsampled);
  }
  int64_t frameWidthDownsampled;
  int64_t frameHeightDownsampled;
  int64_t levelWidthDownsampled;
  int64_t levelHeightDownsampled;
  int64_t levelFrameWidth;
  int64_t levelFrameHeight;
  DCM_Compression levelCompression;
  if (level <= 0) {
    levelCompression = wsiRequest_->firstlevelCompression;
  } else {
    levelCompression = wsiRequest_->compression;
  }
  dimensionDownsampling(wsiRequest_->frameSizeX, wsiRequest_->frameSizeY,
                        levelWidth - cropSourceLevelWidth - initialX_,
                        levelHeight - cropSourceLevelHeight - initialY_,
                        retile_, downsampleOfLevel,
                        &frameWidthDownsampled,
                        &frameHeightDownsampled,
                        &levelWidthDownsampled,
                        &levelHeightDownsampled,
                        &levelFrameWidth,
                        &levelFrameHeight,
                        &levelCompression);
  slideLevelDim->level = level;
  slideLevelDim->readFromTiff = readFromTiff;
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
  slideLevelDim->sourceDescription = std::move(sourceDescription);
  return (std::move(slideLevelDim));
}

double  WsiToDcm::getOpenSlideDimensionMM(OpenSlidePtr *osptr,
                                      const char* openSlideLevelDimProperty) {
  double firstLevelMpp = 0.0;
  const char *openslideFirstLevelMpp =
      openslide_get_property_value(osptr->osr, openSlideLevelDimProperty);
  if (openslideFirstLevelMpp != nullptr) {
    firstLevelMpp = std::stod(openslideFirstLevelMpp);
  }
  return firstLevelMpp;
}

double  WsiToDcm::getDimensionMM(const int64_t adjustedFirstLevelDim,
                                 const double firstLevelMpp) {
  return static_cast<double>(adjustedFirstLevelDim) * firstLevelMpp / 1000;
}

bool downsample_order(const std::tuple<int32_t, int64_t, bool> &i,
                      const std::tuple<int32_t, int64_t, bool> &j) {
  const int64_t iDownsample = std::get<1>(i);
  const int64_t jDownsample = std::get<1>(j);
  if (iDownsample != jDownsample) {
    // Sort on downsample: e.g. 1, 2, 4, 8, 16 (highest to lowest mag.)
    return iDownsample < jDownsample;
  }
  const int32_t iLevel = std::get<0>(i);
  const int32_t jLevel = std::get<0>(j);
  // Sort by level e.g., 1, 2, 3, 4
  return iLevel < jLevel;
}

std::unique_ptr<SlideLevelDim> WsiToDcm::getSmallestSlideDim(
                                  OpenSlidePtr *osptr,
                                  std::vector<int32_t> *slideLevels,
                                  std::vector<bool> *saveLevelCompressedRaw) {
  std::unique_ptr<SlideLevelDim> smallestSlideDim = nullptr;
  int32_t levels;
  if (retile_) {
    levels = wsiRequest_->retileLevels;
  } else {
    levels = svsLevelCount_;
  }
  bool smallestLevelIsSingleFrame = false;
  std::vector<std::tuple<int32_t, int64_t, bool>> levelProcessOrder;
  int64_t smallestDownsample;
  bool layerHasShownZeroLengthDimMsg = false;
  for (int32_t level = wsiRequest_->startOnLevel; level < levels &&
           (wsiRequest_->stopOnLevel < wsiRequest_->startOnLevel ||
                                 level <= wsiRequest_->stopOnLevel); level++) {
      std::unique_ptr<SlideLevelDim> tempSlideLevelDim =
    std::move(getSlideLevelDim(level, smallestSlideDim.get(), nullptr, osptr));
      if (tempSlideLevelDim->levelWidthDownsampled == 0 ||
          tempSlideLevelDim->levelHeightDownsampled == 0) {
        // frame is being downsampled to nothing skip file.
        if (!layerHasShownZeroLengthDimMsg) {
          layerHasShownZeroLengthDimMsg = true;
          BOOST_LOG_TRIVIAL(debug) << "Layer has a 0 length dimension."
                                      " Skipping dcm generation for layer.";
        }
        continue;
      }
      const int64_t frameX = std::ceil(
            static_cast<double>(tempSlideLevelDim->levelWidthDownsampled) /
            static_cast<double>(tempSlideLevelDim->levelFrameWidth));
      const int64_t frameY = std::ceil(
            static_cast<double>(tempSlideLevelDim->levelHeightDownsampled) /
            static_cast<double>(tempSlideLevelDim->levelFrameHeight));
      const int64_t frameCount = frameX * frameY;
      const int64_t tempDownsample = tempSlideLevelDim->downsample;
      const bool readSlideLevelFromTiff = tempSlideLevelDim->readFromTiff;
      BOOST_LOG_TRIVIAL(debug) << "Uncropped dimensions Level[" <<
                                  level << "]: " <<
                                  tempSlideLevelDim->levelWidthDownsampled <<
                                   ", " <<
                                   tempSlideLevelDim->levelHeightDownsampled;
      bool setSmallestSlice = false;
      //
      // tempDownsample and smallestDownsample are downsampling factors.
      // Smaller smaller numbers are result in "bigger images".
      // i.e. Image downsampled with smaller factor is dimensionally bigger
      //      than the same image downsampled with a larger factor.
      //
      if (smallestSlideDim == nullptr) {
        // if (smallest slice level not initalized)
        setSmallestSlice = true;
      } else if (tempDownsample > smallestDownsample &&
        (!wsiRequest_->stopDownsamplingAtSingleFrame ||
        !smallestLevelIsSingleFrame)) {
        // if dimensions are smaller than previous frame and
        // not stopDownsamplingAtSingleFrame or haven't seen single frame.
        setSmallestSlice = true;
      } else if (smallestLevelIsSingleFrame &&
        wsiRequest_->stopDownsamplingAtSingleFrame &&
        frameCount == 1 && tempDownsample < smallestDownsample) {
        // if smallest area is a single frame and stopDownsamplingAtSingleFrame
        // and temp level is also a single frame and temp level
        // is bigger than previous found smallest level.
        //
        // TLDR logic: smallestSlideDim == largest level that fits in a single
        // frame.
        setSmallestSlice = true;
      }
      if (setSmallestSlice) {
        smallestSlideDim = std::move(tempSlideLevelDim);
        smallestDownsample = tempDownsample;
        BOOST_LOG_TRIVIAL(debug) << "Set Smallest";
      }
      if (tempDownsample <= smallestDownsample) {
        std::tuple<int32_t, int64_t, bool> tpl(level, tempDownsample,
                                               readSlideLevelFromTiff);
        levelProcessOrder.push_back(tpl);
        BOOST_LOG_TRIVIAL(debug) << "Level[" <<level << "] frames:" <<
                                    frameX << ", " << frameY;
      }
      if (wsiRequest_->stopDownsamplingAtSingleFrame && frameCount <= 1) {
        smallestLevelIsSingleFrame = true;
        if (!customDownSampleFactorsDefined_) {
          // If not generating downsamples from a user supplied list of
          // downsamples. testing additional levels is no longer necessary.
          BOOST_LOG_TRIVIAL(debug) << "stop searching for smallest frame";
          break;
        }
      }
    }
    // Process process levels in order of area largest to smallest.
    if (smallestSlideDim != nullptr) {
      std::sort(levelProcessOrder.begin(),
                levelProcessOrder.end(), downsample_order);

      for (size_t idx = 0; idx < levelProcessOrder.size(); ++idx) {
        const int32_t level = std::get<0>(levelProcessOrder[idx]);
        if (wsiRequest_->cropFrameToGenerateUniformPixelSpacing) {
          // Frame cropping requires that images are downsampled
          // by a constant factor. Verify that here.
          const int64_t downsample = std::get<1>(levelProcessOrder[idx]);
          for (size_t pre_index = 0; pre_index < idx; ++pre_index) {
            const int64_t prior_downsample =
                                    std::get<1>(levelProcessOrder[pre_index]);
            if (downsample %  prior_downsample != 0) {
               const int32_t prior_level =
                                    std::get<0>(levelProcessOrder[pre_index]);
               BOOST_LOG_TRIVIAL(error) << "Crop images to achieve uniform"
                                           " pixel spacing images requires "
                                           "that images must be downsampled "
                                           "by a consistent factor (e.g., 1,"
                                           " 2, 4, 8, ../)\n\nLevel[" <<
                                           pre_index << "] downsample: " <<
                                           prior_downsample <<
                                           " and Level[" << level <<
                                           "] downsample: " << downsample <<
                                            " are not multiples.";
                throw 1;
            }
          }
        }
        slideLevels->push_back(level);
        if (level == smallestSlideDim->level) {
          // if last slice do not save raw
          saveLevelCompressedRaw->push_back(false);
          break;
        }
        if (std::get<2>(levelProcessOrder[idx+1])) {
          // memory & comput optimization
          // if next slice reads from tiff do save raw
          saveLevelCompressedRaw->push_back(false);
        } else if ((slideLevels->size() == 1) &&
                   !std::get<2>(levelProcessOrder[idx]) &&
                   (0 == getOpenslideLevelForDownsample(osptr,
                    std::get<1>(levelProcessOrder[idx+1])))) {
          // Memory optimization, if processing an image without downsampling &
          // openslide is being used to read the imaging
          // (!std::get<2>(levelProcessOrder[idx])
          // no downsamples and reading downsample at level will also read the
          // highest resolution image.  Do not save compressed raw versions of
          // the highest resolution.  Start progressive downsampling from
          // downsample level 2 and on.
          saveLevelCompressedRaw->push_back(false);
        } else {
          // Otherwise save compressed slice if progressive downsampling is
          // enabled.
          const bool progDS = wsiRequest_->preferProgressiveDownsampling;
          saveLevelCompressedRaw->push_back(progDS);
        }
      }
    }
    return (std::move(smallestSlideDim));
  }

int WsiToDcm::dicomizeTiff() {
  std::unique_ptr<DcmTags> tags = std::make_unique<DcmTags>();
  if (wsiRequest_->jsonFile.size() > 0) {
    tags->readJsonFile(wsiRequest_->jsonFile);
  }

  int8_t threadsForPool = boost::thread::hardware_concurrency();
  if (wsiRequest_->threads > 0) {
    threadsForPool = std::min(wsiRequest_->threads, threadsForPool);
  }

  // Initalize openslide
  std::unique_ptr<OpenSlidePtr> osptr = std::move(initOpenslide());
  // Determine smallest_slide downsample dimensions to enable
  // slide pixel spacing normalization croping to ensure pixel
  // spacing across all downsample images is uniform.
  int64_t largestSlideWidthCrop;
  int64_t largestSlideHeightCrop;
  std::vector<int32_t> slideLevels;
  std::vector<int32_t> saveLevelRaw;
  std::vector<bool> saveLevelCompressedRaw;
  std::unique_ptr<SlideLevelDim> smallestSlideDim = std::move(
                                          getSmallestSlideDim(osptr.get(),
                                                              &slideLevels,
                                                    &saveLevelCompressedRaw));
  if (wsiRequest_->cropFrameToGenerateUniformPixelSpacing) {
    // passing -1 in forces getSlideLevelDim to return non-downsampled
    // image dimensions a.k.a. largest image from openslide.
    std::unique_ptr<SlideLevelDim> largestDimensions =
              std::move(getSlideLevelDim(-1, nullptr, smallestSlideDim.get(),
                                         osptr->osr));
    largestSlideWidthCrop = largestDimensions->cropSourceLevelWidth;
    largestSlideHeightCrop = largestDimensions->cropSourceLevelHeight;
  } else {
    largestSlideWidthCrop = 0;
    largestSlideHeightCrop = 0;
    smallestSlideDim = nullptr;
  }
  osptr = nullptr;

  BOOST_LOG_TRIVIAL(debug) << "Cropping source image: " <<
                              largestSlideWidthCrop <<
                              ", " <<
                              largestSlideHeightCrop;

  DICOMFileFrameRegionReader higherMagnifcationDicomFiles;
  std::vector<std::unique_ptr<DcmFileDraft>> generatedDicomFiles;
  std::unique_ptr<SlideLevelDim> slideLevelDim = nullptr;
  const double levelWidthMM = getDimensionMM(
                          largestSlideLevelWidth_ - largestSlideWidthCrop -
                          initialX_, openslideMPP_X_);
  const double levelHeightMM = getDimensionMM(
                        largestSlideLevelHeight_ - largestSlideHeightCrop -
                        initialY_, openslideMPP_Y_);

  for (size_t levelIndex = 0; levelIndex < slideLevels.size(); ++levelIndex) {
    const int32_t level = slideLevels[levelIndex];
    slideLevelDim = std::move(getSlideLevelDim(level,
                                               slideLevelDim.get(),
                                               smallestSlideDim.get(),
                        higherMagnifcationDicomFiles.dicomFileCount() > 0));

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
    const std::string imageSourceDescription = slideLevelDim->sourceDescription;

    DCM_Compression levelCompression = slideLevelDim->levelCompression;
    if (levelWidthDownsampled == 0 || levelHeightDownsampled == 0) {
      // frame is being downsampled to nothing skip image generation.
      BOOST_LOG_TRIVIAL(debug) << "Layer has a 0 length dimension. Skipping "
                                  "dcm generation for layer.";
      break;
    }
    BOOST_LOG_TRIVIAL(debug) << " ";
    BOOST_LOG_TRIVIAL(debug) << "Starting Level " << level;
    BOOST_LOG_TRIVIAL(debug) << "level size: " << levelWidth << ' '
                             << levelHeight << ' ' << multiplicator;
    BOOST_LOG_TRIVIAL(debug) << "multiplicator: " << multiplicator;
    BOOST_LOG_TRIVIAL(debug) << "levelToGet: " << levelToGet;
    BOOST_LOG_TRIVIAL(debug) << "downsample: " << downsample;
    BOOST_LOG_TRIVIAL(debug) << "downsampleOfLevel: " << downsampleOfLevel;
    BOOST_LOG_TRIVIAL(debug) << "frameDownsampled: " << frameWidthDownsampled
                             << ", " << frameHeightDownsampled;
    BOOST_LOG_TRIVIAL(debug) << "Croping source image: " <<
                             cropSourceLevelWidth << ", " <<
                             cropSourceLevelHeight;
    const int frameX = std::ceil(static_cast<double>(levelWidthDownsampled) /
                                 static_cast<double>(levelFrameWidth));
    const int frameY = std::ceil(static_cast<double>(levelHeightDownsampled) /
                                 static_cast<double>(levelFrameHeight));
    const bool saveCompressedRaw = saveLevelCompressedRaw[levelIndex];

    if (slideLevelDim->readOpenslide || slideLevelDim->readFromTiff) {
      // If slide level was initalized from openslide or tiff
      // clear higherMagnifcationDicomFiles so
      // level is downsampled from openslide and not
      // prior level if progressiveDownsample is enabled.
      higherMagnifcationDicomFiles.clearDicomFiles();
    }
    if (slideLevelDim->readFromTiff) {
      levelCompression = JPEG;
    }
    BOOST_LOG_TRIVIAL(debug) << "higherMagnifcationDicomFiles " <<
                          higherMagnifcationDicomFiles.dicomFileCount();
    int64_t y = initialY_;
    std::vector<std::unique_ptr<Frame>> framesInitalizationData;
    // Preallocate vector space for frames
    framesInitalizationData.reserve(frameX * frameY);
    //  Walk through all frames in selected best layer. Extract frames from
    //  layer FrameDim = (frameWidthDownsampled, frameHeightDownsampled)
    //  which are dim of frame scaled up to the dimension of the layer being
    //  sampled from
    //
    //  Frame objects are processed via a thread pool.
    //  Method in Frame::sliceFrame () downsamples the imaging.
    //
    //  DcmFileDraft Joins threads and combines results and writes dcm file.

    std::unique_ptr<TiffFile> tiffFrameFilePtr = nullptr;
    if (slideLevelDim->readFromTiff) {
      tiffFrameFilePtr = std::make_unique<TiffFile>(*tiffFile_.get(),
                                                    levelToGet);
    }
    while (y < levelHeight - cropSourceLevelHeight) {
      int64_t x = initialX_;
      while (x < levelWidth - cropSourceLevelWidth) {
        std::unique_ptr<Frame> frameData;
        if (slideLevelDim->readFromTiff) {
          frameData = std::make_unique<TiffFrame>(tiffFrameFilePtr.get(),
              frameIndexFromLocation(tiffFrameFilePtr.get(), levelToGet, x, y),
              saveCompressedRaw);
        } else if (wsiRequest_->useOpenCVDownsampling) {
          frameData = std::make_unique<OpenCVInterpolationFrame>(
              slideLevelDim->osptr.get(), x, y, levelToGet,
              frameWidthDownsampled, frameHeightDownsampled, levelFrameWidth,
              levelFrameHeight, levelCompression, wsiRequest_->quality,
              levelWidth, levelHeight, largestSlideLevelWidth_,
              largestSlideLevelHeight_, saveCompressedRaw,
              &higherMagnifcationDicomFiles,
              wsiRequest_->openCVInterpolationMethod);
        } else {
          frameData = std::make_unique<NearestNeighborFrame>(
              slideLevelDim->osptr.get(), x, y, levelToGet,
              frameWidthDownsampled, frameHeightDownsampled,
              multiplicator, levelFrameWidth, levelFrameHeight,
              levelCompression, wsiRequest_->quality,
              saveCompressedRaw, &higherMagnifcationDicomFiles);
        }
        if (higherMagnifcationDicomFiles.dicomFileCount() != 0) {
          frameData->incSourceFrameReadCounter();
        }
        framesInitalizationData.push_back(std::move(frameData));
        x += frameWidthDownsampled;
      }
      y += frameHeightDownsampled;
    }
    BOOST_LOG_TRIVIAL(debug) << "Level Frame Count: " <<
                          framesInitalizationData.size();

    boost::asio::thread_pool pool(threadsForPool);
    uint32_t row = 1;
    uint32_t column = 1;
    std::vector<std::unique_ptr<Frame>> framesData;
    if (wsiRequest_->batchLimit == 0) {
      framesData.reserve(frameX * frameY);
    } else {
      framesData.reserve(std::min(frameX * frameY, wsiRequest_->batchLimit));
    }

    const size_t total_frame_count = framesInitalizationData.size();
    for (std::vector<std::unique_ptr<Frame>>::iterator frameData =
                                             framesInitalizationData.begin();
                    frameData != framesInitalizationData.end(); ++frameData) {
      const int64_t  frameXPos = (*frameData)->locationX();
      const int64_t  frameYPos = (*frameData)->locationY();
      boost::asio::post(
          pool, [frameData = frameData->get()]() { frameData->sliceFrame(); });
      framesData.push_back(std::move(*frameData));
      if (wsiRequest_->batchLimit > 0 &&
          framesData.size() >= wsiRequest_->batchLimit) {
        std::unique_ptr<DcmFileDraft> filedraft =
            std::make_unique<DcmFileDraft>(
                std::move(framesData), wsiRequest_->outputFileMask,
                levelWidthDownsampled, levelHeightDownsampled, level, row,
                column, wsiRequest_->studyId, wsiRequest_->seriesId,
                wsiRequest_->imageName, levelCompression, wsiRequest_->tiled,
                tags.get(), levelWidthMM, levelHeightMM, downsample,
                &generatedDicomFiles, imageSourceDescription);
        boost::asio::post(pool, [th_filedraft = filedraft.get()]() {
          th_filedraft->saveFile();
        });
        generatedDicomFiles.push_back(std::move(filedraft));
        row = static_cast<uint32_t>((frameYPos +
                                     frameHeightDownsampled + 1) /
                        (frameHeightDownsampled - 1));
        column = static_cast<uint32_t>((frameXPos +
                                        frameWidthDownsampled + 1) /
                          (frameWidthDownsampled - 1));
        }
    }
    if (framesData.size() > 0) {
      std::unique_ptr<DcmFileDraft> filedraft = std::make_unique<DcmFileDraft>(
          std::move(framesData), wsiRequest_->outputFileMask,
          levelWidthDownsampled, levelHeightDownsampled, level, row, column,
          wsiRequest_->studyId, wsiRequest_->seriesId, wsiRequest_->imageName,
          levelCompression, wsiRequest_->tiled, tags.get(), levelWidthMM,
          levelHeightMM, downsample, &generatedDicomFiles,
          imageSourceDescription);
      boost::asio::post(pool, [th_filedraft = filedraft.get()]() {
        th_filedraft->saveFile();
      });
      generatedDicomFiles.push_back(std::move(filedraft));
    }
    pool.join();
    if  (!saveCompressedRaw) {
      generatedDicomFiles.clear();
    }
    higherMagnifcationDicomFiles.setDicomFiles(std::move(generatedDicomFiles),
                                               std::move(tiffFrameFilePtr));
    // The combination of cropFrameToGenerateUniformPixelSpacing &
    // stopDownsamplingAtSingleFrame can result in cropping images
    // which would otherwise span multiple frames to one frame.
    // Check if stopDownsamplingAtSingleFrame is enabled
    // and if the downsampled image was written in one frame.
    if (wsiRequest_->stopDownsamplingAtSingleFrame && total_frame_count <= 1) {
      break;
    }
  }
  BOOST_LOG_TRIVIAL(info) << "dicomization is done";
  return 0;
}

int WsiToDcm::wsi2dcm() {
  try {
    checkArguments();
    return dicomizeTiff();
  } catch (int exception) {
    return 1;
  }
}

}  // namespace wsiToDicomConverter
