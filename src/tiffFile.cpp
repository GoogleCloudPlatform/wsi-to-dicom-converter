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
#include <boost/thread.hpp>

#include <memory>
#include <string>
#include <utility>

#include "src/tiffFile.h"


namespace wsiToDicomConverter {

TiffFile::TiffFile(const std::string &path) {
  tiffFile_ = TIFFOpen(path.c_str(), "r");
  if (tiffFile_ == nullptr) {
      return;
  }
  do {
    // Uncomment to print description of tiff dir to stdio.
    // TIFFPrintDirectory(tiffFile_, stdout);
    tiffDir_.push_back(std::move(std::make_unique<TiffDirectory>(tiffFile_)));
  } while (TIFFReadDirectory(tiffFile_));
  currentDirectoryIndex_ = 0;
  tiffFilePath_ = path;
  TIFFSetDirectory(tiffFile_, currentDirectoryIndex_);
  tileReadBufSize_ = TIFFTileSize(tiffFile_);
  tileReadBuffer_ = _TIFFmalloc(tileReadBufSize_);
}

TiffFile::~TiffFile() {
  if (!isLoaded()) {
    return;
  }
  if (tileReadBuffer_  != nullptr) {
    _TIFFfree(tileReadBuffer_);
  }
  TIFFClose(tiffFile_);
}

bool TiffFile::isLoaded() {
  return (tiffFile_ != nullptr);
}

bool TiffFile::hasExtractablePyramidImages() {
  for (int32_t idx = 0; idx < tiffDir_.size(); ++idx) {
    if (tiffDir_[idx]->isExtractablePyramidImage()) {
      return true;
    }
  }
  return false;
}

int32_t TiffFile::getDirectoryIndexMatchingImageDimensions(uint32_t width,
                                                           uint32_t height,
                                            bool isExtractablePyramidImage) {
  for (int32_t idx = 0; idx < tiffDir_.size(); ++idx) {
    if (!isExtractablePyramidImage ||
        tiffDir_[idx]->isExtractablePyramidImage()) {
      if (tiffDir_[idx]->doImageDimensionsMatch(width, height)) {
        return idx;
      }
    }
  }
  return -1;
}

const TiffDirectory *TiffFile::directory(int64_t dirIndex) const {
  return tiffDir_[dirIndex].get();
}

uint32_t TiffFile::directoryCount() {
  return tiffDir_.size();
}

std::unique_ptr<TiffTile> TiffFile::tile(int32_t dirIndex,
                                         uint32_t tileIndex) {
  std::unique_ptr<uint8_t[]>  mem_buffer;
  uint32_t bufferSize;
  {
    boost::lock_guard<boost::mutex> guard(tiffReadMutex_);
    if (currentDirectoryIndex_ != dirIndex) {
      currentDirectoryIndex_ = dirIndex;
      TIFFSetDirectory(tiffFile_, currentDirectoryIndex_);
    }
    bufferSize = TIFFReadRawTile(tiffFile_, static_cast<ttile_t>(tileIndex),
                                 tileReadBuffer_, tileReadBufSize_);
    if (bufferSize == 0) {
      return nullptr;
    }
    mem_buffer = std::make_unique<uint8_t[]>(bufferSize);
    std::memcpy(mem_buffer.get(), tileReadBuffer_, bufferSize);
  }
  return std::make_unique<TiffTile>(directory(dirIndex), tileIndex,
                                    std::move(mem_buffer), bufferSize);
}

}  // namespace wsiToDicomConverter
