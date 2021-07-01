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
#include <stdio.h>

#include <boost/gil/extension/numeric/affine.hpp>
#include <boost/gil/extension/numeric/resample.hpp>
#include <boost/gil/extension/numeric/sampler.hpp>
#include <boost/gil/typedefs.hpp>
#include <boost/log/trivial.hpp>

#include "src/bilinearinterpolationframe.h"
#include "src/jpeg2000Compression.h"
#include "src/jpegCompression.h"
#include "src/rawCompression.h"

BilinearInterpolationFrame::BilinearInterpolationFrame(
    openslide_t *osr, int64_t locationX, int64_t locationY, int32_t level,
    int64_t frameWidthDownsampled, int64_t frameHeightDownsampled,
    int64_t frameWidht, int64_t frameHeight, DCM_Compression compression,
    int quality, int64_t levelWidthDownsampled, int64_t levelHeightDownsampled,
    int64_t levelWidth, int64_t levelHeight, int64_t level0Width,
    int64_t level0Height) {
  done_ = false;
  osr_ = osr;
  locationX_ = locationX;
  locationY_ = locationY;
  level_ = level;
  frameWidthDownsampled_ = frameWidthDownsampled;
  frameHeightDownsampled_ = frameHeightDownsampled;
  targetLevelWidth_ = levelWidthDownsampled;
  targetLevelHeight_ = levelHeightDownsampled;

  levelWidth_ = levelWidth;
  levelHeight_ = levelHeight;
  level0Width_ = level0Width;
  level0Height_ = level0Height;
  frameWidth_ = frameWidht;
  frameHeight_ = frameHeight;
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

BilinearInterpolationFrame::~BilinearInterpolationFrame() {}

void set_pixel(double *rgb_mem, const int width, const int height, int cx,
               int cy, const double red, const double green, const double blue,
               const double percent) {
  if (cx < 0 || cy < 0 || cx >= width || cy >= height) {
    return;
  }
  const int pix = 4 * (cx + (cy * width));
  rgb_mem[pix] += percent * red;
  rgb_mem[pix + 1] += percent * green;
  rgb_mem[pix + 2] += percent * blue;
  rgb_mem[pix + 3] += percent;
}

void BilinearInterpolationFrame::sliceFrame() {
  // Downsamples a rectangular region a layer of a SVS and compresses frame
  // output.

  // Slightly over estimate X and Y downsampling necessary to scale down from
  // levelheight to target height downsamping factor used to expand sampleing
  // region to incorporate pixels on the edge of the sampling region which
  // effect the edge pixel values.
  const int64_t down_sample_y =
      static_cast<int64_t>(ceil(static_cast<double>(levelHeight_) /
                                static_cast<double>(targetLevelHeight_)) -
                           1);
  const int64_t down_sample_x =
      static_cast<int64_t>(ceil(static_cast<double>(levelWidth_) /
                                static_cast<double>(targetLevelWidth_)) -
                           1);

  // Extend samping with to include boundry voxels.  Sample sampleWidth &
  // sampleHeight = target sampling region for level to downsample
  int64_t sampleWidth = 2 * down_sample_x + frameWidthDownsampled_;
  int64_t sampleHeight = 2 * down_sample_y + frameHeightDownsampled_;

  // Adjust sampleing region and start position to include surrounding voxels
  int64_t Level_x = locationX_ - down_sample_x;
  int64_t Level_y = locationY_ - down_sample_y;
  if (Level_x < 0) {
    sampleWidth += Level_x;
    Level_x = 0;
  }
  if (Level_y < 0) {
    sampleHeight += Level_y;
    Level_y = 0;
  }
  if (Level_x + sampleWidth >= levelWidth_) {
    sampleWidth = levelWidth_ - Level_x - 1;
  }
  if (Level_y + sampleHeight >= levelHeight_) {
    sampleHeight = levelHeight_ - Level_y - 1;
  }

  // Allocate memory to retrieve layer data from openslide
  uint32_t *buf_bytes = reinterpret_cast<uint32_t *>(
      malloc((size_t)sampleWidth * (size_t)sampleHeight * sizeof(uint32_t)));

  // Open slide API samples using xy coordinages from level 0 image.
  // upsample coordinates to level 0 to compute sampleing site.

  const int64_t Level0_x = (Level_x * level0Width_) / levelWidth_;
  const int64_t Level0_y = (Level_y * level0Height_) / levelHeight_;
  // Open slide read region returns ARGB formated pixels
  // Values are pre-multiplied with alpha
  // https://github.com/openslide/openslide/wiki/PremultipliedARGB
  openslide_read_region(osr_, reinterpret_cast<uint32_t *>(buf_bytes), Level0_x,
                        Level0_y, level_, sampleWidth, sampleHeight);

  if (openslide_get_error(osr_)) {
    BOOST_LOG_TRIVIAL(error) << openslide_get_error(osr_);
    throw 1;
  }

  // Allocate memory (size (width/height) of frame being downsampled into) to
  // hold RGB and RGB_Area downsampleing accumulators.
  double *rgb_mem = reinterpret_cast<double *>(
      malloc(sizeof(double) * frameWidth_ * frameHeight_ * 4));
  for (int idx = 0; idx < frameWidth_ * frameHeight_ * 4; ++idx) {
    rgb_mem[idx] = 0.0;  // Inititalize to zero.
  }

  // Transform coordinate of (0,0) in the down sampled frame to position in the
  // downsampled layer
  const double TargetFrame_XStart =
      static_cast<double>(targetLevelWidth_ * locationX_ / levelWidth_);
  const double TargetFrame_YStart =
      static_cast<double>(targetLevelHeight_ * locationY_ / levelHeight_);

  // Cache X layer coordinate position trannsformation.
  double *frameX_Cache;
  if (targetLevelWidth_ == levelWidth_) {
    frameX_Cache = NULL;
  } else {
    frameX_Cache =
        reinterpret_cast<double *>(malloc(sizeof(double) * sampleWidth));
    for (int px = 0; px < sampleWidth; ++px) {
      const int64_t PositionInLevel_x = px + Level_x;
      frameX_Cache[px] =
          static_cast<double>(PositionInLevel_x * targetLevelWidth_) /
              static_cast<double>(levelWidth_) -
          TargetFrame_XStart;
    }
  }

  // Walk across layer being downsampled Rows x columns
  for (int py = 0; py < sampleHeight; ++py) {
    const int buffer_heightoffset =
        py * sampleWidth;  // Offset into layer memory

    // transform layer coordinate py into frame coordinates
    const int64_t PositionInLevel_y = py + Level_y;
    const double framey =
        static_cast<double>(PositionInLevel_y * targetLevelHeight_) /
            static_cast<double>(levelHeight_) -
        TargetFrame_YStart;
    const double framey_floor = floor(framey);
    const int cy = static_cast<int>(framey_floor);  // frame Y coordinate
    if (cy <= -1 || cy >= levelHeight_) {
      continue;
    }
    const double ny_weight =
        framey -
        framey_floor;  // releative contribution of point at position cy + 1
    const double y_weight =
        1.0 - ny_weight;  // releative contribution of point at position cy

    // iterate over columns
    for (int px = 0; px < sampleWidth; ++px) {
      const uint32_t pixel =
          buf_bytes[px + buffer_heightoffset];  // Pixel value to be downsampled
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
      if (alpha != 255) {
        red = red * 255 / alpha;
        green = green * 255 / alpha;
        blue = blue * 255 / alpha;
      }

      const double red_d = static_cast<double>(red);
      const double green_d = static_cast<double>(green);
      const double blue_d = static_cast<double>(blue);

      // If layer does not require downsampling.. Just copy values across
      if (targetLevelWidth_ == levelWidth_) {
        const int cx = px + Level_x - locationX_;
        set_pixel(rgb_mem, frameWidth_, frameHeight_, cx, cy, red_d, green_d,
                  blue_d, 1.0);
      } else {
        const double framex = frameX_Cache[px];
        const double framex_floor = floor(framex);
        const int cx = static_cast<int>(framex_floor);  // Frame coordinate
        const double nx_weight =
            framex - framex_floor;                // Projection into cx + 1
        const double x_weight = 1.0 - nx_weight;  // Projection into cx
        set_pixel(rgb_mem, frameWidth_, frameHeight_, cx, cy, red_d, green_d,
                  blue_d, x_weight * y_weight);
        set_pixel(rgb_mem, frameWidth_, frameHeight_, cx + 1, cy, red_d,
                  green_d, blue_d, nx_weight * y_weight);
        set_pixel(rgb_mem, frameWidth_, frameHeight_, cx + 1, cy + 1, red_d,
                  green_d, blue_d, nx_weight * ny_weight);
        set_pixel(rgb_mem, frameWidth_, frameHeight_, cx, cy + 1, red_d,
                  green_d, blue_d, x_weight * ny_weight);
      }
    }
  }
  if (frameX_Cache != NULL) {
    free(frameX_Cache);  // Free frameX_Cache
  }
  free(buf_bytes);  // Free buffer read from openslide

  // Allocate memory to transform downsampled frame into ARGB
  uint32_t *downsample_bytes = reinterpret_cast<uint32_t *>(malloc(
      (size_t)frameWidth_ * (size_t)frameHeight_ * (size_t)sizeof(uint32_t)));
  int areaindex = 0;
  for (int idx = 0; idx < frameHeight_ * frameWidth_; ++idx) {
    const double area = rgb_mem[areaindex + 3];
    if (area == 0) {
      downsample_bytes[idx] = 0xFFFFFFFF;  // If no pixels were projected to
    } else {                               // space set white.
      // Get R, G, B pixel weighted area sums.  Divide by area
      const int red = static_cast<int>(rgb_mem[areaindex] / area);
      const int green = static_cast<int>(rgb_mem[areaindex + 1] / area);
      const int blue = static_cast<int>(rgb_mem[areaindex + 2] / area);

      // Dicom color space is ABGR
      downsample_bytes[idx] = 0xFF000000 | ((blue & 0xFF) << 16) |
                              ((green & 0xFF) << 8) | (red & 0xFF);
    }
    areaindex += 4;
  }
  free(rgb_mem);  // Free RGB area accumulator memory

  // Create a boot::gli view of memory in downsample_bytes
  boost::gil::rgba8c_view_t gil = boost::gil::interleaved_view(
      (size_t)frameWidth_, (size_t)frameHeight_,
      (const boost::gil::rgba8c_pixel_t *)downsample_bytes,
      frameWidth_ * sizeof(uint32_t));

  // Create gli::image to copy bytes into
  boost::gil::rgb8_image_t exp(frameWidth_, frameHeight_);
  boost::gil::rgb8_view_t rgbView = view(exp);
  boost::gil::copy_pixels(gil, rgbView);
  free(
      downsample_bytes);  // Free downsampled bytes. Image now in boost::gli obj
  // Compress memory (RAW, jpeg, or jpeg2000)
  data_ = compressor_->compress(rgbView, &size_);
  BOOST_LOG_TRIVIAL(debug) << " frame size: " << size_ / 1024 << "kb";
  done_ = true;
}

bool BilinearInterpolationFrame::isDone() { return done_; }

uint8_t *BilinearInterpolationFrame::getData() { return data_.get(); }

size_t BilinearInterpolationFrame::getSize() { return size_; }
