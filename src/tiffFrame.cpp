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
#include <boost/log/trivial.hpp>

#include <string>
#include <utility>

#include "src/jpegUtil.h"
#include "src/tiffDirectory.h"
#include "src/tiffFrame.h"


namespace wsiToDicomConverter {


TiffFrame::TiffFrame(
    TiffFile *tiffFile, int64_t locationX, int64_t locationY, int64_t level,
    int64_t frameWidth, int64_t frameHeight):
    Frame(locationX, locationY, frameWidth, frameHeight, NONE, -1, true) {
  tiffFile_ = tiffFile;
  level_ = level;
}

TiffFrame::~TiffFrame() {}

std::unique_ptr<TiffTile> TiffFrame::getTiffTileData(int64_t *tileWidth,
                                                     int64_t *tileHeight) {
  TiffDirectory *dir = tiffFile_->directory(level_);
  uint64_t tileIndex = ((getLocationY() / dir->tileHeight()) *
                         dir->tilesPerRow()) +
                        (getLocationX() /  dir->tileWidth());

  *tileWidth = dir->tileWidth();
  *tileHeight = dir->tileHeight();
  return std::move(tiffFile_->tile(level_, tileIndex));
}

bool TiffFrame::canDecodeJpeg() {
  if (done_) {
    return true;
  }
  int64_t tileWidth, tileHeight;
  std::unique_ptr<TiffTile> tile =
                          std::move(getTiffTileData(&tileWidth, &tileHeight));
  uint8_t *data = tile->rawBuffer_.get();
  uint64_t rawBufferSize = tile->rawBufferSize_;
  const bool result = jpegUtil::canDecodeJpeg(tileWidth, tileHeight, data,
                                              rawBufferSize);
  if (result) {
    BOOST_LOG_TRIVIAL(info) << "Decoded jpeg in TIFF file.";
    return true;
  }
  BOOST_LOG_TRIVIAL(error) << "Error occured decoding jpeg in TIFF file.";
  return false;
}

std::string TiffFrame::getPhotoMetrInt() const {
  TiffDirectory *dir = tiffFile_->directory(level_);
  if (dir->isPhotoMetricRGB()) {
      return "RGB";
  } else {
      return "YBR_FULL_422";
  }
}

bool TiffFrame::has_compressed_raw_bytes() const {
  return data_ != nullptr;
}

void TiffFrame::clear_raw_mem() {
  // Clear out DICOM memory.
  // DICOM memory and compressed memory are same thing.
  // For TiffFrames.  Data in native form is already compressed.
  // Compressed memory is cleared after dicom memory.
  data_ = nullptr;
}

void TiffFrame::incSourceFrameReadCounter() {
  // Reads from Tiff no source frame counter to increment.
}

void TiffFrame::clear_dicom_mem() {
  // DICOM memory and compressed memory are same thing.
  // For TiffFrames.  Data in native form is already compressed.
  // Compressed memory is cleared after dicom memory.
}

int64_t TiffFrame::get_raw_frame_bytes(uint8_t *raw_memory,
                                      int64_t memorysize) {
  // For tiff frame data in native format is jpeg encoded.
  // to work with just uncompress and return.
  uint64_t abgrBufferSizeRead;
  jpegUtil::decodedJpeg(get_frame_width(), get_frame_height(), data_.get(),
                        size_, &abgrBufferSizeRead, raw_memory, memorysize);
  dec_read_counter();
  return abgrBufferSizeRead;
}

void TiffFrame::sliceFrame() {
  int64_t tileWidth, tileHeight;
  std::unique_ptr<TiffTile> tile = std::move(getTiffTileData(&tileWidth,
                                                             &tileHeight));
  data_ = std::move(tile->rawBuffer_);
  size_ = tile->rawBufferSize_;
  BOOST_LOG_TRIVIAL(debug) << " Tiff extracted frame size: " << size_ /
                                                                1024 << "kb";
  done_ = true;
}

}  // namespace wsiToDicomConverter


