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

#include "src/dicom_file_region_reader.h"
#include "src/nearestneighborframe.h"
#include "tests/testUtils.h"

namespace wsiToDicomConverter {

TEST(NearestNeighborFrame, jpeg) {
  DICOMFileFrameRegionReader dicom_frame_reader;
  openslide_t* osr = openslide_open(tiffFileName);
  NearestNeighborFrame frame(osr, 0, 0, 0, 100, 100, 1, 100, 100, JPEG, 1,
                             false, dicom_frame_reader);
  frame.sliceFrame();
  ASSERT_TRUE(frame.isDone());
  ASSERT_FALSE(frame.has_compressed_raw_bytes());
  ASSERT_GE(frame.getSize(), 0);
}

TEST(NearestNeighborFrame, jpeg2000Scaling) {
  DICOMFileFrameRegionReader dicom_frame_reader;
  std::vector<std::unique_ptr<DcmFileDraft>> higher_magnifcation_dicom_files;
  openslide_t* osr = openslide_open(tiffFileName);
  NearestNeighborFrame frame(osr, 0, 0, 0, 1000, 1000, 1, 100, 100, JPEG2000,
                             1, true, dicom_frame_reader);
  frame.sliceFrame();
  ASSERT_TRUE(frame.isDone());
  ASSERT_TRUE(frame.has_compressed_raw_bytes());
  ASSERT_GE(frame.getSize(), 0);
}

TEST(NearestNeighborFrame, rawData) {
  // all black except first pixel
  DICOMFileFrameRegionReader dicom_frame_reader;
  openslide_t* osr = openslide_open(tiffFileName);
  NearestNeighborFrame frame(osr, 2219, 2966, 0, 100, 100, 1, 100, 100, RAW, 1,
                             true, dicom_frame_reader);
  frame.sliceFrame();
  ASSERT_TRUE(frame.isDone());
  ASSERT_TRUE(frame.has_compressed_raw_bytes());
  for (size_t i = 0; i < 3; i++) {
    ASSERT_EQ(248, frame.get_dicom_frame_bytes()[i]);
  }
  for (size_t i = 3; i < frame.getSize(); i++) {
    ASSERT_EQ(frame.get_dicom_frame_bytes()[i], 0);
  }
}

}  // namespace wsiToDicomConverter
