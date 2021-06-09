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
#include <openslide.h>
#include <stdlib.h>

#include <memory>
#include <vector>

#include "src/compressor.h"
#include "src/enums.h"
#include "src/frame.h"

// Frame represents a single image frame from the OpenSlide library
class NearestNeighborFrame : public Frame {
 public:
  // osr - openslide
  // locationX, locationY - top-left corner of frame in level coordinates
  // frameWidhtDownsampled, frameHeightDownsampled - size of frame to get
  // multiplicator - size difference between 0 level and current one
  // frameWidht, frameHeight_ - size frame needs to be scaled to
  // compression - type of compression
  NearestNeighborFrame(openslide_t *osr, int64_t locationX, int64_t locationY,
                       int32_t level, int64_t frameWidhtDownsampled,
                       int64_t frameHeightDownsampled, double multiplicator,
                       int64_t frameWidht, int64_t frameHeight,
                       DCM_Compression compression, int quality);

  virtual ~NearestNeighborFrame();
  // Gets frame by openslide library, performs scaling it and compressing
  virtual void sliceFrame();
  virtual bool isDone();
  virtual uint8_t *getData();
  virtual size_t getSize();

 private:
  std::unique_ptr<uint8_t[]> data_;
  size_t size_;
  bool done_;
  openslide_t *osr_;
  int64_t locationX_;
  int64_t locationY_;
  int level_;
  int64_t frameWidhtDownsampled_;
  int64_t frameHeightDownsampled_;
  int64_t frameWidht_;
  int64_t frameHeight_;
  double multiplicator_;
  std::unique_ptr<Compressor> compressor_;
};

#endif  // SRC_NEARESTNEIGHBORFRAME_H_
