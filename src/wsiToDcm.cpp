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
#include <math.h>

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
  if (!wsiRequest_->genPyramidFromUntiledImage) {
    const char *slideFile = wsiRequest_->inputFile.c_str();
    if (!openslide_detect_vendor(slideFile)) {
      BOOST_LOG_TRIVIAL(error) << "File format is not supported by openslide";
      throw 1;
    }
  }
  BOOST_LOG_TRIVIAL(info) << "dicomization is started";
  largestSlideLevelWidth_ = 0;
  largestSlideLevelHeight_ = 0;
  svsLevelCount_ = 0;
  initialX_ = 0;
  initialY_ = 0;
  if (wsiRequest_->dropFirstRowAndColumn) {
    initialX_ = 1;
    initialY_ = 1;
  }
  const size_t downsample_count = wsiRequest_->downsamples.size();
  if (downsample_count > 0) {
      for (size_t idx=1; idx < downsample_count; ++idx) {
         if (wsiRequest_->downsamples[idx] < wsiRequest_->downsamples[idx-1]) {
            BOOST_LOG_TRIVIAL(warning) << "Downsample levels not defined in "
                                          "increasing  sorted order. "
                                          "[e.g., 1, 2, 8]";
            std::sort(wsiRequest_->downsamples.begin(),
                      wsiRequest_->downsamples.end());
            break;
         }
      }
      wsiRequest_->retileLevels = 0;
  }
  retile_ = (wsiRequest_->retileLevels > 0) || (downsample_count > 0);
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

