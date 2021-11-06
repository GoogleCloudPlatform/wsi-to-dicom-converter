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

#ifndef SRC_RAWCOMPRESSION_H_
#define SRC_RAWCOMPRESSION_H_
#include <memory>
#include "src/compressor.h"

// Implementation of Compressor for raw data
class RawCompression : public Compressor {
 public:
  virtual std::unique_ptr<uint8_t[]> compress(
                            const boost::gil::rgb8_view_t& view, size_t* size);

  // Returns data without compression from rgb view
  std::unique_ptr<uint8_t[]> getRawData(const boost::gil::rgb8_view_t& view,
                                        size_t* size);
};

#endif  // SRC_RAWCOMPRESSION_H_
