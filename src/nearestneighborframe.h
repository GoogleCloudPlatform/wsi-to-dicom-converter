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

#ifndef SRC_NEARESTNEIGHBORFRAME_H_
#define SRC_NEARESTNEIGHBORFRAME_H_
#include <stdlib.h>

#include <memory>
#include <vector>

#include "src/dicom_file_region_reader.h"
#include "src/compressor.h"
#include "src/frame.h"
#include "src/openslideUtil.h"

namespace wsiToDicomConverter {

// Frame represents a DICOM image frame from the OpenSlide library or
// downsampled from level captured at higher magnification.
class NearestNeighborFrame : public Frame {
 public:
  // osr - openslide
  // locationX, locationY - top-left corner of frame in level coordinates
  // frameWidthDownsampled, frameHeightDownsampled - size of frame to get
  // multiplicator - size difference between 0 level and current one
  // frameWidth, frameHeight_ - size frame needs to be scaled to
  // compression - type of compression
  // quality - compression quality setting
  // store_raw_bytes - store raw version of frame pixels in frame in addition
  //                   to compressed pixel bytes. Required for progressive
  //                   downsampling.
  // frame_region_reader - frame reader for raw frame data from prior level.
  //                       Used to generate downsamples directly from prior
  //                       downsampled level.
  NearestNeighborFrame(OpenSlidePtr *osptr, int64_t locationX,
                       int64_t locationY, int64_t level,
                       int64_t frameWidthDownsampled,
                       int64_t frameHeightDownsampled, double multiplicator,
                       int64_t frameWidth, int64_t frameHeight,
                       DCM_Compression compression, int quality,
                       bool store_raw_bytes,
                       DICOMFileFrameRegionReader *frame_region_reader);

  virtual ~NearestNeighborFrame();
  // Gets frame by openslide library, performs scaling it and compressing
  virtual void sliceFrame();
  virtual void incSourceFrameReadCounter();

 private:
  OpenSlidePtr *osptr_;
  int64_t level_;
  int64_t frameWidthDownsampled_;
  int64_t frameHeightDownsampled_;
  double multiplicator_;
  DICOMFileFrameRegionReader *dcmFrameRegionReader_;
};

}  // namespace wsiToDicomConverter

#endif  // SRC_NEARESTNEIGHBORFRAME_H_
