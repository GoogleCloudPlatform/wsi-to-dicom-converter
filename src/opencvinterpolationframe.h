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

#ifndef SRC_OPENCVINTERPOLATIONFRAME_H_
#define SRC_OPENCVINTERPOLATIONFRAME_H_
#include <stdlib.h>
#include <opencv2/opencv.hpp>

#include <memory>
#include <vector>

#include "src/dicom_file_region_reader.h"
#include "src/compressor.h"
#include "src/frame.h"
#include "src/openslideUtil.h"

namespace wsiToDicomConverter {

// Frame represents a single image frame from the OpenSlide library
class OpenCVInterpolationFrame : public Frame {
 public:
  // osr - openslide
  // locationX, locationY - top-left corner of frame in source level coords.
  // frameWidhtDownsampled, frameHeightDownsampled - size of frame to get
  // frameWidht, frameHeight - size frame is scaled to
  // compression - type of compression
  // quality - compression quality
  // levelWidthDownsampled, levelHeightDownsampled - dimensions of dest
  //                                                level being generated.
  // levelWidth, levelHeight - dimensions of source level being downsampled.
  // level0Width, level0Height - dimensions of base level (highest mag)
  OpenCVInterpolationFrame(OpenSlidePtr *osptr, int64_t locationX,
                       int64_t locationY, int32_t level,
                       int64_t frameWidthDownsampled,
                       int64_t frameHeightDownsampled, int64_t frameWidth,
                       int64_t frameHeight, DCM_Compression compression,
                       int quality,  JpegSubsampling subsampling,
                       int64_t levelWidth, int64_t levelHeight,
                       int64_t level0Width, int64_t level0Height,
                       bool storeRawBytes,
                       DICOMFileFrameRegionReader* frame_region_reader,
                       const cv::InterpolationFlags openCVInterpolationMethod);

  virtual ~OpenCVInterpolationFrame();
  // Gets frame by openslide library, performs scaling it and compressing
  virtual void sliceFrame();
  virtual void incSourceFrameReadCounter();

 private:
  OpenSlidePtr *osptr_;
  int64_t level_;
  int64_t frameWidthDownsampled_;
  int64_t frameHeightDownsampled_;

  int64_t levelWidth_;
  int64_t levelHeight_;

  int64_t level0Width_;
  int64_t level0Height_;
  DICOMFileFrameRegionReader *dcmFrameRegionReader_;

  bool resized_;
  int widthScaleFactor_, heightScaleFactor_;
  int padLeft_, padTop_;
  int padWidth_, padHeight_;
  cv::InterpolationFlags openCVInterpolationMethod_;

  inline void scalefactorNormPadding(int *padding, int scalefactor);
};

}  // namespace wsiToDicomConverter

#endif  // SRC_OPENCVINTERPOLATIONFRAME_H_
