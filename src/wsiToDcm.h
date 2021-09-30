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
#include <openslide.h>

#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "src/enums.h"

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
  bool tiled;

  // maximum frames in one file, as limit is exceeded new files is started
  // eg: 3 files will be generated for batchLimit is 10 and 30 frames in level
  int32_t batchLimit;

  // threads to consume during execution
  int8_t threads = -1;

  // start slicing from point (1,1) instead of (0,0) to avoid bug
  //  https://github.com/openslide/openslide/issues/268
  bool dropFirstRowAndColumn = false;

  // print debug messages: dimensions of levels, size of frames
  bool debug = false;

  // stop downsampling if total layer dimensions < 1 frame
  bool stopDownsamplingAtSingleFrame = false;

  // use bilinear interpolation instead of nearest neighbor
  // interpolation
  bool useBilinearDownsampling = false;

  // floor correct reported openslide downsampling.
  bool floorCorrectDownsampling = false;

  // prefer progressive downsampling.
  bool preferProgressiveDownsampling = false;
};

// Contains static methods for generation DICOM files
// from wsi images supported by openslide
class WsiToDcm {
 public:
  // Performs file checks and generation of tasks
  // for generation of frames and DICOM files
  explicit WsiToDcm(WsiRequest *wsiRequest);
  int wsi2dcm();

 private:
  WsiRequest *wsiRequest_;
  bool retile_;
  openslide_t *osr_;
  int64_t initialX_;
  int64_t initialY_;
  int64_t firstLevelWidth_;
  int64_t firstLevelHeight_;
  int32_t svsLevelCount_;


  // Generates tasks and handling thread pool
  int dicomizeTiff();
  void checkArguments();
  int32_t getOpenslideLevelForDownsample(int64_t downsample);
  std::unique_ptr<SlideLevelDim> getSlideLevelDim(
                                      const int32_t level,
                                      SlideLevelDim *priorLevel,
                                      bool enableProgressiveDownsample = true);

  double  getDownsampledLevelDimensionMM(const int64_t adjustedFirstLevelDim,
                                         const int64_t levelDimDownsampled,
                                         const double downsample,
                                        const char* openSlideLevelDimProperty);
};

}  // namespace wsiToDicomConverter
#endif  // SRC_WSITODCM_H_
