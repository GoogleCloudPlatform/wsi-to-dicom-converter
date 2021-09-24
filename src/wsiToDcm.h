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

#include <iostream>
#include <string>
#include <vector>

#include "src/enums.h"

namespace wsiToDicomConverter {

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
  int* downsamples;

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

  // crop frame so downsampleing generates uniform pixel spacing.
  bool cropFrameToGenerateUniformPixelSpacing = false;
};

// Contains static methods for generation DICOM files
// from wsi images supported by openslide
class WsiToDcm {
 public:
  // Performs file checks and generation of tasks
  // for generation of frames and DICOM files
  static int wsi2dcm(WsiRequest wsiRequest);
  WsiToDcm();

 private:
  // Generates tasks and handling thread pool
  static int dicomizeTiff(const std::string &inputFile,
                          const std::string &outputFileMask,
                          const int64_t frameSizeX,
                          const int64_t frameSizeY,
                          const DCM_Compression compression,
                          const int32_t quality,
                          const  int32_t startOnLevel,
                          const  int32_t stopOnLevel,
                          const std::string &imageName,
                          std::string studyId,
                          std::string seriesId,
                          const std::string &jsonFile,
                          const int32_t retileLevels,
                          const std::vector<int> &downsamples,
                          const bool tiled, const int32_t batchLimit,
                          const int8_t threads,
                          const bool dropFirstRowAndColumn,
                          const bool stopDownSamplingAtSingleFrame,
                          const bool useBilinearDownsampling,
                          const bool floorCorrectDownsampling,
                          const bool preferProgressiveDownsampling,
                          const bool cropFrameToGenerateUniformPixelSpacing);

  static void checkArguments(WsiRequest wsiRequest);
};

}  // namespace wsiToDicomConverter
#endif  // SRC_WSITODCM_H_
