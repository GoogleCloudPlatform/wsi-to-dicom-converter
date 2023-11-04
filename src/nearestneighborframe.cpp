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
#include "src/jpegCompression.h"
#include "src/nearestneighborframe.h"
#include "src/rawCompression.h"
#include "src/zlibWrapper.h"

namespace wsiToDicomConverter {

NearestNeighborFrame::NearestNeighborFrame(
    OpenSlidePtr *osptr, int64_t locationX, int64_t locationY, int64_t level,
    int64_t frameWidthDownsampled, int64_t frameHeightDownsampled,
    double multiplicator, int64_t frameWidth, int64_t frameHeight,
    DCM_Compression compression, int quality, JpegSubsampling sampling,
    bool storeRawBytes,
    DICOMFileFrameRegionReader *frame_region_reader): Frame(locationX,
                                                            locationY,
                                                            frameWidth,
                                                             frameHeight,
                                                        compression, quality,
                                                        sampling,
                                                        storeRawBytes) {
  osptr_ = osptr;
  level_ = level;
  frameWidthDownsampled_ = frameWidthDownsampled;
  frameHeightDownsampled_ = frameHeightDownsampled;
  multiplicator_ = multiplicator;
  dcmFrameRegionReader_ = frame_region_reader;
}

NearestNeighborFrame::~NearestNeighborFrame() {}

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

void NearestNeighborFrame::incSourceFrameReadCounter() {
  if (dcmFrameRegionReader_->dicomFileCount() != 0) {
    dcmFrameRegionReader_->incSourceFrameReadCounter(locationX_, locationY_,
                                        frameWidthDownsampled_,
                                        frameHeightDownsampled_);
  }
}

void NearestNeighborFrame::sliceFrame() {
  std::unique_ptr<uint32_t[]>buf =
                          std::make_unique<uint32_t[]>(frameWidthDownsampled_ *
                                                      frameHeightDownsampled_);
  if (dcmFrameRegionReader_->dicomFileCount() == 0) {
    openslide_read_region(osptr_->osr(), buf.get(),
                          static_cast<int64_t>(locationX_ * multiplicator_),
                          static_cast<int64_t>(locationY_ * multiplicator_),
                          level_, frameWidthDownsampled_,
                          frameHeightDownsampled_);
    if (openslide_get_error(osptr_->osr())) {
      BOOST_LOG_TRIVIAL(error) << openslide_get_error(osptr_->osr());
      throw 1;
    }
  } else {
    if (!dcmFrameRegionReader_->readRegion(locationX_, locationY_,
                                       frameWidthDownsampled_,
                                       frameHeightDownsampled_,
                                       buf.get())) {
      BOOST_LOG_TRIVIAL(error) << "Error occured decoding region from previous"
                                  " level.";
      throw 1;
    }
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
  if (!storeRawBytes_) {
    clearRawABGRMem();
  } else {
    const int64_t frame_mem_size = frameWidth_ * frameHeight_;
    std::unique_ptr<uint32_t[]>  raw_bytes = std::make_unique<uint32_t[]>(
                                          static_cast<size_t>(frame_mem_size));
    boost::gil::rgba8_view_t raw_byte_view = boost::gil::interleaved_view(
        frameWidth_, frameHeight_,
        reinterpret_cast<boost::gil::rgba8_pixel_t *>(raw_bytes.get()),
        frameWidth_ * sizeof(uint32_t));
    boost::gil::copy_pixels(gil, raw_byte_view);

    rawCompressedBytes_ = std::move(compress_memory(
                reinterpret_cast<uint8_t*>(raw_bytes.get()), frame_mem_size *
                              sizeof(uint32_t), &rawCompressedBytesSize_));
  }

  boost::gil::copy_and_convert_pixels(gil, rgbView, convert_rgba_to_rgb());
  uint64_t size;
  std::unique_ptr<uint8_t[]>mem = std::move(compressor_->compress(rgbView,
                                                                  &size));
  setDicomFrameBytes(std::move(mem), size);
  done_ = true;
}

}  // namespace wsiToDicomConverter


