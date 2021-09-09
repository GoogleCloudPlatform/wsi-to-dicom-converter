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

#include "src/dicom_file_region_reader.h"
#include "src/compressor.h"
#include "src/enums.h"
#include "src/frame.h"

namespace wsiToDicomConverter {

// Frame represents a single image frame from the OpenSlide library
class NearestNeighborFrame : public Frame {
 public:
  // osr - openslide
  // locationX, locationY - top-left corner of frame in level coordinates
  // frameWidthDownsampled, frameHeightDownsampled - size of frame to get
  // multiplicator - size difference between 0 level and current one
  // frameWidth, frameHeight_ - size frame needs to be scaled to
  // compression - type of compression
  NearestNeighborFrame(openslide_t *osr, int64_t locationX, int64_t locationY,
                       int64_t level, int64_t frameWidthDownsampled,
                       int64_t frameHeightDownsampled, double multiplicator,
                       int64_t frameWidth, int64_t frameHeight,
                       DCM_Compression compression, int quality,
                       bool store_raw_bytes,
                       const DICOMFileFrameRegionReader &frame_region_reader);

  virtual ~NearestNeighborFrame();
  // Gets frame by openslide library, performs scaling it and compressing
  virtual void sliceFrame();
  virtual bool isDone() const;
  virtual uint8_t *get_dicom_frame_bytes();
  virtual size_t getSize() const;

  virtual int64_t get_raw_frame_bytes(uint8_t *raw_memory,
                                      int64_t memorysize) const;
  virtual int64_t get_frame_width() const;
  virtual int64_t get_frame_height() const;
  virtual void clear_dicom_mem();
  virtual bool has_compressed_raw_bytes() const;

 private:
  std::unique_ptr<uint8_t[]> data_;
  size_t size_;
  bool done_;
  openslide_t *osr_;
  int64_t locationX_;
  int64_t locationY_;
  int64_t level_;
  int64_t frameWidthDownsampled_;
  int64_t frameHeightDownsampled_;
  int64_t frameWidth_;
  int64_t frameHeight_;
  double multiplicator_;

  bool store_raw_bytes_;
  std::unique_ptr<uint8_t[]> raw_compressed_bytes_;
  int64_t raw_compressed_bytes_size_;

  std::unique_ptr<Compressor> compressor_;
  const DICOMFileFrameRegionReader &dcm_frame_region_reader;
};

}  // namespace wsiToDicomConverter

#endif  // SRC_NEARESTNEIGHBORFRAME_H_
