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
#include <dcmtk/dcmdata/libi2d/i2dimgs.h>

#include <boost/gil/extension/numeric/affine.hpp>
#include <boost/gil/extension/numeric/resample.hpp>
#include <boost/gil/extension/numeric/sampler.hpp>
#include <boost/gil/typedefs.hpp>
#include <boost/log/trivial.hpp>

#include <utility>

#include "src/dicom_file_region_reader.h"
#include "src/jpeg2000Compression.h"
#include "src/jpegCompression.h"
#include "src/nearestneighborframe.h"
#include "src/rawCompression.h"
#include "src/zlibWrapper.h"

namespace wsiToDicomConverter {

NearestNeighborFrame::NearestNeighborFrame(
    openslide_t *osr, int64_t locationX, int64_t locationY, int64_t level,
    int64_t frameWidthDownsampled, int64_t frameHeightDownsampled,
    double multiplicator, int64_t frameWidth, int64_t frameHeight,
    DCM_Compression compression, int quality, bool store_raw_bytes,
    const DICOMFileFrameRegionReader &frame_region_reader) :
    dcm_frame_region_reader(frame_region_reader) {
  done_ = false;
  osr_ = osr;
  locationX_ = locationX;
  locationY_ = locationY;
  level_ = level;
  frameWidthDownsampled_ = frameWidthDownsampled;
  frameHeightDownsampled_ = frameHeightDownsampled;
  multiplicator_ = multiplicator;
  frameWidth_ = frameWidth;
  frameHeight_ = frameHeight;
  // flag indicates if raw frame bytes should be retained.
  // required for to enable progressive downsampling.
  store_raw_bytes_ = store_raw_bytes;
  switch (compression) {
    case JPEG:
      compressor_ = std::make_unique<JpegCompression>(quality);
      break;
    case JPEG2000:
      compressor_ = std::make_unique<Jpeg2000Compression>();
      break;
    default:
      compressor_ = std::make_unique<RawCompression>();
  }
}

NearestNeighborFrame::~NearestNeighborFrame() {}

int64_t NearestNeighborFrame::get_frame_width() const {
  return frameWidth_;
}

int64_t NearestNeighborFrame::get_frame_height() const {
  return frameHeight_;
}

class convert_rgba_to_rgb {
 public:
  void operator()(
      const boost::gil::rgba8c_pixel_t &src,
      boost::gil::rgb8_pixel_t &dst) const {  // NOLINT, boost template
    boost::gil::get_color(dst, boost::gil::blue_t()) =
        boost::gil::channel_multiply(get_color(src, boost::gil::red_t()),
                                     get_color(src, boost::gil::alpha_t()));
    boost::gil::get_color(dst, boost::gil::green_t()) =
        boost::gil::channel_multiply(get_color(src, boost::gil::green_t()),
                                     get_color(src, boost::gil::alpha_t()));
    boost::gil::get_color(dst, boost::gil::red_t()) =
        boost::gil::channel_multiply(get_color(src, boost::gil::blue_t()),
                                     get_color(src, boost::gil::alpha_t()));
  }
};

void NearestNeighborFrame::sliceFrame() {
  std::unique_ptr<uint32_t[]>buf =
                          std::make_unique<uint32_t[]>(frameWidthDownsampled_ *
                                                      frameHeightDownsampled_);
  if (dcm_frame_region_reader.dicom_file_count() == 0) {
    openslide_read_region(osr_, buf.get(), static_cast<int64_t>(locationX_ *
                                                          multiplicator_),
                          static_cast<int64_t>(locationY_ * multiplicator_),
                          level_, frameWidthDownsampled_,
                          frameHeightDownsampled_);
    if (openslide_get_error(osr_)) {
      BOOST_LOG_TRIVIAL(error) << openslide_get_error(osr_);
      throw 1;
    }
  } else {
    dcm_frame_region_reader.read_region(locationX_, locationY_,
                                        frameWidthDownsampled_,
                                        frameHeightDownsampled_,
                                        buf.get());
  }
  boost::gil::rgba8c_view_t gil = boost::gil::interleaved_view(
              frameWidthDownsampled_,
              frameHeightDownsampled_,
              reinterpret_cast<const boost::gil::rgba8c_pixel_t *>(buf.get()),
              frameWidthDownsampled_ * sizeof(uint32_t));

  boost::gil::rgba8_image_t newFrame(frameWidth_, frameHeight_);
  if (frameWidthDownsampled_ != frameWidth_ ||
      frameHeightDownsampled_ != frameHeight_) {
    boost::gil::resize_view(gil, view(newFrame),
                            boost::gil::nearest_neighbor_sampler());
    gil = view(newFrame);
  }

  boost::gil::rgb8_image_t exp(frameWidth_, frameHeight_);
  boost::gil::rgb8_view_t rgbView = view(exp);

  // Create a copy of the pre-compressed downsampled bits
  if (!store_raw_bytes_) {
    raw_compressed_bytes_size_ = 0;
    raw_compressed_bytes_ = NULL;
  } else {
    const int64_t frame_mem_size = frameWidth_ * frameHeight_;
    std::unique_ptr<uint32_t[]>  raw_bytes = std::make_unique<uint32_t[]>(
                                          static_cast<size_t>(frame_mem_size));
    boost::gil::rgba8_view_t raw_byte_view = boost::gil::interleaved_view(
        frameWidth_, frameHeight_,
        reinterpret_cast<boost::gil::rgba8_pixel_t *>(raw_bytes.get()),
        frameWidth_ * sizeof(uint32_t));
    boost::gil::copy_pixels(gil, raw_byte_view);

    raw_compressed_bytes_ = std::move(compress_memory(
                reinterpret_cast<uint8_t*>(raw_bytes.get()), frame_mem_size *
                              sizeof(uint32_t), &raw_compressed_bytes_size_));
    BOOST_LOG_TRIVIAL(debug) << " compressed raw frame size: " <<
                                  raw_compressed_bytes_size_ / 1024 << "kb";
  }

  boost::gil::copy_and_convert_pixels(gil, rgbView, convert_rgba_to_rgb());
  data_ = compressor_->compress(rgbView, &size_);
  BOOST_LOG_TRIVIAL(debug) << " frame size: " << size_ / 1024 << "kb";
  done_ = true;
}

int64_t NearestNeighborFrame::get_raw_frame_bytes(uint8_t *raw_memory,
                                                  int64_t memorysize) const {
  return decompress_memory(raw_compressed_bytes_.get(),
                           raw_compressed_bytes_size_, raw_memory, memorysize);
}

bool NearestNeighborFrame::isDone() const { return done_; }

void NearestNeighborFrame::clear_dicom_mem() {
  data_ = NULL;
}

uint8_t *NearestNeighborFrame::get_dicom_frame_bytes() {
  return data_.get();
}

size_t NearestNeighborFrame::getSize() const { return size_; }

bool NearestNeighborFrame::has_compressed_raw_bytes() const {
  return (raw_compressed_bytes_ != NULL && raw_compressed_bytes_size_ > 0);
}

}  // namespace wsiToDicomConverter