std::unique_ptr<DcmFilePyramidSource> WsiToDcm::initDicomIngest(
                                 bool load_frame_data_from_dicom_using_dcmtk) {
  std::unique_ptr<DcmFilePyramidSource> dicomFile =
                std::make_unique<DcmFilePyramidSource>(wsiRequest_->inputFile,
                                       load_frame_data_from_dicom_using_dcmtk);
  if (!dicomFile->isValid()) {
    throw 1;
  }
  svsLevelCount_ = 1;
  largestSlideLevelWidth_ = dicomFile->imageWidth();
  largestSlideLevelHeight_ = dicomFile->imageHeight();
  wsiRequest_->frameSizeX = dicomFile->frameWidth();
  wsiRequest_->frameSizeY = dicomFile->frameHeight();
  if (wsiRequest_->studyId.size() < 1) {
    wsiRequest_->studyId =
            std::move(static_cast<std::string>(dicomFile->studyInstanceUID()));
  }
  if (wsiRequest_->seriesId.size() < 1) {
    wsiRequest_->seriesId =
           std::move(static_cast<std::string>(dicomFile->seriesInstanceUID()));
  }
  if (wsiRequest_->imageName.size() < 1) {
    wsiRequest_->imageName =
           std::move(static_cast<std::string>(dicomFile->seriesDescription()));
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

std::string WsiToDcm::initOpenSlide() {
  svsLevelCount_ = openslide_get_level_count(getOpenSlidePtr());
  // Openslide API call 0 returns dimensions of highest resolution image.
  openslide_get_level_dimensions(getOpenSlidePtr(), 0,
                                 &largestSlideLevelWidth_,
                                 &largestSlideLevelHeight_);
  std::string vendor(openslide_get_property_value(getOpenSlidePtr(),
                     OPENSLIDE_PROPERTY_NAME_VENDOR));
  BOOST_LOG_TRIVIAL(info) << "Reading " << vendor.c_str() << " formatted WSI.";
  if (vendor == "dicom") {
    wsiRequest_->startOnLevel = std::max(wsiRequest_->startOnLevel, 1);
  }
  tiffFile_ = nullptr;
  if (wsiRequest_->SVSImportPreferScannerTileingForAllLevels ||
      wsiRequest_->SVSImportPreferScannerTileingForLargestLevel) {
    bool useSVSTileing = false;
    if (vendor == "aperio" || vendor == "generic-tiff") {
      tiffFile_ = std::make_unique<TiffFile>(wsiRequest_->inputFile);
      if (tiffFile_->isLoaded()) {
          int32_t level = tiffFile_->getDirectoryIndexMatchingImageDimensions(
                            largestSlideLevelWidth_, largestSlideLevelHeight_);
          if (level != -1) {
            tiffFile_ = std::make_unique<TiffFile>(*tiffFile_, level);
            TiffFrame tiffFrame(tiffFile_.get(), 0, true);
            if (!tiffFrame.tiffDirectory()->isJpeg2kCompressed() &&
                !tiffFrame.tiffDirectory()->isJpegCompressed()) {
              BOOST_LOG_TRIVIAL(error) << "Tiff contains unexpected format.";
              throw 1;
            } else if (tiffFrame.tiffDirectory()->isJpegCompressed() &&
                       !tiffFrame.canDecodeJpeg()) {
              BOOST_LOG_TRIVIAL(error) << "Error decoding JPEG in SVS.";
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
  return vendor;
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
  slideLevelDim->levelToGet = 0;
  slideLevelDim->multiplicator = 1;
  slideLevelDim->downsample = 1;
  slideLevelDim->downsampleOfLevel = 1;
  slideLevelDim->sourceLevelWidth = largestSlideLevelWidth_;
  slideLevelDim->sourceLevelHeight = largestSlideLevelHeight_;
  slideLevelDim->downsampledLevelWidth = largestSlideLevelWidth_;
  slideLevelDim->downsampledLevelHeight = largestSlideLevelHeight_;
  slideLevelDim->sourceDerivationDescription =
                              std::move(static_cast<std::string>(description));
  slideLevelDim->useSourceDerivationDescriptionForDerivedImage = true;
  slideLevelDim->readFromTiff = false;
  slideLevelDim->readOpenslide = false;
  slideLevelDim->levelCompression = UNKNOWN;
  return std::move(slideLevelDim);
}

std::unique_ptr<SlideLevelDim> WsiToDcm::getSlideLevelDim(int64_t downsample,
                                            SlideLevelDim *priorLevel) {
  int32_t levelToGet;
  bool readOpenslide = false;
  std::string sourceDerivationDescription = "";
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
  double multiplicator, downsampleOfLevel;
  int64_t sourceLevelWidth, sourceLevelHeight;
  bool generateFromPrimarySource = true;
  bool readFromTiff = false;
  if ((tiffFile_ != nullptr && tiffFile_->isInitalized()) &&
      ((downsample == 1 &&
        wsiRequest_->SVSImportPreferScannerTileingForLargestLevel) ||
        wsiRequest_->SVSImportPreferScannerTileingForAllLevels)) {
    sourceLevelWidth = largestSlideLevelWidth_ / downsample;
    sourceLevelHeight = largestSlideLevelHeight_ / downsample;
    levelToGet = tiffFile_->getDirectoryIndexMatchingImageDimensions(
                              sourceLevelWidth, sourceLevelHeight);
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
      // Progressive downsampling, level to get not used. Init to -1.
      levelToGet = -1;
      sourceLevelWidth = priorLevel->downsampledLevelWidth;
      sourceLevelHeight = priorLevel->downsampledLevelHeight;
      generateFromPrimarySource = false;
      // Source component of DCM_DerivationDescription
      // describes in text where imaging data was acquired from.
      if (priorLevel->useSourceDerivationDescriptionForDerivedImage) {
        sourceDerivationDescription = priorLevel->sourceDerivationDescription;
      } else if (downsampleOfLevel > 1.0) {
        sourceDerivationDescription =
          std::string("Image frame/tiles generated by downsampling, ") +
          std::to_string(downsampleOfLevel) + " times; "
          "raw pixel values extracted from previous image level and ";
      } else {
        sourceDerivationDescription =
          std::string("Image frame/tiles generated from the raw pixel values "
          "and ");
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
    openslide_get_level_dimensions(getOpenSlidePtr(),
                                   levelToGet,
                                   &sourceLevelWidth,
                                   &sourceLevelHeight);
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
  // downsampledLevelHeight and downsampledLevelWidth will reflect
  // new starting position.
  int64_t downsampledLevelWidth, downsampledLevelHeight;
  int64_t downsampledLevelFrameWidth, downsampledLevelFrameHeight;
  DCM_Compression levelCompression;
  if (downsample <= 1) {
    levelCompression = wsiRequest_->firstlevelCompression;
  } else {
    levelCompression = wsiRequest_->compression;
  }
  sourceLevelWidth -= initialX_;
  sourceLevelHeight -= initialY_;
  dimensionDownsampling(wsiRequest_->frameSizeX, wsiRequest_->frameSizeY,
                        sourceLevelWidth, sourceLevelHeight,
                        retile_, downsampleOfLevel,
                        &downsampledLevelWidth,
                        &downsampledLevelHeight,
                        &downsampledLevelFrameWidth,
                        &downsampledLevelFrameHeight);
  slideLevelDim->readFromTiff = readFromTiff;
  slideLevelDim->levelToGet = levelToGet;
  slideLevelDim->downsample = downsample;
  slideLevelDim->multiplicator = multiplicator;
  slideLevelDim->downsampleOfLevel = downsampleOfLevel;
  slideLevelDim->sourceLevelWidth = sourceLevelWidth;
  slideLevelDim->sourceLevelHeight = sourceLevelHeight;
  slideLevelDim->downsampledLevelWidth = downsampledLevelWidth;
  slideLevelDim->downsampledLevelHeight = downsampledLevelHeight;
  slideLevelDim->downsampledLevelFrameWidth = downsampledLevelFrameWidth;
  slideLevelDim->downsampledLevelFrameHeight = downsampledLevelFrameHeight;
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

class LevelProcessOrder {
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

void WsiToDcm::getSlideDownSamplingLevels(
                     std::vector<DownsamplingSlideState> *slideDownsampleState,
                     SlideLevelDim *startPyramidCreationDim) {
  int32_t levels;
  if (retile_) {
    double singleframe_downsample_width =
      static_cast<double>(largestSlideLevelWidth_) /
      static_cast<double>(wsiRequest_->frameSizeX);
    double singleframe_downsample_height =
       static_cast<double>(largestSlideLevelHeight_) /
       static_cast<double>(wsiRequest_->frameSizeY);
    double singleframe_downsample = std::max<double>(
                  singleframe_downsample_width, singleframe_downsample_height);
    if (wsiRequest_->downsamples.size() > 0 &&
        (wsiRequest_->stopDownsamplingAtSingleFrame ||
         wsiRequest_->includeSingleFrameDownsample)) {
      std::vector<int>::iterator ds_it = wsiRequest_->downsamples.begin();
      double largest_ds = *ds_it;
      for (; ds_it !=  wsiRequest_->downsamples.end(); ++ds_it) {
        if (*ds_it >= singleframe_downsample) {
          if (wsiRequest_->includeSingleFrameDownsample &&
              *ds_it > singleframe_downsample) {
            ds_it = wsiRequest_->downsamples.insert(ds_it,
                                                    singleframe_downsample);
          }
          largest_ds = *ds_it;
          if (wsiRequest_->stopDownsamplingAtSingleFrame) {
            wsiRequest_->downsamples.resize(1 +
                      std::distance(wsiRequest_->downsamples.begin(), ds_it));
          }
          break;
        }
        largest_ds = *ds_it;
     }
     if (largest_ds < singleframe_downsample &&
         wsiRequest_->includeSingleFrameDownsample) {
        wsiRequest_->downsamples.push_back(singleframe_downsample);
     }
    } else if (wsiRequest_->downsamples.size() == 0 &&
              wsiRequest_->retileLevels > 0) {
      int64_t single_frame_level = 1 + static_cast<int32_t>(std::ceil(
                                       log2(singleframe_downsample)));
      if ((wsiRequest_->stopDownsamplingAtSingleFrame &&
           wsiRequest_->includeSingleFrameDownsample) ||
          (wsiRequest_->stopDownsamplingAtSingleFrame &&
           wsiRequest_->retileLevels > single_frame_level) ||
          (wsiRequest_->includeSingleFrameDownsample &&
           wsiRequest_->retileLevels < single_frame_level)) {
        wsiRequest_->retileLevels = single_frame_level;
      }
    }
  }
  if (retile_) {
    if (wsiRequest_->downsamples.size() > 0) {
      levels = wsiRequest_->downsamples.size();
    } else {
      levels = wsiRequest_->retileLevels;
    }
  } else {
    levels = svsLevelCount_;
  }
  std::unique_ptr<SlideLevelDim> smallestSlideDim = nullptr;
  std::vector<std::unique_ptr<LevelProcessOrder>> levelOrderVec;
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

    int64_t downsample = 1;
    if (retile_) {
      if (wsiRequest_->downsamples.size() > level &&
          wsiRequest_->downsamples[level] >= 1) {
        downsample = wsiRequest_->downsamples[level];
        if (level > 0 &&  downsample ==  wsiRequest_->downsamples[level - 1]) {
          continue;
        }
      } else if (wsiRequest_->downsamples.size() == 0) {
        downsample = pow(2, level);
      } else {
        continue;
      }
    }
    std::unique_ptr<SlideLevelDim> tempSlideLevelDim =
        std::move(getSlideLevelDim(downsample, priorSlideLevelDim));
    if (tempSlideLevelDim->downsampledLevelWidth == 0 ||
        tempSlideLevelDim->downsampledLevelHeight == 0) {
      // frame is being downsampled to nothing skip file.
      BOOST_LOG_TRIVIAL(debug) << "Layer has a 0 length dimension."
                                  " Skipping dcm generation for layer.";
      break;
    }
    smallestSlideDim = std::move(tempSlideLevelDim);
    const int64_t frameX = std::ceil(
          static_cast<double>(smallestSlideDim->downsampledLevelWidth) /
          static_cast<double>(smallestSlideDim->downsampledLevelFrameWidth));
    const int64_t frameY = std::ceil(
          static_cast<double>(smallestSlideDim->downsampledLevelHeight) /
          static_cast<double>(smallestSlideDim->downsampledLevelFrameHeight));
    const int64_t frameCount = frameX * frameY;
    BOOST_LOG_TRIVIAL(debug) << "Dimensions Level[" <<
                                level << "]: " <<
                                smallestSlideDim->downsampledLevelWidth <<
                                ", " <<
                                smallestSlideDim->downsampledLevelHeight;
    priorSlideLevelDim = smallestSlideDim.get();
    levelOrderVec.push_back(std::make_unique<LevelProcessOrder>(level,
                                              downsample,
                                              smallestSlideDim->readFromTiff));
    BOOST_LOG_TRIVIAL(debug) << "Level[" <<level << "] frames:" <<
                                frameX << ", " << frameY;
  }
  if (smallestSlideDim == nullptr) {
    return;
  }
  // Process process levels in order of area largest to smallest.
  const int32_t levelCount = static_cast<int32_t>(levelOrderVec.size());
  double sourceLevelDownsample =  1.0;
  int64_t instanceNumberCounter = 1;
  for (int32_t idx = 0; idx <  levelCount; ++idx) {
    DownsamplingSlideState slideState;
    const double next_cross_level_downsample =
      levelOrderVec[idx]->downsample() / sourceLevelDownsample;
    if (wsiRequest_->preferProgressiveDownsampling &&
        next_cross_level_downsample > 8.0) {
       slideState.saveDicom = false;
       slideState.generateCompressedRaw = true;
       slideState.instanceNumber = 0;
       if (idx == 0 && slideDownsampleState->size() == 0) {
         // create a virtual level for level 1.
         if (startPyramidCreationDim != nullptr) {
           slideState.downsample = 1.0;
           slideDownsampleState->push_back(slideState);
         } else {
           // if getting levels from openslide get largest starting level
           // which acquires imaging from highest magnification.
           double starting_downsample = openslide_get_level_downsample(
                                          getOpenSlidePtr(), 0);
           if (starting_downsample != sourceLevelDownsample) {
             slideState.downsample = starting_downsample;
             slideDownsampleState->push_back(slideState);
           }
         }
       }
       while (levelOrderVec[idx]->downsample() / sourceLevelDownsample > 8) {
          sourceLevelDownsample = sourceLevelDownsample * 8.0;
          slideState.downsample = sourceLevelDownsample;
          slideDownsampleState->push_back(slideState);
       }
    }
    sourceLevelDownsample = levelOrderVec[idx]->downsample();
    slideState.downsample = sourceLevelDownsample;
    slideState.instanceNumber = instanceNumberCounter;
    instanceNumberCounter += 1;
    slideState.saveDicom = true;
    slideState.generateCompressedRaw = false;
    if (sourceLevelDownsample == smallestSlideDim->downsample) {
      // if last slice do not save raw
      slideDownsampleState->push_back(slideState);
      break;
    }
    if (levelOrderVec[idx+1]->readLevelFromTiff()) {
      // memory & compute optimization
      // if next slice reads from tiff do save raw
      slideDownsampleState->push_back(slideState);
      continue;
    } else if ((startPyramidCreationDim == nullptr) &&
                (slideDownsampleState->size() == 0) &&
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
      slideDownsampleState->push_back(slideState);
      continue;
    } else {
      // Otherwise save compressed slice if progressive downsampling is
      // enabled.
      slideState.generateCompressedRaw = wsiRequest_->
                                            preferProgressiveDownsampling;
      slideDownsampleState->push_back(slideState);
      continue;
    }
  }
}

double abstractDicomDimensionMM(double imageDimMM, uint64_t imageDim,
                                int64_t imageDimOffset) {
  if (imageDimOffset <= 0) {
    return imageDimMM;
  }
  return ((static_cast<double>(imageDim - imageDimOffset) * imageDimMM) /
          static_cast<double>(imageDim));
}


int64_t sourceLevelPixelCoord(int64_t source_level_dim,
                              int64_t downsample_level_dim,
                              int64_t downsample_layer_coord,
                              int64_t inital_offset) {
  return (source_level_dim * downsample_layer_coord / downsample_level_dim +
          inital_offset);
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
  std::string openslide_vendor("");
  if (wsiRequest_->genPyramidFromUntiledImage) {
    BOOST_LOG_TRIVIAL(info) << "Reading untiled image.";
    std::string description = "Image frames generated from "
      " values extracted from un-tiled image (" +
      wsiRequest_->inputFile + ") and ";
    abstractDicomFile = std::move(initUntiledImageIngest());
    slideLevelDim = std::move(initAbstractDicomFileSourceLevelDim(
                                                         description.c_str()));
    // Initalize height and width dimensions directly from file measures
    levelWidthMM = abstractDicomDimensionMM(abstractDicomFile->imageWidthMM(),
                                            largestSlideLevelWidth_,
                                            initialX_);
    levelHeightMM = abstractDicomDimensionMM(abstractDicomFile->imageHeightMM(),
                                            largestSlideLevelHeight_,
                                            initialY_);
  } else {
    // Initalize openslide
    if (initOpenSlide() == "dicom") {
       // DICOM will be read with openslide. Do not init for reading
       // with DCMTK. Perform minor validation and init dimensions, and uid.
       // from DICOM provided.
       std::unique_ptr<AbstractDcmFile> dicom_file =
                                        std::move(initDicomIngest(false));
       levelWidthMM = abstractDicomDimensionMM(dicom_file->imageWidthMM(),
                                            largestSlideLevelWidth_,
                                            initialX_);
       levelHeightMM = abstractDicomDimensionMM(dicom_file->imageHeightMM(),
                                            largestSlideLevelHeight_,
                                            initialY_);
    } else {
      double openslideMPP_X = getOpenSlideDimensionMM("openslide.mpp-x");
      double openslideMPP_Y = getOpenSlideDimensionMM("openslide.mpp-y");
      levelWidthMM = getDimensionMM(largestSlideLevelWidth_ - initialX_,
                                  openslideMPP_X);
      levelHeightMM = getDimensionMM(largestSlideLevelHeight_ - initialY_,
                                   openslideMPP_Y);
    }
  }
  if (largestSlideLevelWidth_ <= initialX_ ||
      largestSlideLevelHeight_ <= initialY_) {
    BOOST_LOG_TRIVIAL(error) << "Input image dimensions are to small.";
    return 1;
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
  std::vector<DownsamplingSlideState> downsampleSlide;
  getSlideDownSamplingLevels(&downsampleSlide,
                             slideLevelDim.get());
  DICOMFileFrameRegionReader higherMagnifcationDicomFiles;
  std::vector<std::unique_ptr<AbstractDcmFile>> generatedDicomFiles;
  if (abstractDicomFile != nullptr) {
    generatedDicomFiles.push_back(std::move(abstractDicomFile));
    higherMagnifcationDicomFiles.setDicomFiles(std::move(generatedDicomFiles),
                                               nullptr);
  }
  clearOpenSlidePtr();
  for (size_t levelIndex = 0;
       levelIndex < downsampleSlide.size();
       ++levelIndex) {
    const bool save_dicom_instance_to_disk = downsampleSlide[levelIndex
                                                      ].saveDicom;
    const bool saveCompressedRaw = downsampleSlide[levelIndex
                                                      ].generateCompressedRaw;
    const int32_t instanceNumber = downsampleSlide[levelIndex].instanceNumber;

    SlideLevelDim *priorSlideLevel;
    if (higherMagnifcationDicomFiles.dicomFileCount() > 0) {
      priorSlideLevel = slideLevelDim.get();
    } else {
      priorSlideLevel = nullptr;
    }
    const int64_t downsample = downsampleSlide[levelIndex].downsample;
    slideLevelDim = std::move(getSlideLevelDim(downsample, priorSlideLevel));
    const int32_t levelToGet  = slideLevelDim->levelToGet;
    const double multiplicator = slideLevelDim->multiplicator;
    const double downsampleOfLevel = slideLevelDim->downsampleOfLevel;
    const int64_t sourceLevelWidth = slideLevelDim->sourceLevelWidth;
    const int64_t sourceLevelHeight = slideLevelDim->sourceLevelHeight;
    const int64_t downsampledLevelWidth = slideLevelDim->downsampledLevelWidth;
    const int64_t downsampledLevelHeight =
                    slideLevelDim->downsampledLevelHeight;
    const int64_t downsampledLevelFrameWidth =
                    slideLevelDim->downsampledLevelFrameWidth;
    const int64_t downsampledLevelFrameHeight =
                    slideLevelDim->downsampledLevelFrameHeight;
    const std::string sourceDerivationDescription =
                                    slideLevelDim->sourceDerivationDescription;

    DCM_Compression levelCompression = slideLevelDim->levelCompression;
    if (downsampledLevelWidth == 0 || downsampledLevelHeight == 0) {
      // frame is being downsampled to nothing skip image generation.
      BOOST_LOG_TRIVIAL(debug) << "Layer has a 0 length dimension. Skipping "
                                  "dcm generation for layer.";
      break;
    }
    //  BOOST_LOG_TRIVIAL(debug) << "Starting Instance Number " <<
    //    instanceNumber << "\n level size: " <<
    //    sourceLevelWidth << ", " << sourceLevelHeight << "\n"
    //    "multiplicator: " << multiplicator << "\n"
    //    "levelToGet: " << levelToGet << "\n"
    //    "downsample: " << downsample << "\n"
    //    "downsampleOfLevel: " << downsampleOfLevel << "\n";
    const int frameX = std::ceil(
                         static_cast<double>(downsampledLevelWidth) /
                         static_cast<double>(downsampledLevelFrameWidth));
    const int frameY = std::ceil(
                         static_cast<double>(downsampledLevelHeight) /
                         static_cast<double>(downsampledLevelFrameHeight));

    if (slideLevelDim->readOpenslide || slideLevelDim->readFromTiff) {
      // If slide level was initalized from openslide or tiff
      // clear higherMagnifcationDicomFiles so
      // level is downsampled from openslide and not
      // prior level if progressiveDownsample is enabled.
      higherMagnifcationDicomFiles.clearDicomFiles();
    }
    BOOST_LOG_TRIVIAL(debug) << "higherMagnifcationDicomFiles " <<
                          higherMagnifcationDicomFiles.dicomFileCount();
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
      if (tiffFrameFilePtr->fileDirectory()->isJpegCompressed()) {
        levelCompression = JPEG;
      } else if (tiffFrameFilePtr->fileDirectory()->isJpeg2kCompressed()) {
        levelCompression = JPEG2000;
      } else {
        BOOST_LOG_TRIVIAL(error) << "Tiff file is not jpeg or jpeg2000 "
                                    "encoded.";
        return 1;
      }
    }
    // Step across destination imaging height.
    for (int64_t downsampledLevelYCoord = 0;
        downsampledLevelYCoord < downsampledLevelHeight;
        downsampledLevelYCoord += downsampledLevelFrameHeight) {
      // back project from destination to source.

      const int64_t sourceLevelYCoord = sourceLevelPixelCoord(
                                          sourceLevelHeight,
                                          downsampledLevelHeight,
                                          downsampledLevelYCoord,
                                          initialY_);
      int64_t sourceHeight;
      if (frameY == 1) {
        // if imaging fits in one frame then sampling dimensions = source dim
        sourceHeight = sourceLevelHeight;
      } else {
        // Differences between start of next frame and current frame
        sourceHeight = sourceLevelPixelCoord(
                         sourceLevelHeight,
                         downsampledLevelHeight,
                         downsampledLevelYCoord + downsampledLevelFrameHeight,
                         initialY_) - sourceLevelYCoord;
      }
      // Step across destination imaging with.
      for (int64_t downsampledLevelXCoord = 0;
          downsampledLevelXCoord < downsampledLevelWidth;
          downsampledLevelXCoord += downsampledLevelFrameWidth) {
        // back project from destination to source.
        const int64_t sourceLevelXCoord = sourceLevelPixelCoord(
                                            sourceLevelWidth,
                                            downsampledLevelWidth,
                                            downsampledLevelXCoord,
                                            initialX_);
        int64_t sourceWidth;
        if (frameX == 1) {
          // if imaging fits in one frame then sampling dimensions = source dim
          sourceWidth = sourceLevelWidth;
        } else {
          // Differences between start of next frame and current frame
          sourceWidth = sourceLevelPixelCoord(
                          sourceLevelWidth,
                          downsampledLevelWidth,
                          downsampledLevelXCoord + downsampledLevelFrameWidth,
                          initialX_) - sourceLevelXCoord;
        }
        // BOOST_LOG_TRIVIAL(debug) << "Sample: " << sourceLevelXCoord <<
        //   ", " << sourceLevelYCoord << " [" << sourceWidth << ", " <<
        //   sourceHeight << "]";
        std::unique_ptr<Frame> frameData;
        if (slideLevelDim->readFromTiff) {
          frameData = std::make_unique<TiffFrame>(tiffFrameFilePtr.get(),
              frameIndexFromLocation(tiffFrameFilePtr.get(), levelToGet,
              sourceLevelXCoord, sourceLevelYCoord), saveCompressedRaw);
        } else if (wsiRequest_->useOpenCVDownsampling) {
          frameData = std::make_unique<OpenCVInterpolationFrame>(
              osptr_.get(), sourceLevelXCoord, sourceLevelYCoord, levelToGet,
              sourceWidth, sourceHeight, downsampledLevelFrameWidth,
              downsampledLevelFrameHeight, levelCompression,
              wsiRequest_->quality, wsiRequest_->jpegSubsampling,
              sourceLevelWidth, sourceLevelHeight, largestSlideLevelWidth_,
              largestSlideLevelHeight_, saveCompressedRaw,
              &higherMagnifcationDicomFiles,
              wsiRequest_->openCVInterpolationMethod);
        } else {
          frameData = std::make_unique<NearestNeighborFrame>(
              osptr_.get(), sourceLevelXCoord, sourceLevelYCoord, levelToGet,
              sourceWidth, sourceHeight, multiplicator,
              downsampledLevelFrameWidth, downsampledLevelFrameHeight,
              levelCompression, wsiRequest_->quality,
              wsiRequest_->jpegSubsampling, saveCompressedRaw,
              &higherMagnifcationDicomFiles);
        }
        if (higherMagnifcationDicomFiles.dicomFileCount() != 0) {
          frameData->incSourceFrameReadCounter();
        }
        framesInitalizationData.push_back(std::move(frameData));
      }
    }
    BOOST_LOG_TRIVIAL(debug) << "Level Frame Count: " <<
                          framesInitalizationData.size();
    boost::asio::thread_pool pool(threadsForPool);
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
      boost::asio::post(
          pool, [frameData = frameData->get()]() { frameData->sliceFrame(); });
      framesData.push_back(std::move(*frameData));
      if (wsiRequest_->batchLimit > 0 &&
          framesData.size() >= wsiRequest_->batchLimit) {
        std::unique_ptr<DcmFileDraft> filedraft =
            std::make_unique<DcmFileDraft>(
                std::move(framesData), wsiRequest_->outputFileMask,
                downsampledLevelWidth, downsampledLevelHeight, instanceNumber,
                wsiRequest_->studyId, wsiRequest_->seriesId,
                wsiRequest_->imageName, levelCompression, wsiRequest_->tiled,
                tags.get(), levelWidthMM, levelHeightMM, downsample,
                &generatedDicomFiles, sourceDerivationDescription,
                save_dicom_instance_to_disk);
        boost::asio::post(pool, [th_filedraft = filedraft.get()]() {
          th_filedraft->saveFile();
        });
        generatedDicomFiles.push_back(std::move(filedraft));
      }
    }
    if (framesData.size() > 0) {
      std::unique_ptr<DcmFileDraft> filedraft = std::make_unique<DcmFileDraft>(
          std::move(framesData), wsiRequest_->outputFileMask,
          downsampledLevelWidth, downsampledLevelHeight, instanceNumber,
          wsiRequest_->studyId, wsiRequest_->seriesId,
          wsiRequest_->imageName, levelCompression,
          wsiRequest_->tiled, tags.get(), levelWidthMM, levelHeightMM,
          downsample, &generatedDicomFiles, sourceDerivationDescription,
          save_dicom_instance_to_disk);
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
