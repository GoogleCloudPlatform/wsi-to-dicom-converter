// Copyright 2021 Google LLC
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
#include <boost/thread.hpp>

#include <atomic>
#include <string>

#include "src/enums.h"
#include "src/frame.h"
#include "src/jpeg2000Compression.h"
#include "src/jpegCompression.h"
#include "src/rawCompression.h"
#include "src/zlibWrapper.h"

namespace wsiToDicomConverter {

Frame::Frame(int64_t locationX, int64_t locationY, int64_t frameWidth,
             int64_t frameHeight, DCM_Compression compression,
             int quality, bool store_raw_bytes) : locationX_(locationX),
                                                  locationY_(locationY),
                                                  frameWidth_(frameWidth),
                                                  frameHeight_(frameHeight),
                                            storeRawBytes_(store_raw_bytes) {
    done_ = false;
    readCounter_ = 0;
    size_ = 0;
    rawCompressedBytes_ = nullptr;
    rawCompressedBytesSize_ = 0;
    data_ = nullptr;
    switch (compression) {
        case JPEG:
            compressor_ = std::make_unique<JpegCompression>(quality);
        break;
        case JPEG2000:
            compressor_ = std::make_unique<Jpeg2000Compression>();
        break;
        case NONE:
          compressor_ = nullptr;
        break;
        default:
            compressor_ = std::make_unique<RawCompression>();
        break;
    }
}

std::string Frame::getPhotoMetrInt() const {
  return "";
}

int64_t Frame::get_frame_width() const {
  return frameWidth_;
}

int64_t Frame::get_frame_height() const {
  return frameHeight_;
}

int64_t Frame::getLocationX() const {
    return locationX_;
}

int64_t Frame::getLocationY() const {
    return locationY_;
}

void Frame::inc_read_counter() {
    readCounter_ += 1;
}

bool Frame::isDone() const {
    return done_;
}

void Frame::clear_dicom_mem() {
  data_ = nullptr;
}

void Frame::dec_read_counter() {
    boost::lock_guard<boost::mutex> guard(readCounterMutex_);
    readCounter_ -= 1;
    if (readCounter_ <= 0) {
      clearRawABGRMem();
    }
  }

int64_t Frame::rawABGRFrameBytes(uint8_t *raw_memory,
                                                    int64_t memorysize) {
  int64_t memsize =  decompress_memory(rawCompressedBytes_.get(),
                           rawCompressedBytesSize_,
                           raw_memory, memorysize);
  dec_read_counter();
  return memsize;
}

void Frame::clearRawABGRMem() {
  if (rawCompressedBytes_ != nullptr) {
    rawCompressedBytes_ = nullptr;
    rawCompressedBytesSize_ = 0;
  }
}

uint8_t *Frame::get_dicom_frame_bytes() {
  return data_.get();
}

size_t Frame::getSize() const { return size_; }

bool Frame::has_compressed_raw_bytes() const {
  return (rawCompressedBytes_ != nullptr && rawCompressedBytesSize_ > 0);
}

}  // namespace wsiToDicomConverter
