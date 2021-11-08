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
#include <tuple>
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
    osr_ = NULL;
    return;
  }
  osr_ = openslide_open(slideFile);
  if (openslide_get_error(osr_)) {
    BOOST_LOG_TRIVIAL(error) << openslide_get_error(osr_);
    osr_ = NULL;
    return;
  }
  svsLevelCount_ = openslide_get_level_count(osr_);
  // Openslide API call 0 returns dimensions of highest resolution image.
  openslide_get_level_dimensions(osr_, 0,
                                 &largestSlideLevelWidth_,
                                 &largestSlideLevelHeight_);
  BOOST_LOG_TRIVIAL(info) << "dicomization is started";
  initialX_ = 0;
  initialY_ = 0;
  if (wsiRequest_->dropFirstRowAndColumn) {
    initialX_ = 1;
    initialY_ = 1;
  }
  const int64_t downsample_size = wsiRequest_->downsamples.size();
  if (downsample_size > 0) {
    if (wsiRequest_->retileLevels > 0 &&
        wsiRequest_->retileLevels != downsample_size) {
      BOOST_LOG_TRIVIAL(info) << "--levels command line parameter is " <<
                                 "unnecessary levels initialized to " <<
                                 downsample_size << " from --downsamples.";
    }
    wsiRequest_->retileLevels = downsample_size;
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
}

WsiToDcm::~WsiToDcm() {
  if (osr_ != NULL) {
    openslide_close(osr_);
    osr_ = NULL;
  }
}

void WsiToDcm::checkArguments() {
  if (osr_ == NULL) {
    // Openslide did not intialize
    throw 1;
  }
  if (wsiRequest_ == NULL) {
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
      openslide_get_level_dimensions(osr_, levelToGet, &lw, &lh);
      if (lw < tw || lh < th) {
        break;
      }
    }
    return (levelToGet - 1);
}

