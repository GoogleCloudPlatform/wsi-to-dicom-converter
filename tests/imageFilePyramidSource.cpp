// Copyright 2022 Google LLC
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
#include <string>
#include "src/imageFilePyramidSource.h"

namespace wsiToDicomConverter {

TEST(imageFilePyramidSource, readJpeg) {
  ImageFilePyramidSource img("../tests/bone.jpeg", 100, 256, 12.0);
  EXPECT_EQ(img.frameWidth(), 100);
  EXPECT_EQ(img.frameHeight(), 256);
  EXPECT_EQ(img.imageWidth(), 957);
  EXPECT_EQ(img.imageHeight(), 715);
  EXPECT_EQ(img.fileFrameCount(), 30);
  EXPECT_EQ(img.downsample(), 1.0);
  EXPECT_EQ(img.imageHeightMM(), 12.0);
  EXPECT_TRUE(std::abs(img.imageWidthMM() - 16.06153846153846) <= 0.001);
  EXPECT_EQ(img.photometricInterpretation(), "RGB");
  EXPECT_EQ(img.filename(), "../tests/bone.jpeg");
  EXPECT_TRUE(img.image() != nullptr);
  for (size_t idx = 0; idx < img.fileFrameCount(); ++idx) {
    EXPECT_TRUE(img.frame(idx) != nullptr);
  }
}

}  // namespace wsiToDicomConverter
