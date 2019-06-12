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

#ifndef RAWCOMPRESSION_H
#define RAWCOMPRESSION_H
#include "compressor.h"

class RawCompression : public Compressor {
 public:
  RawCompression();
  void compress(uint8_t*& output, const boost::gil::rgb8_view_t& view,
                size_t& size);
  void getRawData(uint8_t*& output, const boost::gil::rgb8_view_t& view,
                  size_t& size);
};

#endif  // RAWCOMPRESSION_H
