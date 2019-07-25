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

#include "src/geometryUtils.h"
#include <gtest/gtest.h>

TEST(geometryTest, frameSizeWithoutRetile) {
  int64_t levelWidht = 100;
  int64_t levelHeight = 100;
  int64_t frameWidht = 150;
  int64_t frameHeight = 150;
  bool retile = true;
  int level = 0;
  double downsampleOfLevel = 1;
  int64_t frameWidhtDownsampled;
  int64_t frameHeightDownsampled;
  int64_t levelWidhtDownsampled;
  int64_t levelHeightDownsampled;
  wsiToDicomConverter::dimensionDownsampling(
      frameWidht, frameHeight, levelWidht, levelHeight, retile, level,
      downsampleOfLevel, &frameWidhtDownsampled, &frameHeightDownsampled,
      &levelWidhtDownsampled, &levelHeightDownsampled);
  ASSERT_EQ(100, frameWidhtDownsampled);
  ASSERT_EQ(100, frameHeightDownsampled);

  frameWidht = 50;
  frameHeight = 50;

  wsiToDicomConverter::dimensionDownsampling(
      frameWidht, frameHeight, levelWidht, levelHeight, retile, level,
      downsampleOfLevel, &frameWidhtDownsampled, &frameHeightDownsampled,
      &levelWidhtDownsampled, &levelHeightDownsampled);
  ASSERT_EQ(50, frameWidhtDownsampled);
  ASSERT_EQ(50, frameHeightDownsampled);
}

TEST(geometryTest, frameSizeWithRetile) {
  int64_t levelWidht = 100;
  int64_t levelHeight = 100;
  int64_t frameWidht = 42;
  int64_t frameHeight = 42;
  bool retile = true;
  int level = 2;
  double downsampleOfLevel = 2;
  int64_t frameWidhtDownsampled;
  int64_t frameHeightDownsampled;
  int64_t levelWidhtDownsampled;
  int64_t levelHeightDownsampled;
  wsiToDicomConverter::dimensionDownsampling(
      frameWidht, frameHeight, levelWidht, levelHeight, retile, level,
      downsampleOfLevel, &frameWidhtDownsampled, &frameHeightDownsampled,
      &levelWidhtDownsampled, &levelHeightDownsampled);
  ASSERT_EQ(84, frameWidhtDownsampled);
  ASSERT_EQ(84, frameHeightDownsampled);
  ASSERT_EQ(50, levelWidhtDownsampled);
  ASSERT_EQ(50, levelHeightDownsampled);

  frameWidht = 55;
  frameHeight = 55;

  wsiToDicomConverter::dimensionDownsampling(
      frameWidht, frameHeight, levelWidht, levelHeight, retile, level,
      downsampleOfLevel, &frameWidhtDownsampled, &frameHeightDownsampled,
      &levelWidhtDownsampled, &levelHeightDownsampled);
  ASSERT_EQ(50, levelWidhtDownsampled);
  ASSERT_EQ(50, levelHeightDownsampled);
  ASSERT_EQ(levelWidht, frameWidhtDownsampled);
  ASSERT_EQ(levelHeight, frameHeightDownsampled);

  frameHeight = 42;

  wsiToDicomConverter::dimensionDownsampling(
      frameWidht, frameHeight, levelWidht, levelHeight, retile, level,
      downsampleOfLevel, &frameWidhtDownsampled, &frameHeightDownsampled,
      &levelWidhtDownsampled, &levelHeightDownsampled);
  ASSERT_EQ(50, levelWidhtDownsampled);
  ASSERT_EQ(50, levelHeightDownsampled);
  ASSERT_EQ(110, frameWidhtDownsampled);
  ASSERT_EQ(84, frameHeightDownsampled);
}
