// Copyright 2022 Google LLC
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

#ifndef SRC_TIFFTILE_H_
#define SRC_TIFFTILE_H_

#include <memory>

#include "src/tiffDirectory.h"

namespace wsiToDicomConverter {

class TiffTile {
 public:
  TiffTile(TiffDirectory* tiffDirectory, uint64_t tileIndex,
           std::unique_ptr<uint8_t[]> rawBuffer, uint64_t bufferSize);

  std::unique_ptr<uint8_t[]> rawBuffer_;
  uint64_t rawBufferSize_;
  uint64_t tileIndex_;
  TiffDirectory *tiffDirectory_;
};

}  // namespace wsiToDicomConverter

#endif  // SRC_TIFFTILE_H_
