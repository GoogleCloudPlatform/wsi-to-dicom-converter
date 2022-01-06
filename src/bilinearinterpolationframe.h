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

#ifndef SRC_BILINEARINTERPOLATIONFRAME_H_
#define SRC_BILINEARINTERPOLATIONFRAME_H_
#include <openslide.h>
#include <stdlib.h>

#include <memory>
#include <vector>

#include "src/dicom_file_region_reader.h"
#include "src/compressor.h"
#include "src/frame.h"

namespace wsiToDicomConverter {

// Frame represents a single image frame from the OpenSlide library
class BilinearInterpolationFrame : public Frame {
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
  BilinearInterpolationFrame(openslide_t *osr, int64_t locationX,
                             int64_t locationY, int32_t level,
                             int64_t frameWidthDownsampled,
                             int64_t frameHeightDownsampled, int64_t frameWidth,
                             int64_t frameHeight, DCM_Compression compression,
                             int quality, int64_t levelWidthDownsampled,
                             int64_t levelHeightDownsampled, int64_t levelWidth,
                             int64_t levelHeight, int64_t level0Width,
                             int64_t level0Height,
                             bool store_raw_bytes,
                             DICOMFileFrameRegionReader* frame_region_reader);

  virtual ~BilinearInterpolationFrame();
  // Gets frame by openslide library, performs scaling it and compressing
  virtual void sliceFrame();
  virtual void incSourceFrameReadCounter();

 private:
  openslide_t *osr_;
  int64_t level_;
  int64_t frameWidthDownsampled_;
  int64_t frameHeightDownsampled_;
  int64_t targetLevelWidth_;
  int64_t targetLevelHeight_;

  int64_t levelWidth_;
  int64_t levelHeight_;

  int64_t level0Width_;
  int64_t level0Height_;
  DICOMFileFrameRegionReader *dcmFrameRegionReader_;

  void _get_frame_location_and_dim(int64_t *LevelX, int64_t *LevelY,
                                   int64_t *sampleWidth,
                                   int64_t *sampleHeight);
};

}  // namespace wsiToDicomConverter

#endif  // SRC_BILINEARINTERPOLATIONFRAME_H_
