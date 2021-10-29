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
#include <memory>
#include "src/compressor.h"

// Implementation of Compressor for jpeg
class JpegCompression : public Compressor {
 public:
  explicit JpegCompression(const int quality);
  virtual ~JpegCompression();

  // Gets Raw data from rgb view and performs compression on it
  virtual std::unique_ptr<uint8_t[]> compress(
                            const boost::gil::rgb8_view_t& view, size_t* size);

 private:
  jpeg_compress_struct _cinfo;
  jpeg_error_mgr _jerr;
  int _quality;
};
#endif  // SRC_JPEGCOMPRESSION_H_
