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
#include <absl/strings/string_view.h>
#include <boost/log/trivial.hpp>
#include <memory>
#include <string>
#include <utility>

#include "src/openslideUtil.h"
#include "src/tiffFile.h"


namespace wsiToDicomConverter {

TiffFile::TiffFile(absl::string_view path, const int32_t dirIndex) :
  tiffFilePath_(path), currentDirectoryIndex_(dirIndex) {
  initalized_ = false;
  tileReadBufSize_  = 0;
  openslide_level_ = 0;
  std::string path_str = std::move(static_cast<std::string>(path));
  tiffFile_ = TIFFOpen(path_str.c_str(), "r");
  if (tiffFile_ == nullptr) {
      return;
  }
  do {
    // Uncomment to print description of tiff dir to stdio.
    // TIFFPrintDirectory(tiffFile_, stdout);
    tiffDir_.push_back(std::move(std::make_unique<TiffDirectory>(tiffFile_)));
  } while (TIFFReadDirectory(tiffFile_));
  TIFFSetDirectory(tiffFile_, currentDirectoryIndex_);
  tileReadBufSize_ = TIFFTileSize(tiffFile_);
  initalized_ = true;
}

TiffFile::TiffFile(const TiffFile &tf, const int32_t dirIndex) :
    tiffFilePath_(tf.path()), currentDirectoryIndex_(dirIndex) {
    initalized_ = false;
    tileReadBufSize_  = 0;
    openslide_level_ = 0;
    tiffFile_ = TIFFOpen(tiffFilePath_.c_str(), "r");
    if (tiffFile_ == nullptr) {
      return;
    }
    const size_t dirCount = tf.directoryCount();
    for (size_t idx = 0; idx < dirCount; ++idx) {
      tiffDir_.push_back((std::move(
                      std::make_unique<TiffDirectory>(*tf.directory(idx)))));
    }
    TIFFSetDirectory(tiffFile_, currentDirectoryIndex_);
    tileReadBufSize_ = tf.tileReadBufSize_;
    if (!fileDirectory()->isJpeg2kCompressed()) {
      osptr_ = nullptr;
      openslide_level_ = 0;
    } else {
      osptr_ = std::make_unique<OpenSlidePtr>(tf.path());
      openslide_t* opslide = osptr_->osr();
      int64_t level_width = -1;
      int64_t level_height = -1;
      const int64_t targetWidth = fileDirectory()->imageWidth();
      const int64_t targetHeight = fileDirectory()->imageHeight();
      if (openslide_get_error(opslide)) {
        BOOST_LOG_TRIVIAL(error) << openslide_get_error(opslide);
        throw 1;
      }
      const int32_t level_count = openslide_get_level_count(opslide);
      for (openslide_level_ = 0;
           openslide_level_ < level_count;
           ++openslide_level_) {
        openslide_get_level_dimensions(opslide, openslide_level_,
                                       &level_width, &level_height);
        if (level_width == targetWidth && level_height == targetHeight) {
          break;
        }
      }
      if (level_width != targetWidth && level_height != targetHeight) {
        BOOST_LOG_TRIVIAL(error) << "Could not find expected level in "
                                    "JPG2000 encoded tiff.";
        throw 1;
      }
    }
    initalized_ = true;
}

openslide_t * TiffFile::getOpenslidePtr() {
  return osptr_->osr();
}

int32_t TiffFile::getOpenslideLevel() const {
  return openslide_level_;
}

TiffFile::~TiffFile() {
  osptr_ = nullptr;
  close();
}

void TiffFile::close() {
  if (tiffFile_ == nullptr) {
    return;
  }
  TIFFClose(tiffFile_);
  tiffFile_ = nullptr;
}

std::string TiffFile::path() const {
  return tiffFilePath_;
}

int32_t TiffFile::directoryLevel() const {
  return currentDirectoryIndex_;
}

bool TiffFile::isLoaded() const {
  return (tiffFile_ != nullptr);
}

bool TiffFile::isInitalized() const {
  return initalized_;
}

bool TiffFile::hasExtractablePyramidImages() const {
  for (int32_t idx = 0; idx < tiffDir_.size(); ++idx) {
    if (tiffDir_[idx]->isExtractablePyramidImage()) {
      return true;
    }
  }
  return false;
}

int32_t TiffFile::getDirectoryIndexMatchingImageDimensions(uint32_t width,
                                                           uint32_t height,
                                        bool isExtractablePyramidImage) const {
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

const TiffDirectory *TiffFile::fileDirectory() const {
  return directory(currentDirectoryIndex_);
}

const TiffDirectory *TiffFile::directory(int64_t dirIndex) const {
  return tiffDir_[dirIndex].get();
}

uint32_t TiffFile::directoryCount() const {
  return tiffDir_.size();
}

class TileReadBuffer {
 public:
  explicit TileReadBuffer(uint64_t size);
  virtual ~TileReadBuffer();
  tdata_t buffer_;
};

TileReadBuffer::TileReadBuffer(uint64_t size) {
  buffer_ = _TIFFmalloc(size);
}

TileReadBuffer::~TileReadBuffer() {
  if (buffer_ != nullptr) {
    _TIFFfree(buffer_);
  }
}

std::unique_ptr<TiffTile> TiffFile::tile(uint32_t tileIndex) {
  if (tiffFile_ == nullptr) {
    return nullptr;
  }
  TileReadBuffer readBuffer(tileReadBufSize_);
  if (readBuffer.buffer_ == nullptr) {
    return nullptr;
  }
  uint32_t bufferSize = TIFFReadRawTile(tiffFile_,
                                        static_cast<ttile_t>(tileIndex),
                                        readBuffer.buffer_,
                                        tileReadBufSize_);
  if (bufferSize == 0) {
    return nullptr;
  }
  std::unique_ptr<uint8_t[]> mem_buffer =
                                       std::make_unique<uint8_t[]>(bufferSize);
  if (mem_buffer == nullptr) {
    return nullptr;
  }
  _TIFFmemcpy(mem_buffer.get(), readBuffer.buffer_, bufferSize);
  return std::make_unique<TiffTile>(directory(directoryLevel()), tileIndex,
                                    std::move(mem_buffer), bufferSize);
}

}  // namespace wsiToDicomConverter
