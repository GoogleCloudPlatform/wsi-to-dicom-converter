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

#include "src/jpegCompression.h"
#include <boost/bind/bind.hpp>
#include <boost/gil/extension/io/jpeg/old.hpp>
#include <algorithm>
#include <utility>
#include <vector>

JpegCompression::JpegCompression(int quality) {
  _quality = quality;
  _cinfo.err = jpeg_std_error(&_jerr);
  jpeg_create_compress(&_cinfo);
}

JpegCompression::~JpegCompression() { jpeg_destroy_compress(&_cinfo); }

std::unique_ptr<uint8_t[]> JpegCompression::compress(
    const boost::gil::rgb8_view_t &view, size_t *size) {
  typedef typename boost::gil::channel_type<
      typename boost::gil::rgb8_view_t::value_type>::type channel_t;
  _cinfo.image_width = (JDIMENSION)view.width();
  _cinfo.image_height = (JDIMENSION)view.height();
  _cinfo.input_components =
      boost::gil::num_channels<boost::gil::rgb8_view_t>::value;
  _cinfo.in_color_space = boost::gil::detail::jpeg_write_support<
      channel_t, typename boost::gil::color_space_type<
                     boost::gil::rgb8_view_t>::type>::_color_space;

  size_t outlen = 0;
  unsigned char *imgd = 0;
  jpeg_mem_dest(&_cinfo, &imgd, &outlen);
  jpeg_set_defaults(&_cinfo);
  jpeg_set_quality(&_cinfo, _quality, TRUE);
  jpeg_start_compress(&_cinfo, TRUE);
  std::vector<boost::gil::pixel<
      uint8_t, boost::gil::layout<typename boost::gil::color_space_type<
                   boost::gil::rgb8_view_t>::type> > >
      row(view.width());
  JSAMPLE *row_address = reinterpret_cast<JSAMPLE *>(&row.front());
  for (int y = 0; y < view.height(); ++y) {
    std::copy(view.row_begin(y), view.row_end(y), row.begin());
    jpeg_write_scanlines(&_cinfo, (JSAMPARRAY)&row_address, 1);
  }
  jpeg_finish_compress(&_cinfo);
  std::unique_ptr<uint8_t[]> output = std::make_unique<uint8_t[]>(outlen);
  std::move(imgd, imgd + outlen, output.get());
  jpeg_destroy_compress(&_cinfo);
  *size = outlen;
  return output;
}
