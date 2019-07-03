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

#ifndef SRC_JPEGCOMPRESSION_H_
#define SRC_JPEGCOMPRESSION_H_
#include <boost/gil/extension/io/jpeg.hpp>
#include "src/compressor.h"
class JpegCompression : public Compressor {
  jpeg_compress_struct _cinfo;
  jpeg_error_mgr _jerr;
  int _quality;

 public:
  explicit JpegCompression(const int quality);
  ~JpegCompression();
  void compress(const boost::gil::rgb8_view_t& view, uint8_t** output,
                size_t* size);
};
#endif  // SRC_JPEGCOMPRESSION_H_
