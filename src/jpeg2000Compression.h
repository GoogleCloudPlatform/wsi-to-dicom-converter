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

#ifndef SRC_JPEG2000COMPRESSION_H_
#define SRC_JPEG2000COMPRESSION_H_

#include <memory>
#include "rawCompression.h"

// Implementation of Compressor for JPEG2000
class Jpeg2000Compression : public RawCompression {
 public:
  Jpeg2000Compression() {}

  virtual ~Jpeg2000Compression();

  // Performs compression
  virtual std::unique_ptr<uint8_t[]> writeToMemory(unsigned int width,
                                                   unsigned int height,
                                                   unsigned int pitch,
                                                   uint8_t* buffer,
                                                   size_t* size);

  // Gets Raw data from rgb view and performs compression on it
  virtual std::unique_ptr<uint8_t[]> compress(
                            const boost::gil::rgb8_view_t& view, size_t* size);

 private:
  uint8_t* buffer_;
  size_t size_;
};

#endif  // SRC_JPEG2000COMPRESSION_H_
