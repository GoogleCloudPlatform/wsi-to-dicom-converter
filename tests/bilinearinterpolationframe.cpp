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

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "src/bilinearinterpolationframe.h"
#include "src/dicom_file_region_reader.h"
#include "tests/testUtils.h"

namespace wsiToDicomConverter {

TEST(BilinearInterpolationFrame, jpeg) {
  DICOMFileFrameRegionReader dicom_frame_reader;
  openslide_t* osr = openslide_open(tiffFileName);
  BilinearInterpolationFrame frame(osr, 0, 0, 0, 100, 100, 100, 100, JPEG, 1,
                                   500, 500, 1000, 1000, 2000, 2000,
                                   false, dicom_frame_reader);
  frame.sliceFrame();
  ASSERT_TRUE(frame.isDone());
  ASSERT_FALSE(frame.has_compressed_raw_bytes());
  ASSERT_GE(frame.getSize(), 0);
}

TEST(BilinearInterpolationFrame, jpeg2000Scaling) {
  DICOMFileFrameRegionReader dicom_frame_reader;
  openslide_t* osr = openslide_open(tiffFileName);
  BilinearInterpolationFrame frame(osr, 0, 0, 0, 1000, 1000, 100, 100, JPEG2000,
                                   1, 500, 500, 1000, 1000, 2000, 2000,
                                   true, dicom_frame_reader);
  frame.sliceFrame();
  ASSERT_TRUE(frame.isDone());
  ASSERT_TRUE(frame.has_compressed_raw_bytes());
  ASSERT_GE(frame.getSize(), 0);
}

}  // namespace wsiToDicomConverter
