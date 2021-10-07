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

#include "src/geometryUtils.h"

TEST(geometryTest, frameSizeWithoutRetile) {
  int64_t levelWidth = 100;
  int64_t levelHeight = 100;
  int64_t frameWidth = 150;
  int64_t frameHeight = 150;
  bool retile = true;
  double downsampleOfLevel = 1;
  int64_t frameWidthDownsampled;
  int64_t frameHeightDownsampled;
  int64_t levelWidthDownsampled;
  int64_t levelHeightDownsampled;
  int64_t level_frameWidth;
  int64_t level_frameHeight;
  DCM_Compression level_compression = JPEG;

  wsiToDicomConverter::dimensionDownsampling(
      frameWidth, frameHeight, levelWidth, levelHeight, retile,
      downsampleOfLevel, &frameWidthDownsampled, &frameHeightDownsampled,
      &levelWidthDownsampled, &levelHeightDownsampled, &level_frameWidth,
      &level_frameHeight, &level_compression);
  ASSERT_EQ(100, frameWidthDownsampled);
  ASSERT_EQ(100, frameHeightDownsampled);
  ASSERT_EQ(100, level_frameWidth);
  ASSERT_EQ(100, level_frameHeight);

  frameWidth = 50;
  frameHeight = 50;

  wsiToDicomConverter::dimensionDownsampling(
      frameWidth, frameHeight, levelWidth, levelHeight, retile,
      downsampleOfLevel, &frameWidthDownsampled, &frameHeightDownsampled,
      &levelWidthDownsampled, &levelHeightDownsampled, &level_frameWidth,
      &level_frameHeight, &level_compression);
  ASSERT_EQ(50, frameWidthDownsampled);
  ASSERT_EQ(50, frameHeightDownsampled);
  ASSERT_EQ(50, level_frameWidth);
  ASSERT_EQ(50, level_frameHeight);
}

TEST(geometryTest, frameSizeWithRetile) {
  int64_t levelWidth = 100;
  int64_t levelHeight = 100;
  int64_t frameWidth = 42;
  int64_t frameHeight = 42;
  bool retile = true;
  double downsampleOfLevel = 2;
  int64_t frameWidthDownsampled;
  int64_t frameHeightDownsampled;
  int64_t levelWidthDownsampled;
  int64_t levelHeightDownsampled;
  int64_t level_frameWidth;
  int64_t level_frameHeight;
  DCM_Compression level_compression = JPEG;
  wsiToDicomConverter::dimensionDownsampling(
      frameWidth, frameHeight, levelWidth, levelHeight, retile,
      downsampleOfLevel, &frameWidthDownsampled, &frameHeightDownsampled,
      &levelWidthDownsampled, &levelHeightDownsampled, &level_frameWidth,
      &level_frameHeight, &level_compression);
  ASSERT_EQ(84, frameWidthDownsampled);
  ASSERT_EQ(84, frameHeightDownsampled);
  ASSERT_EQ(50, levelWidthDownsampled);
  ASSERT_EQ(50, levelHeightDownsampled);
  ASSERT_EQ(42, level_frameWidth);
  ASSERT_EQ(42, level_frameHeight);

  frameWidth = 55;
  frameHeight = 55;

  wsiToDicomConverter::dimensionDownsampling(
      frameWidth, frameHeight, levelWidth, levelHeight, retile,
      downsampleOfLevel, &frameWidthDownsampled, &frameHeightDownsampled,
      &levelWidthDownsampled, &levelHeightDownsampled, &level_frameWidth,
      &level_frameHeight, &level_compression);
  ASSERT_EQ(50, levelWidthDownsampled);
  ASSERT_EQ(50, levelHeightDownsampled);
  ASSERT_EQ(levelWidth, frameWidthDownsampled);
  ASSERT_EQ(levelHeight, frameHeightDownsampled);
  ASSERT_EQ(50, level_frameWidth);
  ASSERT_EQ(50, level_frameHeight);

  frameHeight = 42;

  wsiToDicomConverter::dimensionDownsampling(
      frameWidth, frameHeight, levelWidth, levelHeight, retile,
      downsampleOfLevel, &frameWidthDownsampled, &frameHeightDownsampled,
      &levelWidthDownsampled, &levelHeightDownsampled, &level_frameWidth,
      &level_frameHeight, &level_compression);
  ASSERT_EQ(50, levelWidthDownsampled);
  ASSERT_EQ(50, levelHeightDownsampled);
  ASSERT_EQ(100, frameWidthDownsampled);
  ASSERT_EQ(84, frameHeightDownsampled);
  ASSERT_EQ(50, level_frameWidth);
  ASSERT_EQ(42, level_frameHeight);
}
