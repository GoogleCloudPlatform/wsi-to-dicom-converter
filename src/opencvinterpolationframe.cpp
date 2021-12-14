// Copyright 2021 Google LLC
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

#include <algorithm>
#include <utility>

#include "src/jpeg2000Compression.h"
#include "src/jpegCompression.h"
#include "src/opencvinterpolationframe.h"
#include "src/rawCompression.h"
#include "src/zlibWrapper.h"

namespace wsiToDicomConverter {

OpenCVInterpolationFrame::OpenCVInterpolationFrame(
    openslide_t *osr, int64_t locationX, int64_t locationY, int32_t level,
    int64_t frameWidthDownsampled, int64_t frameHeightDownsampled,
    int64_t frameWidth, int64_t frameHeight, DCM_Compression compression,
    int quality, int64_t levelWidth, int64_t levelHeight, int64_t level0Width,
    int64_t level0Height, bool store_raw_bytes,
    DICOMFileFrameRegionReader *frame_region_reader,
    const cv::InterpolationFlags openCVInterpolationMethod) : Frame(locationX,
                                                                locationY,
                                                                frameWidth,
                                                               frameHeight,
                                                      compression, quality,
                                                           store_raw_bytes) {
  osr_ = osr;
  level_ = level;
  frameWidthDownsampled_ = frameWidthDownsampled;
  frameHeightDownsampled_ = frameHeightDownsampled;
  levelWidth_ = levelWidth;
  levelHeight_ = levelHeight;
  level0Width_ = level0Width;
  level0Height_ = level0Height;
  dcmFrameRegionReader_ = frame_region_reader;

  resized_ = frameWidth_ != frameWidthDownsampled_ ||
             frameHeight_ != frameHeightDownsampled_;
  openCVInterpolationMethod_ = openCVInterpolationMethod;

  if  (resized_)  {
    /*Images padded with pixels outside of the frame to enable resampling algorithms which
      sample the surrounding pixels to incporate neighboring pixel values that reside outside
      of the frames pixels that are being resized.

      Actual padding = unscaledMaxPadding * scaling factor.
    */
    const int unscaledMaxPadding = 5;

    widthScaleFactor_ = frameWidthDownsampled_ / frameWidth_;
    heightScaleFactor_ = frameHeightDownsampled_ / frameHeight_;
    int max_pad_width  = unscaledMaxPadding *
                                    static_cast<int>(ceil(widthScaleFactor_));
    int max_pad_height = unscaledMaxPadding *
                                    static_cast<int>(ceil(heightScaleFactor_));
    padLeft_ = std::min<int>(max_pad_width, locationX_);
    padTop_ =  std::min<int>(max_pad_height, locationY_);
    int padRight = std::min<int>(std::max<int>(0, levelWidth_ - (locationX_ +
                                      frameWidthDownsampled_)), max_pad_width);
    int padBottom = std::min<int>(std::max<int>(0, levelHeight_ - (locationY_ +
                                    frameHeightDownsampled_)), max_pad_height);

    scalefactorNormPadding(&padLeft_, widthScaleFactor_);
    scalefactorNormPadding(&padTop_, heightScaleFactor_);
    scalefactorNormPadding(&padRight, widthScaleFactor_);
    scalefactorNormPadding(&padBottom, heightScaleFactor_);

    padWidth_ =  padLeft_ + padRight;
    padHeight_ = padTop_ + padBottom;
  } else {
    widthScaleFactor_ = 1;
    heightScaleFactor_ = 1;
    padLeft_ = 0;
    padTop_ = 0;
    padWidth_ = 0;
    padHeight_ = 0;
  }
}

OpenCVInterpolationFrame::~OpenCVInterpolationFrame() {}

void OpenCVInterpolationFrame::scalefactorNormPadding(int *padding,
                                                      int scalefactor) {
  *padding = (*padding / scalefactor) * scalefactor;
}

void OpenCVInterpolationFrame::incSourceFrameReadCounter() {
  if (dcmFrameRegionReader_->dicom_file_count() != 0) {
    // Computes frames which downsample region will access from and increments
    // source frame counter.
    dcmFrameRegionReader_->incSourceFrameReadCounter(locationX_ - padLeft_,
                                                     locationY_ - padTop_,
                                                     frameWidthDownsampled_ +
                                                     padWidth_,
                                                     frameHeightDownsampled_ +
                                                     padHeight_);
  }
}

void OpenCVInterpolationFrame::sliceFrame() {
  // Downsamples a rectangular region a layer of a SVS and compresses frame
  // output.

  // Allocate memory to retrieve layer data from openslide
  std::unique_ptr<uint32_t[]> buf_bytes = std::make_unique<uint32_t[]>(
                    static_cast<size_t>((frameWidthDownsampled_ + padWidth_) *
                    (frameHeightDownsampled_ + padHeight_)));

  // If progressively downsampling then dcmFrameRegionReader_ contains
  // previous level downsample files and their assocciated frames. If no
  // files are contained, either the highest resolution image is being
  // downsampled or progressive downsampling is not being used. If this is
  // the case the image is retrieved using openslide.
  const bool dcmFrameRegionReaderNotInitalized =
                                dcmFrameRegionReader_->dicom_file_count() == 0;
  if (dcmFrameRegionReaderNotInitalized) {
    // Open slide API samples using xy coordinages from level 0 image.
    // upsample coordinates to level 0 to compute sampleing site.

    const int64_t Level0_x = ((locationX_ - padLeft_) * level0Width_) /
                             levelWidth_;
    const int64_t Level0_y = ((locationY_ - padTop_) * level0Height_) /
                             levelHeight_;
    // Open slide read region returns ARGB formated pixels
    // Values are pre-multiplied with alpha
    // https://github.com/openslide/openslide/wiki/PremultipliedARGB
    openslide_read_region(osr_, buf_bytes.get(), Level0_x,
                          Level0_y, level_,
                          frameWidthDownsampled_ + padWidth_,
                           frameHeightDownsampled_ + padHeight_);
    if (openslide_get_error(osr_)) {
       BOOST_LOG_TRIVIAL(error) << openslide_get_error(osr_);
       throw 1;
    }
    // Uncommon, openslide C++ API premults RGB by alpha.
    // if alpha is not zero reverse transform to get RGB
    // https://openslide.org/api/openslide_8h.html
    const int yend = frameHeightDownsampled_ + padHeight_;
    int yoffset = 0;
    for (uint32_t y = 0; y < yend; ++y) {
      const int xend = frameWidthDownsampled_ + padWidth_ + yoffset;
      for (uint32_t x = yoffset; x < xend; ++x) {
        const uint32_t pixel = buf_bytes[x];  // Pixel value to be downsampled
        const int alpha = pixel >> 24;            // Alpha value of pixel
        if (alpha == 0) {                         // If transparent skip
          continue;
        }
        uint32_t red = (pixel >> 16) & 0xFF;  // Get RGB Bytes
        uint32_t green = (pixel >> 8) & 0xFF;
        uint32_t blue = pixel & 0xFF;
        // Uncommon, openslide C++ API premults RGB by alpha.
        // if alpha is not zero reverse transform to get RGB
        // https://openslide.org/api/openslide_8h.html
        if (alpha != 0xFF) {
          red = red * 255 / alpha;
          green = green * 255 / alpha;
          blue = blue * 255 / alpha;
        }
        // Swap red and blue channel for dicom compatiability.
        buf_bytes[x] = (alpha << 24) | (blue << 16) | (green << 8) | red;
      }
      yoffset += frameWidthDownsampled_ + padWidth_;
    }
  } else {
    dcmFrameRegionReader_->read_region(locationX_ - padLeft_,
                                       locationY_ - padTop_,
                                       frameWidthDownsampled_ + padWidth_,
                                       frameHeightDownsampled_ + padHeight_,
                                       buf_bytes.get());
  }
  const size_t frame_mem_size = static_cast<size_t>(frameWidth_ * frameHeight_);
  std::unique_ptr<uint32_t[]> raw_bytes;
  if  (!resized_)  {
    // If image is not being resized move memory from buffer to raw_bytes
    raw_bytes = std::move(buf_bytes);
  } else {
    // Initalize OpenCV image with source image bits padded out to
    // provide context beyond frame boundry.
    cv::Mat source_image(frameHeightDownsampled_+padHeight_,
                         frameWidthDownsampled_+padWidth_, CV_8UC4,
                          buf_bytes.get());
    cv::Mat resized_image;
    const int resize_width = frameWidth_ + (padWidth_ / widthScaleFactor_);
    const int resize_height =  frameHeight_ + (padHeight_ / heightScaleFactor_);
    /*
     ResizeFlags
     cv::INTER_NEAREST = 0,
     cv::INTER_LINEAR = 1,
     cv::INTER_CUBIC = 2,
     cv::INTER_AREA = 3,
     cv::INTER_LANCZOS4 = 4,
     cv::INTER_LINEAR_EXACT = 5,
     cv::INTER_NEAREST_EXACT = 6,
     cv::INTER_MAX = 7,
    */
    // Open CV resize image
    cv::resize(source_image, resized_image,
               cv::Size(resize_width, resize_height), 0, 0,
               openCVInterpolationMethod_);

    // Copy area of intrest from resized source image to raw bytres buffer
    raw_bytes = std::make_unique<uint32_t[]>(frame_mem_size);
    const int xstart = padLeft_ / widthScaleFactor_;
    const int ystart = padTop_ / heightScaleFactor_;
    const int yend = frameHeight_ + ystart;
    uint32_t raw_offset = 0;
    uint32_t source_yoffset = ystart * resize_width;
    uint32_t* resized_source_img =
                              reinterpret_cast<uint32_t*>(resized_image.data);
    for (uint32_t y = ystart; y < yend; ++y) {
      const uint32_t xend = frameWidth_+xstart + source_yoffset;
      for (uint32_t x = xstart+source_yoffset; x < xend; ++x) {
        raw_bytes[raw_offset] = resized_source_img[x];
        raw_offset += 1;
      }
      source_yoffset  +=  resize_width;
    }
  }

  // Create a boot::gil view of memory in downsample_bytes
  boost::gil::rgba8c_view_t gil = boost::gil::interleaved_view(
      frameWidth_, frameHeight_,
      (const boost::gil::rgba8c_pixel_t *)raw_bytes.get(),
      frameWidth_ * sizeof(uint32_t));

  // Create gil::image to copy bytes into
  boost::gil::rgb8_image_t exp(frameWidth_, frameHeight_);
  boost::gil::rgb8_view_t rgbView = view(exp);
  boost::gil::copy_pixels(gil, rgbView);
  // Compress memory (RAW, jpeg, or jpeg2000)
  data_ = compressor_->compress(rgbView, &size_);
  if (!store_raw_bytes_) {
    raw_compressed_bytes_ = nullptr;
    raw_compressed_bytes_size_ = 0;
  } else {
    raw_compressed_bytes_ = std::move(compress_memory(
                                   reinterpret_cast<uint8_t*>(raw_bytes.get()),
                                   frame_mem_size * sizeof(uint32_t),
                                   &raw_compressed_bytes_size_));
    BOOST_LOG_TRIVIAL(debug) << " compressed raw frame size: " <<
                                raw_compressed_bytes_size_ / 1024 << "kb";
  }
  BOOST_LOG_TRIVIAL(debug) << " frame size: " << size_ / 1024 << "kb";
  done_ = true;
}

}  // namespace wsiToDicomConverter