std::unique_ptr<SlideLevelDim> WsiToDcm::getSlideLevelDim(int32_t level,
                                            SlideLevelDim *priorLevel,
                                            SlideLevelDim * smallestSlideDim,
                                            bool enableProgressiveDownsample) {
  int32_t levelToGet = std::max(level, 0);
  int64_t downsample = 1;
  bool readOpenslide = false;
  if (level < 0) {
     // if -1 passed in load largest level using openslide.
     level = 0;
     enableProgressiveDownsample = false;
     priorLevel = NULL;
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
  double multiplicator;
  double downsampleOfLevel;
  int64_t levelWidth;
  int64_t levelHeight;
  bool generateUsingOpenSlide = true;
  // ProgressiveDownsampling
  if (wsiRequest_->preferProgressiveDownsampling &&
                                                 enableProgressiveDownsample) {
    if (priorLevel != NULL) {
      multiplicator = static_cast<double>(priorLevel->downsample);
      downsampleOfLevel = static_cast<double>(downsample) / multiplicator;
      // check that downsampling is going from higher to lower magnification
      if (downsampleOfLevel >= 1.0) {
        levelToGet = priorLevel->level;
        levelWidth = priorLevel->levelWidthDownsampled;
        levelHeight = priorLevel->levelHeightDownsampled;
        generateUsingOpenSlide = false;
      }
    }
  }

  // if no higherMagnifcationDicomFiles then downsample from openslide
  if (generateUsingOpenSlide) {
    levelToGet = getOpenslideLevelForDownsample(downsample);

    multiplicator = openslide_get_level_downsample(osr_, levelToGet);
    // Downsampling factor required to go from selected
    // downsampled level to the desired level of downsampling
    if (wsiRequest_->floorCorrectDownsampling) {
      multiplicator = floor(multiplicator);
    }
    downsampleOfLevel = static_cast<double>(downsample) / multiplicator;
    openslide_get_level_dimensions(osr_, levelToGet, &levelWidth, &levelHeight);
    readOpenslide = true;
  }
  // Adjust level size by starting position if skipping row and column.
  // levelHeightDownsampled and levelWidthDownsampled will reflect
  // new starting position.
  int64_t cropSourceLevelWidth = 0;
  int64_t cropSourceLevelHeight = 0;
  if (smallestSlideDim != NULL) {
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
  DCM_Compression levelCompression = wsiRequest_->compression;
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

  std::unique_ptr<SlideLevelDim> slideLevelDim;
  slideLevelDim = std::make_unique<SlideLevelDim>();
  slideLevelDim->level = level;
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

double  WsiToDcm::getDownsampledLevelDimensionMM(
                                           const int64_t adjustedFirstLevelDim,
                                           const int64_t levelDimDownsampled,
                                           const double downsample,
                                       const char* openSlideLevelDimProperty) {
  double firstLevelMpp = 0.0;
  const char *openslideFirstLevelMpp =
      openslide_get_property_value(osr_, openSlideLevelDimProperty);
  if (openslideFirstLevelMpp != nullptr) {
    firstLevelMpp = std::stod(openslideFirstLevelMpp);
  }
  const int64_t dimAdjustment = (levelDimDownsampled * downsample) -
                                                  adjustedFirstLevelDim;
  const double levelDim = static_cast<double>(adjustedFirstLevelDim +
                                                 dimAdjustment);
  return levelDim * firstLevelMpp / 1000;
}

bool downsample_order(const std::tuple<int32_t, int64_t> &i,
                        const std::tuple<int32_t, int64_t> &j) {
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

std::unique_ptr<SlideLevelDim>  WsiToDcm::getSmallestSlideDim(
                                          std::vector<int32_t> *slideLevels) {
  std::unique_ptr<SlideLevelDim> smallestSlideDim = NULL;
  int32_t levels;
  if (retile_) {
    levels = wsiRequest_->retileLevels;
  } else {
    levels = svsLevelCount_;
  }
  bool smallestLevelIsSingleFrame = false;
  std::vector<std::tuple<int32_t, int64_t>> levelProcessOrder;
  int64_t smallestDownsample;
  bool layerHasShownZeroLengthDimMsg = false;
  for (int32_t level = wsiRequest_->startOnLevel; level < levels &&
           (wsiRequest_->stopOnLevel < wsiRequest_->startOnLevel ||
                                 level <= wsiRequest_->stopOnLevel); level++) {
      std::unique_ptr<SlideLevelDim> tempSlideLevelDim =
              std::move(getSlideLevelDim(level, smallestSlideDim.get(), NULL));
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
      BOOST_LOG_TRIVIAL(debug) << "Uncropped dimensions Level[" <<
                                  level << "]: " <<
                                  tempSlideLevelDim->levelWidthDownsampled <<
                                   ", " <<
                                   tempSlideLevelDim->levelHeightDownsampled;
      bool setSmallestSlice = false;
      if (smallestSlideDim == NULL) {
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
      std::tuple<int32_t, int64_t> tpl(level, tempDownsample);
      levelProcessOrder.push_back(tpl);
      BOOST_LOG_TRIVIAL(debug) << "Level[" <<level << "] frames:" <<
                                    frameX << ", " << frameY;
      if (wsiRequest_->stopDownsamplingAtSingleFrame && frameCount <= 1) {
        smallestLevelIsSingleFrame = true;
      }
    }
    // Process process levels in order of area largest to smallest.
    if (smallestSlideDim != NULL) {
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
          break;
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

  BOOST_LOG_TRIVIAL(debug) << " ";
  BOOST_LOG_TRIVIAL(debug) << "Level Count: " << svsLevelCount_;
  // Determine smallest_slide downsample dimensions to enable
  // slide pixel spacing normalization croping to ensure pixel
  // spacing across all downsample images is uniform.
  int64_t largestSlideWidthCrop;
  int64_t largestSlideHeightCrop;
  std::vector<int32_t> slideLevels;
  std::unique_ptr<SlideLevelDim> smallestSlideDim = std::move(
                                          getSmallestSlideDim(&slideLevels));
  if (wsiRequest_->cropFrameToGenerateUniformPixelSpacing) {
    // passing -1 in forces getSlideLevelDim to return non-downsampled
    // image dimensions a.k.a. largest image from openslide.
    std::unique_ptr<SlideLevelDim> largestDimensions =
                  std::move(getSlideLevelDim(-1, NULL, smallestSlideDim.get()));
    largestSlideWidthCrop = largestDimensions->cropSourceLevelWidth;
    largestSlideHeightCrop = largestDimensions->cropSourceLevelHeight;
  } else {
    largestSlideWidthCrop = 0;
    largestSlideHeightCrop = 0;
    smallestSlideDim = NULL;
  }

  BOOST_LOG_TRIVIAL(debug) << "Cropping source image: " <<
                              largestSlideWidthCrop <<
                              ", " <<
                              largestSlideHeightCrop;

  DICOMFileFrameRegionReader higherMagnifcationDicomFiles;
  std::vector<std::unique_ptr<DcmFileDraft>> generatedDicomFiles;
  std::unique_ptr<SlideLevelDim> slideLevelDim = NULL;
  for (const int32_t &level : slideLevels) {
    // only progressive downsample if have higher mag buffer filled.
    slideLevelDim = std::move(getSlideLevelDim(level,
                                               slideLevelDim.get(),
                                               smallestSlideDim.get(),
                        higherMagnifcationDicomFiles.dicom_file_count() > 0));

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
    BOOST_LOG_TRIVIAL(debug) << "level size: " << levelWidth << ' '
                             << levelHeight << ' ' << multiplicator;
    uint32_t row = 1;
    uint32_t column = 1;
    int64_t x = initialX_;
    int64_t y = initialY_;
    size_t numberOfFrames = 0;
    std::vector<std::unique_ptr<Frame>> framesData;

    const double levelWidthMM = getDownsampledLevelDimensionMM(
                          largestSlideLevelWidth_ - largestSlideWidthCrop -
                          initialX_, levelWidthDownsampled, downsample,
                          "openslide.mpp-x");
    const double levelHeightMM = getDownsampledLevelDimensionMM(
                          largestSlideLevelHeight_ - largestSlideHeightCrop -
                          initialY_, levelHeightDownsampled, downsample,
                          "openslide.mpp-y");

    BOOST_LOG_TRIVIAL(debug) << "multiplicator: " << multiplicator;
    BOOST_LOG_TRIVIAL(debug) << "levelToGet: " << levelToGet;
    BOOST_LOG_TRIVIAL(debug) << "downsample: " << downsample;
    BOOST_LOG_TRIVIAL(debug) << "downsampleOfLevel: " << downsampleOfLevel;
    BOOST_LOG_TRIVIAL(debug) << "frameDownsampled: " << frameWidthDownsampled
                             << ", " << frameHeightDownsampled;
    BOOST_LOG_TRIVIAL(debug) << "higherMagnifcationDicomFiles " <<
                          higherMagnifcationDicomFiles.dicom_file_count();
    BOOST_LOG_TRIVIAL(debug) << "Croping source image: " <<
                             cropSourceLevelWidth << ", " <<
                             cropSourceLevelHeight;
    // Preallocate vector space for frames
    const int frameX = std::ceil(static_cast<double>(levelWidthDownsampled) /
                                 static_cast<double>(levelFrameWidth));
    const int frameY = std::ceil(static_cast<double>(levelHeightDownsampled) /
                                 static_cast<double>(levelFrameHeight));
    if (wsiRequest_->batchLimit == 0) {
      framesData.reserve(frameX * frameY);
    } else {
      framesData.reserve(std::min(frameX * frameY, wsiRequest_->batchLimit));
    }
    bool saveCompressedRaw = wsiRequest_->preferProgressiveDownsampling;
    if ((downsample == 1) && (0 == getOpenslideLevelForDownsample(2))) {
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

    while (y < levelHeight - cropSourceLevelHeight) {
      while (x < levelWidth - cropSourceLevelWidth) {
        assert(osr != nullptr && openslide_get_error(osr_) == nullptr);
        std::unique_ptr<Frame> frameData;
        if (wsiRequest_->useBilinearDownsampling) {
          frameData = std::make_unique<BilinearInterpolationFrame>(
              osr_, x, y, levelToGet, frameWidthDownsampled,
              frameHeightDownsampled, levelFrameWidth, levelFrameHeight,
              levelCompression, wsiRequest_->quality, levelWidthDownsampled,
              levelHeightDownsampled, levelWidth, levelHeight,
              largestSlideLevelWidth_, largestSlideLevelHeight_,
              saveCompressedRaw, higherMagnifcationDicomFiles);
        } else {
          frameData = std::make_unique<NearestNeighborFrame>(
              osr_, x, y, levelToGet, frameWidthDownsampled,
              frameHeightDownsampled, multiplicator, levelFrameWidth,
              levelFrameHeight, levelCompression, wsiRequest_->quality,
              saveCompressedRaw, higherMagnifcationDicomFiles);
        }

        boost::asio::post(
            pool, [frameData = frameData.get()]() { frameData->sliceFrame(); });
        framesData.push_back(std::move(frameData));
        numberOfFrames++;
        x += frameWidthDownsampled;

        if (wsiRequest_->batchLimit > 0 &&
            framesData.size() >= wsiRequest_->batchLimit) {
          std::unique_ptr<DcmFileDraft> filedraft =
              std::make_unique<DcmFileDraft>(
                  std::move(framesData), wsiRequest_->outputFileMask,
                  levelWidthDownsampled, levelHeightDownsampled, level, row,
                  column, wsiRequest_->studyId, wsiRequest_->seriesId,
                  wsiRequest_->imageName, levelCompression, wsiRequest_->tiled,
                  tags.get(), levelWidthMM, levelHeightMM, downsample,
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
      x = initialX_;
    }
    if (framesData.size() > 0) {
      std::unique_ptr<DcmFileDraft> filedraft = std::make_unique<DcmFileDraft>(
          std::move(framesData), wsiRequest_->outputFileMask,
          levelWidthDownsampled, levelHeightDownsampled, level, row, column,
          wsiRequest_->studyId, wsiRequest_->seriesId, wsiRequest_->imageName,
          levelCompression, wsiRequest_->tiled, tags.get(), levelWidthMM,
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
    // The combination of cropFrameToGenerateUniformPixelSpacing &
    // stopDownsamplingAtSingleFrame can result in cropping images
    // which would otherwise span multiple frames to one frame.
    // Check if stopDownsamplingAtSingleFrame is enabled
    // and if the downsampled image was written in one frame.
    if (wsiRequest_->stopDownsamplingAtSingleFrame && numberOfFrames <= 1) {
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
