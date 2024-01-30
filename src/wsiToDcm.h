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

#ifndef SRC_WSITODCM_H_
#define SRC_WSITODCM_H_
#include <boost/cstdint.hpp>
#include <opencv2/opencv.hpp>

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "src/openslideUtil.h"
#include "src/enums.h"
#include "src/tiffFile.h"
#include "src/dcmFilePyramidSource.h"
#include "src/imageFilePyramidSource.h"
#include "src/jpegCompression.h"

namespace wsiToDicomConverter {

class SlideLevelDim {
 public:
  // level being downsampled from
  int32_t levelToGet;

  // total downsample being done from highest mag (level 0)
  int64_t downsample;

  // downsample from level being downsampled from to level 0
  double multiplicator;

  // downsample between selected source level and level being generated.
  double downsampleOfLevel;

  // width of the source level (pixels)
  int64_t sourceLevelWidth;

  // height of the source level (pixels)
  int64_t sourceLevelHeight;

  // width of level being generated (pixels)
  int64_t downsampledLevelWidth;

  // height of level being generated (pixels)
  int64_t downsampledLevelHeight;

  // frame width in level being generated  (pixels)
  int64_t downsampledLevelFrameWidth;

  // frame height in level being generated (pixels)
  int64_t downsampledLevelFrameHeight;

  // Compression to use to generate output
  DCM_Compression levelCompression;

  // True if source level is being read using openslide.
  bool readOpenslide;

  bool readFromTiff = false;

  // Source component of DCM_DerivationDescription
  // describes in text where imaging data was acquired from.
  std::string sourceDerivationDescription;
  bool useSourceDerivationDescriptionForDerivedImage = false;
};

// Structure for wsi2dcm settings
struct WsiRequest {
  // wsi input file
  std::string inputFile = "";

  // path to save generated files
  std::string outputFileMask = "./";

  // pixel size for frame
  int64_t frameSizeX = 500;
  int64_t frameSizeY = 500;

  // compression - jpeg, jpeg2000, raw
  DCM_Compression compression = JPEG;

  // applicable to jpeg compression from 0(worst) to 100(best)
  int32_t quality = 80;

  // limitation which levels to generate
  int32_t startOnLevel = 0;
  int32_t stopOnLevel = -1;

  // (0008,103E) [LO] SeriesDescription Dicom tag
  std::string imageName = "image";

  // required DICOM metadata, would be generated if not set
  // (0020,000D) [UI] StudyInstanceUID
  std::string studyId;
  // (0020,000E) [UI] SeriesInstanceUID:
  std::string seriesId;

  // json file with additional DICOM metadata
  std::string jsonFile = "";

  // number of levels, levels == 0  means number of
  // levels will be readed from wsi file
  int32_t retileLevels = 0;

  // for each level, downsample is size factor for each level
  // eg: if base level size is 100x100 and downsamples is (1, 2, 10) then
  // level0 100x100
  // level1 50x50
  // level2 10x10
  std::vector<int> downsamples;

  // frame organization type
  // http://dicom.nema.org/medical/dicom/current/output/chtml/part03/sect_C.7.6.17.3.html
  // true:TILED_FULL
  // false:TILED_SPARSE
  bool tiled = true;

  // maximum frames in one file, as limit is exceeded new files is started
  // eg: 3 files will be generated for batchLimit is 10 and 30 frames in level
  int32_t batchLimit = 0;

  // threads to consume during execution
  int8_t threads = -1;

  // start slicing from point (1,1) instead of (0,0) to avoid bug
  //  https://github.com/openslide/openslide/issues/268
  bool dropFirstRowAndColumn = false;

  // print debug messages: dimensions of levels, size of frames
  bool debug = false;

  // stop downsampling if total layer dimensions < 1 frame
  bool stopDownsamplingAtSingleFrame = false;

  // use opencv interpolation instead of boost nearest neighbor
  // interpolation
  bool useOpenCVDownsampling = false;

  // floor correct reported openslide downsampling.
  bool floorCorrectDownsampling = false;

  // prefer progressive downsampling.
  bool preferProgressiveDownsampling = false;

  cv::InterpolationFlags openCVInterpolationMethod = cv::INTER_AREA;

  DCM_Compression firstlevelCompression = JPEG;

  bool SVSImportPreferScannerTileingForLargestLevel = false;
  bool SVSImportPreferScannerTileingForAllLevels = false;
  bool genPyramidFromUntiledImage = false;
  double untiledImageHeightMM = 0.0;
  bool includeSingleFrameDownsample = false;
  JpegSubsampling jpegSubsampling = subsample_420;
};


struct DownsamplingSlideState {
    int32_t downsample;
    int32_t instanceNumber;
    bool generateCompressedRaw;
    bool saveDicom;
};


// Contains static methods for generation DICOM files
// from wsi images supported by openslide
class WsiToDcm {
 public:
  // Performs file checks and generation of tasks
  // for generation of frames and DICOM files
  explicit WsiToDcm(WsiRequest *wsiRequest);
  virtual ~WsiToDcm();

  int wsi2dcm();

  // Generates tasks and handling thread pool
  double getOpenSlideDimensionMM(const char* openSlideProperty);
  void initOpenSlide();
  std::unique_ptr<DcmFilePyramidSource> initDicomIngest();
  std::unique_ptr<ImageFilePyramidSource> initUntiledImageIngest();
  std::unique_ptr<SlideLevelDim> initAbstractDicomFileSourceLevelDim(
                                                absl::string_view description);

  double getDimensionMM(const int64_t adjustedFirstLevelDim,
                        const double firstLevelMpp);

  void getSlideDownSamplingLevels(std::vector<DownsamplingSlideState> *state,
                                  SlideLevelDim *startPyramidCreationDim);

  // level = downsampled slide level to return. level < 0 forces to return
  // dimensions of largest level, 0
  std::unique_ptr<SlideLevelDim> getSlideLevelDim(
                                      int64_t downsample,
                                      SlideLevelDim *priorLevel);

  int32_t getOpenslideLevelForDownsample(int64_t downsample);

  int dicomizeTiff();
  void checkArguments();

 private:
  WsiRequest *wsiRequest_;
  bool retile_;
  int64_t initialX_;
  int64_t initialY_;
  int64_t largestSlideLevelWidth_;
  int64_t largestSlideLevelHeight_;
  int32_t svsLevelCount_;
  std::unique_ptr<OpenSlidePtr> osptr_;
  std::unique_ptr<TiffFile> tiffFile_;

  openslide_t* getOpenSlidePtr();
  void clearOpenSlidePtr();
};

}  // namespace wsiToDicomConverter
#endif  // SRC_WSITODCM_H_
