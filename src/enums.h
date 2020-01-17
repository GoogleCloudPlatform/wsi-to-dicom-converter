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

#ifndef SRC_ENUMS_H_
#define SRC_ENUMS_H_

#include <algorithm>
#include <string>

typedef enum { UNKNOWN = -1, JPEG2000 = 0, JPEG = 1, RAW = 2 } DCM_Compression;

inline DCM_Compression dcmCompressionFromString(std::string compressionStr) {
  DCM_Compression compression = UNKNOWN;
  std::transform(compressionStr.begin(), compressionStr.end(),
                 compressionStr.begin(), ::tolower);
  if (compressionStr.compare("jpeg") == 0) {
    compression = JPEG;
  }
  if (compressionStr.compare("jpeg2000") == 0) {
    compression = JPEG2000;
  }
  if (compressionStr.compare("none") == 0 ||
      compressionStr.compare("raw") == 0) {
    compression = RAW;
  }
  return compression;
}

#endif  // SRC_ENUMS_H_
