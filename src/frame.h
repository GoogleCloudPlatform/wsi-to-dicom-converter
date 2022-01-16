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

#ifndef SRC_FRAME_H_
#define SRC_FRAME_H_
#include <boost/thread/mutex.hpp>

#include <atomic>
#include <string>
#include <memory>

#include "src/enums.h"
#include "src/compressor.h"

namespace wsiToDicomConverter {

// Frame represents a single image frame from the OpenSlide library
class Frame {
 public:
  Frame(int64_t locationX, int64_t locationY, int64_t frameWidth,
        int64_t frameHeight, DCM_Compression compression, int quality,
        bool storeRawBytes);
  virtual ~Frame() {}

  // Gets frame by openslide library, performs scaling it and compressing
  virtual void sliceFrame() = 0;
  virtual bool isDone() const;
  virtual uint8_t *dicomFrameBytes();
  virtual size_t dicomFrameBytesSize() const;
  virtual void incReadCounter();
  virtual void decReadCounter();
  virtual int64_t rawABGRFrameBytes(uint8_t *rawMemory, int64_t memorySize);
  virtual int64_t frameWidth() const;
  virtual int64_t frameHeight() const;
  virtual void clearDicomMem();
  virtual void clearRawABGRMem();
  virtual bool hasRawABGRFrameBytes() const;
  virtual void incSourceFrameReadCounter() = 0;
  virtual int64_t locationX() const;
  virtual int64_t locationY() const;
  virtual std::string photoMetrInt() const;

 protected:
  std::atomic_bool done_;
  std::unique_ptr<uint8_t[]> data_;
  const int64_t locationX_;
  const int64_t locationY_;
  size_t size_;
  const int64_t frameWidth_;
  const int64_t frameHeight_;
  boost::mutex readCounterMutex_;
  std::atomic_int readCounter_;

  std::unique_ptr<Compressor> compressor_;

  // flag indicates if raw frame bytes should be retained.
  // required for to enable progressive downsampling.
  const bool storeRawBytes_;

  std::unique_ptr<uint8_t[]> rawCompressedBytes_;
  int64_t rawCompressedBytesSize_;
};

}  // namespace wsiToDicomConverter

#endif  // SRC_FRAME_H_
