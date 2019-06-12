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

#include "frame.h"
#include <dcmtk/dcmdata/libi2d/i2dimgs.h>
#include <boost/gil/extension/numeric/affine.hpp>
#include <boost/gil/extension/numeric/resample.hpp>
#include <boost/gil/extension/numeric/sampler.hpp>
#include <boost/gil/typedefs.hpp>
#include <boost/log/trivial.hpp>
#include "jpeg2000Compression.h"
#include "jpegCompression.h"
#include "rawCompression.h"

using namespace boost::gil;
using namespace std;
Frame::Frame(openslide_t *osr, int64_t locationX, int64_t locationY,
             int32_t level, int64_t frameWidhtDownsampled,
             int64_t frameHeightDownsampled, double multiplicator,
             int64_t frameWidht, int64_t frameHeight,
             DCM_Compression compression, int quality) {
  done_ = false;
  osr_ = osr;
  locationX_ = locationX;
  locationY_ = locationY;
  level_ = level;
  frameWidhtDownsampled_ = frameWidhtDownsampled;
  frameHeightDownsampled_ = frameHeightDownsampled;
  multiplicator_ = multiplicator;
  frameWidht_ = frameWidht;
  frameHeight_ = frameHeight;
  switch (compression) {
    case JPEG:
      compressor_ = new JpegCompression(quality);
      break;
    case JPEG2000:
      compressor_ = new Jpeg2000Compression();
      break;
    default:
      compressor_ = new RawCompression();
  }
}

Frame::~Frame() {
  free(data_);
  delete compressor_;
}

class convert_rgba_to_rgb {
 public:
  void operator()(const rgba8c_pixel_t &src, rgb8_pixel_t &dst) const {
    get_color(dst, blue_t()) =
        channel_multiply(get_color(src, red_t()), get_color(src, alpha_t()));
    get_color(dst, green_t()) =
        channel_multiply(get_color(src, green_t()), get_color(src, alpha_t()));
    get_color(dst, red_t()) =
        channel_multiply(get_color(src, blue_t()), get_color(src, alpha_t()));
  }
};

void Frame::sliceFrame() {
  uint32_t *buf =
      (uint32_t *)malloc((size_t)frameWidhtDownsampled_ *
                         (size_t)frameHeightDownsampled_ * (size_t)4);
  openslide_read_region(osr_, buf, (int64_t)(locationX_ * multiplicator_),
                        (int64_t)(locationY_ * multiplicator_), level_,
                        frameWidhtDownsampled_, frameHeightDownsampled_);
  if (openslide_get_error(osr_)) {
    BOOST_LOG_TRIVIAL(error) << openslide_get_error(osr_);
  }
  rgba8c_view_t gil = boost::gil::interleaved_view(
      (uint64_t)frameWidhtDownsampled_, (uint64_t)frameHeightDownsampled_,
      (const rgba8c_pixel_t *)buf, frameWidhtDownsampled_ * 4);

  rgba8_image_t newFrame(frameWidht_, frameHeight_);
  if (frameWidhtDownsampled_ != frameWidht_ ||
      frameHeightDownsampled_ != frameHeight_) {
    boost::gil::resize_view(gil, view(newFrame), nearest_neighbor_sampler());
    gil = view(newFrame);
  }

  rgb8_image_t exp(frameWidht_, frameHeight_);
  rgb8_view_t rgbView = view(exp);
  copy_and_convert_pixels(gil, rgbView, convert_rgba_to_rgb());
  compressor_->compress(data_, rgbView, size_);
  BOOST_LOG_TRIVIAL(debug) << " frame size: " << size_ / 1024 << "kb";
  free(buf);
  done_ = true;
}

bool Frame::isDone() { return done_; }

uint8_t *Frame::getData() { return data_; }

size_t Frame::getSize() { return size_; }
