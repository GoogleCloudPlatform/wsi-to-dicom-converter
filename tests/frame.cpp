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
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include "tests/testUtils.h"
#include "src/nearestneighborframe.h"

TEST(sliceFrame, jpeg) {
  openslide_t* osr = openslide_open(tiffFileName);
  NearestNeighborFrame frame(osr, 0, 0, 0, 100, 100, 1, 100, 100, JPEG, 1);
  frame.sliceFrame();
  ASSERT_TRUE(frame.isDone());
  ASSERT_GE(frame.getSize(), 0);
}

TEST(sliceFrame, jpeg2000Scaling) {
  openslide_t* osr = openslide_open(tiffFileName);
  NearestNeighborFrame frame(osr, 0, 0, 0, 1000, 1000, 1, 100, 100,
                             JPEG2000, 1);
  frame.sliceFrame();
  ASSERT_TRUE(frame.isDone());
  ASSERT_GE(frame.getSize(), 0);
}

TEST(sliceFrame, rawData) {
  // all black except first pixel
  openslide_t* osr = openslide_open(tiffFileName);
  NearestNeighborFrame frame(osr, 2219, 2966, 0, 100, 100, 1, 100, 100, RAW, 1);
  frame.sliceFrame();
  ASSERT_TRUE(frame.isDone());
  for (size_t i = 0; i < 3; i++) {
    ASSERT_EQ(248, frame.getData()[i]);
  }
  for (size_t i = 3; i < frame.getSize(); i++) {
    ASSERT_EQ(frame.getData()[i], 0);
  }
}
