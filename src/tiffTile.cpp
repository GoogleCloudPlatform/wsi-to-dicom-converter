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

TiffTile::TiffTile(const TiffDirectory* tiffDirectory,
                  const uint64_t tileIndex,
                  std::unique_ptr<uint8_t[]> rawBuffer,
                  const uint64_t bufferSize) :
                  tiffDirectory_(tiffDirectory), tileIndex_(tileIndex),
                  rawBufferSize_(bufferSize) {
  rawBuffer_ = std::move(rawBuffer);
}

TiffTile::~TiffTile() {
}

uint64_t TiffTile::index() const {
  return tileIndex_;
}

const TiffDirectory * TiffTile::directory() const {
  return tiffDirectory_;
}
uint64_t TiffTile::rawBufferSize() const {
  return rawBufferSize_;
}

const uint8_t* TiffTile::rawBuffer() const {
  return rawBuffer_.get();
}

std::unique_ptr<uint8_t[]> TiffTile::getRawBuffer() {
  return std::move(rawBuffer_);
}

}  // namespace wsiToDicomConverter
