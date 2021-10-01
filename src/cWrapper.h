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
#include <cstdint>
#ifndef SRC_CWRAPPER_H_
#define SRC_CWRAPPER_H_
#ifdef __cplusplus
extern "C" {
#endif

// Performs file checks and generation of tasks
// for generation of frames and DICOM files
// inputFile - wsi input
// outputFileMask - path to save generated files
// compression - jpeg, jpeg2000, raw
// startOnLevel, stopOnLevel - limitation which levels to generate
// retileLevels - number of levels, levels == 0 means number of
//          levels will be readed from wsi file
// imageName, studyId, seriesId - required DICOM metadata
// downsamples - for each level, downsample is size
//              factor for each level
// tiled - frame organizationtype
//           true:TILED_FULL
//           false:TILED_SPARSE
// batchLimit - maximum frames in one file
int wsi2dcm(const char* inputFile, const char* outputFileMask,
            int64_t frameSizeX, int64_t frameSizeY, const char* compression,
            int quality, int startOnLevel, int stopOnLevel,
            const char* imageName, const char* studyId, const char* seriesId,
            int retileLevels, int* downsamples, bool tiled, int batchLimit,
            int threads, bool debug, bool stopDownsamplingAtSingleFrame,
            bool bilinearDownsampling, bool floorCorrectDownsampling,
            bool progressiveDownsample,
            bool cropFrameToGenerateUniformPixelSpacing);

// Performs file checks and generation of tasks
// for generation of frames and DICOM files
// jsonFile - json file with additional DICOM metadata
int wsi2dcmJson(const char* inputFile, const char* outputFileMask,
                int64_t frameSizeX, int64_t frameSizeY, const char* compression,
                int quality, int startOnLevel, int stopOnLevel,
                const char* imageName, const char* studyId,
                const char* seriesId, const char* jsonFile, int retileLevels,
                int* downsamples, bool tiled, int batchLimit, int threads,
                bool debug, bool stopDownsamplingAtSingleFrame,
                bool bilinearDownsampling, bool floorCorrectDownsampling,
                bool progressiveDownsample,
                bool cropFrameToGenerateUniformPixelSpacing);
#ifdef __cplusplus
}
#endif

#endif  // SRC_CWRAPPER_H_
