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
class WsiToDcm {
 private:
  static int dicomizeTiff(std::string inputFile, std::string outputFileMask,
                          int64_t frameSizeX, int64_t frameSizeY,
                          DCM_Compression compression, int32_t quality,
                          int32_t startOnLevel, int32_t stopOnLevel,
                          std::string imageName, std::string studyId,
                          std::string seriesId, std::string jsonFile,
                          int32_t retileLevels, std::vector<double> downsamples,
                          bool tiled, int32_t batchLimit, int8_t threads);

  static void checkArguments(std::string inputFile, std::string outputFileMask,
                             int64_t frameSizeX, int64_t frameSizeY,
                             DCM_Compression compression, int32_t quality,
                             int32_t startOnLevel, int32_t stopOnLevel,
                             std::string imageName, std::string studyId,
                             std::string seriesId, int32_t retileLevels,
                             std::vector<double> downsamples, bool tiled,
                             int32_t batchLimit, int8_t threads, bool debug);

 public:
  static int wsi2dcm(std::string inputFile, std::string outputFileMask,
                     int64_t frameSizeX, int64_t frameSizeY,
                     std::string compression, int32_t quality,
                     int32_t startOnLevel, int32_t stopOnLevel,
                     std::string imageName, std::string studyId,
                     std::string seriesId, int32_t retileLevels,
                     double* downsamples, bool tiled, int32_t batchLimit,
                     int8_t threads, bool debug);

  static int wsi2dcm(std::string inputFile, std::string outputFileMask,
                     int64_t frameSizeX, int64_t frameSizeY,
                     std::string compression, int32_t quality,
                     int32_t startOnLevel, int32_t stopOnLevel,
                     std::string imageName, std::string studyId,
                     std::string seriesId, std::string jsonFile,
                     int32_t retileLevels, double* downsamples, bool tiled,
                     int32_t batchLimit, int8_t threads, bool debug);

  static int wsi2dcm(std::string inputFile, std::string outputFileMask,
                     int64_t frameSizeX, int64_t frameSizeY,
                     std::string compression, int32_t quality,
                     int32_t startWith, int32_t stopOnLevel, bool tiled,
                     int32_t batchLimit, int8_t threads);
  WsiToDcm();
};
}  // namespace wsiToDicomConverter
#endif  // SRC_WSITODCM_H_
