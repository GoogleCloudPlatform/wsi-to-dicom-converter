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
#include <utility>
#include <vector>

#include "src/abstractDcmFile.h"
#include "src/dcmFileDraft.h"
#include "src/dcmFilePyramidSource.h"
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
  if (!wsiRequest_->genPyramidFromDicom &&
      !wsiRequest_->genPyramidFromUntiledImage) {
    const char *slideFile = wsiRequest_->inputFile.c_str();
    if (!openslide_detect_vendor(slideFile)) {
      BOOST_LOG_TRIVIAL(error) << "File format is not supported by openslide";
      throw 1;
    }
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

std::unique_ptr<DcmFilePyramidSource> WsiToDcm::initDicomIngest() {
  std::unique_ptr<DcmFilePyramidSource> dicomFile =
                std::make_unique<DcmFilePyramidSource>(wsiRequest_->inputFile);
  svsLevelCount_ = 1;
  largestSlideLevelWidth_ = dicomFile->imageWidth();
  largestSlideLevelHeight_ = dicomFile->imageHeight();
  wsiRequest_->frameSizeX = dicomFile->frameWidth();
  wsiRequest_->frameSizeY = dicomFile->frameHeight();
  if (wsiRequest_->studyId.size() < 1) {
    wsiRequest_->studyId = dicomFile->studyInstanceUID();
  }
  if (wsiRequest_->seriesId.size() < 1) {
    wsiRequest_->seriesId = dicomFile->seriesInstanceUID();
  }
  if (wsiRequest_->imageName.size() < 1) {
    wsiRequest_->imageName = dicomFile->seriesDescription();
  }
  return std::move(dicomFile);
}

std::unique_ptr<ImageFilePyramidSource> WsiToDcm::initUntiledImageIngest() {
  std::unique_ptr<ImageFilePyramidSource> dicomFile =
                                      std::make_unique<ImageFilePyramidSource>(
                                            wsiRequest_->inputFile,
                                            wsiRequest_->frameSizeX,
                                            wsiRequest_->frameSizeY,
                                            wsiRequest_->untiledImageHeightMM);
  svsLevelCount_ = 1;
  largestSlideLevelWidth_ = dicomFile->imageWidth();
  largestSlideLevelHeight_ = dicomFile->imageHeight();
  return std::move(dicomFile);
}

openslide_t* WsiToDcm::getOpenSlidePtr() {
  if (osptr_ == nullptr) {
    osptr_ = std::make_unique<OpenSlidePtr>(wsiRequest_->inputFile);
  }
  return osptr_->osr();
}

void  WsiToDcm::clearOpenSlidePtr() {
  osptr_ = nullptr;
}

void WsiToDcm::initOpenSlide() {
  svsLevelCount_ = openslide_get_level_count(getOpenSlidePtr());
  // Openslide API call 0 returns dimensions of highest resolution image.
  openslide_get_level_dimensions(getOpenSlidePtr(), 0,
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
            if (!tiffFrame.canDecodeJpeg()) {
              BOOST_LOG_TRIVIAL(error) << "Error svs contains decoding "
                                          "JPEG in SVS.";
              throw 1;
            } else {
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
  BOOST_LOG_TRIVIAL(debug) << " ";
  BOOST_LOG_TRIVIAL(debug) << "Level Count: " << svsLevelCount_;
}

int32_t WsiToDcm::getOpenslideLevelForDownsample(int64_t downsample) {
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
      openslide_get_level_dimensions(getOpenSlidePtr(), levelToGet, &lw, &lh);
      if (lw < tw || lh < th) {
        break;
      }
    }
    return (levelToGet - 1);
}

std::unique_ptr<SlideLevelDim> WsiToDcm::initAbstractDicomFileSourceLevelDim(
                                              absl::string_view description) {
  std::unique_ptr<SlideLevelDim> slideLevelDim;
  slideLevelDim = std::make_unique<SlideLevelDim>();
  slideLevelDim->level = 0;
  slideLevelDim->levelToGet = 0;
  slideLevelDim->multiplicator = 1;
  slideLevelDim->downsample = 1;
  slideLevelDim->downsampleOfLevel = 1;
  slideLevelDim->frameWidthDownsampled = std::min(wsiRequest_->frameSizeX,
                                                  largestSlideLevelWidth_);
  slideLevelDim->frameHeightDownsampled = std::min(wsiRequest_->frameSizeY,
                                                   largestSlideLevelHeight_);
  slideLevelDim->levelWidth = largestSlideLevelWidth_;
  slideLevelDim->levelHeight = largestSlideLevelHeight_;
  slideLevelDim->levelWidthDownsampled = largestSlideLevelWidth_;
  slideLevelDim->levelHeightDownsampled = largestSlideLevelHeight_;
  slideLevelDim->sourceDerivationDescription =
                              std::move(static_cast<std::string>(description));
  slideLevelDim->useSourceDerivationDescriptionForDerivedImage = true;
  slideLevelDim->readFromTiff = false;
  slideLevelDim->readOpenslide = false;
  slideLevelDim->levelCompression = UNKNOWN;
  return std::move(slideLevelDim);
}

std::unique_ptr<SlideLevelDim> WsiToDcm::getSlideLevelDim(int32_t level,
                                            SlideLevelDim *priorLevel) {
  int32_t levelToGet = std::max(level, 0);
  int64_t downsample = 1;
  bool readOpenslide = false;
  std::string sourceDerivationDescription = "";
  if (retile_) {
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
  bool generateFromPrimarySource = true;
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
      generateFromPrimarySource = false;
      readFromTiff = true;
      // Source component of DCM_DerivationDescription
      // describes in text where imaging data was acquired from.
      sourceDerivationDescription =
                    std::string("Image frame/tiles extracted without "
                    "decompression from ") + tiffFile_->path() +
                    ", file level: " + std::to_string(levelToGet) + ", and ";
    }
  }
  // ProgressiveDownsampling
  if (!readFromTiff && wsiRequest_->preferProgressiveDownsampling &&
      priorLevel != nullptr) {
    multiplicator = static_cast<double>(priorLevel->downsample);
    downsampleOfLevel = static_cast<double>(downsample) / multiplicator;
    // check that downsampling is going from higher to lower magnification
    if (downsampleOfLevel >= 1.0) {
      levelToGet = priorLevel->level;
      levelWidth = priorLevel->levelWidthDownsampled;
      levelHeight = priorLevel->levelHeightDownsampled;
      generateFromPrimarySource = false;
      // Source component of DCM_DerivationDescription
      // describes in text where imaging data was acquired from.
      if (priorLevel->useSourceDerivationDescriptionForDerivedImage) {
        sourceDerivationDescription = priorLevel->sourceDerivationDescription;
      } else if (downsampleOfLevel > 1.0) {
        sourceDerivationDescription =
          std::string("Image frame/tiles generated by downsampling, ") +
          std::to_string(downsampleOfLevel) + " times, "
          "raw pixel values extracted from previous image level, "
          "level: " + std::to_string(levelToGet) + ", and ";
      } else {
        sourceDerivationDescription =
          std::string("Image frame/tiles generated from the "
          "raw pixel values extracted from previous image level, "
          "level: ") + std::to_string(levelToGet) + ", and ";
      }
    }
  }
  if (generateFromPrimarySource) {
    // if no higherMagnifcationDicomFiles then downsample from openslide
    levelToGet = getOpenslideLevelForDownsample(downsample);
    multiplicator = openslide_get_level_downsample(getOpenSlidePtr(),
                                                   levelToGet);
    // Downsampling factor required to go from selected
    // downsampled level to the desired level of downsampling
    if (wsiRequest_->floorCorrectDownsampling) {
      multiplicator = floor(multiplicator);
    }
    downsampleOfLevel = static_cast<double>(downsample) / multiplicator;
    openslide_get_level_dimensions(getOpenSlidePtr(), levelToGet, &levelWidth,
                                   &levelHeight);
    // Source component of DCM_DerivationDescription
    // describes in text where imaging data was acquired from.
    if (downsampleOfLevel > 1.0) {
       sourceDerivationDescription =
          std::string("Image frame/tiles generated by downsampling, ") +
          std::to_string(downsampleOfLevel) + " times, "
          "pixel values extracted via OpenSlide(file: " +
          wsiRequest_->inputFile + ", level: " +
          std::to_string(levelToGet) + ") and ";
    } else {
      sourceDerivationDescription =
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
                        levelWidth - initialX_,
                        levelHeight - initialY_,
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
  slideLevelDim->levelCompression = levelCompression;
  slideLevelDim->readOpenslide = readOpenslide;
  slideLevelDim->sourceDerivationDescription =
                                  std::move(sourceDerivationDescription);
  slideLevelDim->useSourceDerivationDescriptionForDerivedImage = false;
  return (std::move(slideLevelDim));
}

double  WsiToDcm::getOpenSlideDimensionMM(const char* openSlideProperty) {
  double firstLevelMpp = 0.0;
  const char *openslideFirstLevelMpp =
      openslide_get_property_value(getOpenSlidePtr(), openSlideProperty);
  if (openslideFirstLevelMpp != nullptr) {
    firstLevelMpp = std::stod(openslideFirstLevelMpp);
  }
  return firstLevelMpp;
}

double  WsiToDcm::getDimensionMM(const int64_t adjustedFirstLevelDim,
                                 const double firstLevelMpp) {
  return static_cast<double>(adjustedFirstLevelDim) * firstLevelMpp / 1000;
}

struct LevelProcessOrder {
 public:
  LevelProcessOrder(int32_t level, int64_t downsample, bool readLevelFromTiff);
  int32_t level() const;
  int64_t downsample() const;
  bool readLevelFromTiff() const;

 private:
  const int32_t level_;
  const int64_t downsample_;
  const bool readLevelFromTiff_;
};

LevelProcessOrder::LevelProcessOrder(int32_t level, int64_t downsample,
                                     bool readLevelFromTiff) :
                                     level_(level), downsample_(downsample),
                                     readLevelFromTiff_(readLevelFromTiff) {
}

int32_t LevelProcessOrder::level() const {
  return level_;
}

int64_t LevelProcessOrder::downsample() const {
  return downsample_;
}

bool LevelProcessOrder::readLevelFromTiff() const {
  return readLevelFromTiff_;
}

bool downsample_order(const std::unique_ptr<LevelProcessOrder> &i,
                      const std::unique_ptr<LevelProcessOrder> &j) {
  const int64_t iDownsample = i->downsample();
  const int64_t jDownsample = j->downsample();
  if (iDownsample != jDownsample) {
    // Sort on downsample: e.g. 1, 2, 4, 8, 16 (highest to lowest mag.)
    return iDownsample < jDownsample;
  }
  // Sort by level e.g., 1, 2, 3, 4
  return i->level() < j->level();
}

void WsiToDcm::getOptimalDownSamplingOrder(
                                  std::vector<int32_t> *slideLevels,
                                  std::vector<bool> *saveLevelCompressedRaw,
                                  SlideLevelDim *startPyramidCreationDim) {
  int32_t levels;
  if (retile_) {
    levels = wsiRequest_->retileLevels;
  } else {
    levels = svsLevelCount_;
  }
  std::unique_ptr<SlideLevelDim> smallestSlideDim = nullptr;
  bool smallestLevelIsSingleFrame = false;
  std::vector<std::unique_ptr<LevelProcessOrder>> levelOrderVec;
  int64_t smallestDownsample;
  bool layerHasShownZeroLengthDimMsg = false;
  SlideLevelDim *priorSlideLevelDim;
  if (startPyramidCreationDim == nullptr) {
    priorSlideLevelDim = nullptr;
  } else {
    priorSlideLevelDim = startPyramidCreationDim;
  }
  for (int32_t level = wsiRequest_->startOnLevel; level < levels &&
           (wsiRequest_->stopOnLevel < wsiRequest_->startOnLevel ||
                                 level <= wsiRequest_->stopOnLevel); level++) {
    BOOST_LOG_TRIVIAL(debug) << "Level: " << level;
    std::unique_ptr<SlideLevelDim> tempSlideLevelDim =
        std::move(getSlideLevelDim(level, priorSlideLevelDim));
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
    BOOST_LOG_TRIVIAL(debug) << "Dimensions Level[" <<
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
      priorSlideLevelDim = smallestSlideDim.get();
      BOOST_LOG_TRIVIAL(debug) << "Set Smallest";
    }
    if (tempDownsample <= smallestDownsample) {
      levelOrderVec.push_back(std::make_unique<LevelProcessOrder>(level,
                                                tempDownsample,
                                                readSlideLevelFromTiff));
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
    std::sort(levelOrderVec.begin(),
              levelOrderVec.end(), downsample_order);
     for (size_t idx = 0; idx < levelOrderVec.size(); ++idx) {
      const int32_t level = levelOrderVec[idx]->level();
      slideLevels->push_back(level);
      if (level == smallestSlideDim->level) {
        // if last slice do not save raw
        saveLevelCompressedRaw->push_back(false);
        break;
      }
      if (levelOrderVec[idx+1]->readLevelFromTiff()) {
        // memory & comput optimization
        // if next slice reads from tiff do save raw
        saveLevelCompressedRaw->push_back(false);
      } else if ((startPyramidCreationDim == nullptr) &&
                 (slideLevels->size() == 1) &&
                 !levelOrderVec[idx]->readLevelFromTiff() &&
                 (0 == getOpenslideLevelForDownsample(
                  levelOrderVec[idx+1]->downsample()))) {
        // Memory optimization, not reading from dicom.
        // if processing an image without downsampling &
        // openslide is being used to read the imaging
        // (!levelOrderVec[idx]->readLevelFromTiff()
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
}

double abstractDicomDimensionMM(double imageDimMM, uint64_t imageDim,
                                int64_t imageDimOffset) {
  if (imageDimOffset <= 0) {
    return imageDimMM;
  }
  return (static_cast<double>(imageDim - imageDimOffset) * imageDimMM) /
         static_cast<double>(imageDim);
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
  std::unique_ptr<SlideLevelDim> slideLevelDim = nullptr;
  std::unique_ptr<AbstractDcmFile> abstractDicomFile = nullptr;
  double levelWidthMM, levelHeightMM;
  if (wsiRequest_->genPyramidFromUntiledImage) {
    std::string description = "Image frames generated from "
      " values extracted from un-tiled image (" +
      wsiRequest_->inputFile + ") and ";
    abstractDicomFile = std::move(initUntiledImageIngest());
    slideLevelDim = std::move(initAbstractDicomFileSourceLevelDim(
                                                         description.c_str()));
  } else if (wsiRequest_->genPyramidFromDicom) {
    std::string description = "Image frames generated from "
      " values extracted from DICOM(" +
      wsiRequest_->inputFile + ") and ";
    abstractDicomFile = std::move(initDicomIngest());
    slideLevelDim = std::move(initAbstractDicomFileSourceLevelDim(
                                                         description.c_str()));
  }
  if (largestSlideLevelWidth_ <= initialX_ ||
      largestSlideLevelHeight_ <= initialY_) {
    BOOST_LOG_TRIVIAL(error) << "Input image dimensions are to small.";
    return 1;
  }
  if (abstractDicomFile != nullptr) {
    // Initalize height and width dimensions directly from file measures
    levelWidthMM = abstractDicomDimensionMM(abstractDicomFile->imageWidthMM(),
                                            largestSlideLevelWidth_,
                                            initialX_);
    levelHeightMM = abstractDicomDimensionMM(abstractDicomFile->imageHeightMM(),
                                            largestSlideLevelHeight_,
                                            initialY_);
  } else {
    // Initalize openslide
    initOpenSlide();
    double openslideMPP_X = getOpenSlideDimensionMM("openslide.mpp-x");
    double openslideMPP_Y = getOpenSlideDimensionMM("openslide.mpp-y");
    levelWidthMM = getDimensionMM(largestSlideLevelWidth_ - initialX_,
                                  openslideMPP_X);
    levelHeightMM = getDimensionMM(largestSlideLevelHeight_ - initialY_,
                                   openslideMPP_Y);
  }
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
  // Determine smallest_slide downsample dimensions to enable
  // slide pixel spacing normalization croping to ensure pixel
  // spacing across all downsample images is uniform.
  std::vector<int32_t> slideLevels;
  std::vector<bool> saveLevelCompressedRaw;
  getOptimalDownSamplingOrder(&slideLevels,
                              &saveLevelCompressedRaw,
                              slideLevelDim.get());
  DICOMFileFrameRegionReader higherMagnifcationDicomFiles;
  std::vector<std::unique_ptr<AbstractDcmFile>> generatedDicomFiles;
  if (abstractDicomFile != nullptr) {
    generatedDicomFiles.push_back(std::move(abstractDicomFile));
    higherMagnifcationDicomFiles.setDicomFiles(std::move(generatedDicomFiles),
                                               nullptr);
  }
  clearOpenSlidePtr();

  for (size_t levelIndex = 0; levelIndex < slideLevels.size(); ++levelIndex) {
    const int32_t level = slideLevels[levelIndex];
    SlideLevelDim *priorSlideLevel;
    if (higherMagnifcationDicomFiles.dicomFileCount() > 0) {
      priorSlideLevel = slideLevelDim.get();
    } else {
      priorSlideLevel = nullptr;
    }
    slideLevelDim = std::move(getSlideLevelDim(level, priorSlideLevel));
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
    const std::string sourceDerivationDescription =
                                    slideLevelDim->sourceDerivationDescription;

    DCM_Compression levelCompression = slideLevelDim->levelCompression;
    if (levelWidthDownsampled == 0 || levelHeightDownsampled == 0) {
      // frame is being downsampled to nothing skip image generation.
      BOOST_LOG_TRIVIAL(debug) << "Layer has a 0 length dimension. Skipping "
                                  "dcm generation for layer.";
      break;
    }
    BOOST_LOG_TRIVIAL(debug) << "Starting Level " << level << "\n"
                             "level size: " << levelWidth << ", " <<
                             levelHeight << "\n"
                             "multiplicator: " << multiplicator << "\n"
                             "levelToGet: " << levelToGet << "\n"
                             "downsample: " << downsample << "\n"
                             "downsampleOfLevel: " << downsampleOfLevel << "\n"
                             "frameDownsampled: " << frameWidthDownsampled <<
                             ", " << frameHeightDownsampled;
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
    while (y < levelHeight) {
      int64_t x = initialX_;
      while (x < levelWidth) {
        std::unique_ptr<Frame> frameData;
        if (slideLevelDim->readFromTiff) {
          frameData = std::make_unique<TiffFrame>(tiffFrameFilePtr.get(),
              frameIndexFromLocation(tiffFrameFilePtr.get(), levelToGet, x, y),
              saveCompressedRaw);
        } else if (wsiRequest_->useOpenCVDownsampling) {
          frameData = std::make_unique<OpenCVInterpolationFrame>(
              osptr_.get(), x, y, levelToGet,
              frameWidthDownsampled, frameHeightDownsampled, levelFrameWidth,
              levelFrameHeight, levelCompression, wsiRequest_->quality,
              levelWidth, levelHeight, largestSlideLevelWidth_,
              largestSlideLevelHeight_, saveCompressedRaw,
              &higherMagnifcationDicomFiles,
              wsiRequest_->openCVInterpolationMethod);
        } else {
          frameData = std::make_unique<NearestNeighborFrame>(
              osptr_.get(), x, y, levelToGet,
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
                &generatedDicomFiles, sourceDerivationDescription);
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
          sourceDerivationDescription);
      boost::asio::post(pool, [th_filedraft = filedraft.get()]() {
        th_filedraft->saveFile();
      });
      generatedDicomFiles.push_back(std::move(filedraft));
    }
    pool.join();
    clearOpenSlidePtr();
    if  (!saveCompressedRaw) {
      generatedDicomFiles.clear();
    }
    higherMagnifcationDicomFiles.setDicomFiles(std::move(generatedDicomFiles),
                                               std::move(tiffFrameFilePtr));
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
