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
             int quality, bool storeRawBytes) : locationX_(locationX),
                                                  locationY_(locationY),
                                                  frameWidth_(frameWidth),
                                                  frameHeight_(frameHeight),
                                            storeRawBytes_(storeRawBytes) {
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

std::string Frame::photoMetrInt() const {
  return "";
}

int64_t Frame::frameWidth() const {
  return frameWidth_;
}

int64_t Frame::frameHeight() const {
  return frameHeight_;
}

int64_t Frame::locationX() const {
    return locationX_;
}

int64_t Frame::locationY() const {
    return locationY_;
}

void Frame::incReadCounter() {
    readCounter_ += 1;
}

bool Frame::isDone() const {
    return done_;
}

void Frame::clearDicomMem() {
  data_ = nullptr;
}

void Frame::decReadCounter() {
    boost::lock_guard<boost::mutex> guard(readCounterMutex_);
    readCounter_ -= 1;
    if (readCounter_ <= 0) {
      clearRawABGRMem();
    }
  }

int64_t Frame::rawABGRFrameBytes(uint8_t *rawMemory, int64_t memorySize) {
  int64_t memSize =  decompress_memory(rawCompressedBytes_.get(),
                           rawCompressedBytesSize_,
                           rawMemory, memorySize);
  decReadCounter();
  return memSize;
}

void Frame::clearRawABGRMem() {
  if (rawCompressedBytes_ != nullptr) {
    rawCompressedBytes_ = nullptr;
    rawCompressedBytesSize_ = 0;
  }
}

uint8_t *Frame::dicomFrameBytes() {
  return data_.get();
}

size_t Frame::dicomFrameBytesSize() const { return size_; }

bool Frame::hasRawABGRFrameBytes() const {
  return (rawCompressedBytes_ != nullptr && rawCompressedBytesSize_ > 0);
}

}  // namespace wsiToDicomConverter
