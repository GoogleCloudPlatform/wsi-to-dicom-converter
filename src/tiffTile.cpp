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
#include <memory>
#include <utility>

#include "src/tiffTile.h"

namespace wsiToDicomConverter {

TiffTile::TiffTile(TiffDirectory* tiffDirectory, uint64_t tileIndex,
                   std::unique_ptr<uint8_t[]> rawBuffer, uint64_t bufferSize) {
  tiffDirectory_ = tiffDirectory;
  tileIndex_ = tileIndex;
  rawBuffer_ = std::move(rawBuffer);
  rawBufferSize_ = bufferSize;
}

}  // namespace wsiToDicomConverter
