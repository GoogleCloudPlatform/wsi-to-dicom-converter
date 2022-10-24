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
  OpenSlidePtr osptr = OpenSlidePtr(tiffFileName);
  NearestNeighborFrame frame(&osptr, 0, 0, 0, 100, 100, 1, 100, 100, JPEG, 1,
                             subsample_420, false, &dicom_frame_reader);
  frame.sliceFrame();
  ASSERT_TRUE(frame.isDone());
  EXPECT_FALSE(frame.hasRawABGRFrameBytes());
  EXPECT_GE(frame.dicomFrameBytesSize(), 0);
}

TEST(NearestNeighborFrame, jpeg2000Scaling) {
  DICOMFileFrameRegionReader dicom_frame_reader;
  OpenSlidePtr osptr = OpenSlidePtr(tiffFileName);
  NearestNeighborFrame frame(&osptr, 0, 0, 0, 1000, 1000, 1, 100, 100, JPEG2000,
                             1, subsample_420, true, &dicom_frame_reader);
  frame.sliceFrame();
  ASSERT_TRUE(frame.isDone());
  EXPECT_TRUE(frame.hasRawABGRFrameBytes());
  EXPECT_GE(frame.dicomFrameBytesSize(), 0);
}

TEST(NearestNeighborFrame, rawData) {
  // all black except first pixel
  DICOMFileFrameRegionReader dicom_frame_reader;
  OpenSlidePtr osptr = OpenSlidePtr(tiffFileName);
  NearestNeighborFrame frame(&osptr, 2219, 2966, 0, 100, 100, 1, 100, 100,
                             RAW, 1, subsample_420, true, &dicom_frame_reader);
  frame.sliceFrame();
  ASSERT_TRUE(frame.isDone());
  EXPECT_TRUE(frame.hasRawABGRFrameBytes());
  for (size_t i = 0; i < 3; i++) {
    EXPECT_EQ(248, frame.dicomFrameBytes()[i]);
  }
  for (size_t i = 3; i < frame.dicomFrameBytesSize(); i++) {
    EXPECT_EQ(frame.dicomFrameBytes()[i], 0);
  }
}

}  // namespace wsiToDicomConverter
