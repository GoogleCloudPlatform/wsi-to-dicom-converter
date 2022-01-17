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

#include "tests/testUtils.h"

#include "src/tiffDirectory.h"
#include "src/tiffFile.h"

namespace wsiToDicomConverter {

TEST(tiffDirectory, iSet) {
  TiffFile tfile(tiffFileName);
  TiffDirectory *tdir = tfile.directory(0);
  int64_t numi = 5;
  double  numd = 5.0;
  ASSERT_TRUE(tdir->isSet(numi));
  ASSERT_TRUE(tdir->isSet(numd));
  ASSERT_TRUE(tdir->isSet("abc"));
  numi = -1;
  numd = -1.0;
  ASSERT_TRUE(!tdir->isSet(numi));
  ASSERT_TRUE(!tdir->isSet(numd));
  ASSERT_TRUE(!tdir->isSet(""));
}

TEST(tiffDirectory, pyramidImage) {
  TiffFile tfile(tiffFileName);
  TiffDirectory *tdir = tfile.directory(0);
  ASSERT_EQ(tdir->directoryIndex(), 0);
  ASSERT_TRUE(!tdir->hasICCProfile());
  ASSERT_EQ(tdir->subfileType(), 0);
  ASSERT_EQ(tdir->imageWidth(), 2220);
  ASSERT_EQ(tdir->imageHeight(), 2967);
  ASSERT_EQ(tdir->imageDepth(), 1);
  ASSERT_EQ(tdir->bitsPerSample(), 8);
  ASSERT_EQ(tdir->tileWidth(), 240);
  ASSERT_EQ(tdir->tileHeight(), 240);
  ASSERT_EQ(tdir->tileDepth(), -1);
  ASSERT_TRUE(tdir->hasJpegTableData());
  ASSERT_NE(tdir->jpegTableData(), nullptr);
  ASSERT_EQ(tdir->tilesPerColumn(), 13);
  ASSERT_EQ(tdir->tilesPerRow(), 10);
  ASSERT_EQ(tdir->tileCount(), 130);
  ASSERT_TRUE(tdir->isTiled());
  ASSERT_TRUE(tdir->isPyramidImage());
  ASSERT_TRUE(!tdir->isThumbnailImage());
  ASSERT_TRUE(!tdir->isMacroImage());
  ASSERT_TRUE(!tdir->isLabelImage());
  ASSERT_TRUE(tdir->isJpegCompressed());
  ASSERT_TRUE(tdir->isPhotoMetricRGB());
  ASSERT_TRUE(!tdir->isPhotoMetricYCBCR());
  ASSERT_TRUE(tdir->isExtractablePyramidImage());
  ASSERT_TRUE(tdir->doImageDimensionsMatch(2220, 2967));
  ASSERT_TRUE(!tdir->doImageDimensionsMatch(500, 2967));
}

TEST(tiffDirectory, thumbnailImage) {
  TiffFile tfile(tiffFileName);
  TiffDirectory *tdir = tfile.directory(1);
  ASSERT_EQ(tdir->directoryIndex(), 1);
  ASSERT_TRUE(!tdir->hasICCProfile());
  ASSERT_EQ(tdir->subfileType(), 0);
  ASSERT_EQ(tdir->imageWidth(), 574);
  ASSERT_EQ(tdir->imageHeight(), 768);
  ASSERT_EQ(tdir->imageDepth(), 1);
  ASSERT_EQ(tdir->bitsPerSample(), 8);
  ASSERT_EQ(tdir->tileWidth(), -1);
  ASSERT_EQ(tdir->tileHeight(), -1);
  ASSERT_EQ(tdir->tileDepth(), -1);
  ASSERT_TRUE(tdir->hasJpegTableData());
  ASSERT_NE(tdir->jpegTableData(), nullptr);
  ASSERT_EQ(tdir->tilesPerColumn(), -1);
  ASSERT_EQ(tdir->tilesPerRow(), -1);
  ASSERT_EQ(tdir->tileCount(), 0);
  ASSERT_TRUE(!tdir->isTiled());
  ASSERT_TRUE(!tdir->isPyramidImage());
  ASSERT_TRUE(tdir->isThumbnailImage());
  ASSERT_TRUE(!tdir->isMacroImage());
  ASSERT_TRUE(!tdir->isLabelImage());
  ASSERT_TRUE(tdir->isJpegCompressed());
  ASSERT_TRUE(tdir->isPhotoMetricRGB());
  ASSERT_TRUE(!tdir->isPhotoMetricYCBCR());
  ASSERT_TRUE(!tdir->isExtractablePyramidImage());
  ASSERT_TRUE(!tdir->doImageDimensionsMatch(2220, 2967));
  ASSERT_TRUE(tdir->doImageDimensionsMatch(574, 768));
}

TEST(tiffDirectory, labelImage) {
  TiffFile tfile(tiffFileName);
  TiffDirectory *tdir = tfile.directory(2);
  ASSERT_EQ(tdir->directoryIndex(), 2);
  ASSERT_TRUE(!tdir->hasICCProfile());
  ASSERT_EQ(tdir->subfileType(), 0x1);
  ASSERT_EQ(tdir->imageWidth(), 387);
  ASSERT_EQ(tdir->imageHeight(), 463);
  ASSERT_EQ(tdir->imageDepth(), 1);
  ASSERT_EQ(tdir->bitsPerSample(), 8);
  ASSERT_EQ(tdir->tileWidth(), -1);
  ASSERT_EQ(tdir->tileHeight(), -1);
  ASSERT_EQ(tdir->tileDepth(), -1);
  ASSERT_TRUE(!tdir->hasJpegTableData());
  ASSERT_EQ(tdir->jpegTableData(), nullptr);
  ASSERT_EQ(tdir->tilesPerColumn(), -1);
  ASSERT_EQ(tdir->tilesPerRow(), -1);
  ASSERT_EQ(tdir->tileCount(), 0);
  ASSERT_TRUE(!tdir->isTiled());
  ASSERT_TRUE(!tdir->isPyramidImage());
  ASSERT_TRUE(!tdir->isThumbnailImage());
  ASSERT_TRUE(!tdir->isMacroImage());
  ASSERT_TRUE(tdir->isLabelImage());
  ASSERT_TRUE(!tdir->isJpegCompressed());
  ASSERT_TRUE(tdir->isPhotoMetricRGB());
  ASSERT_TRUE(!tdir->isPhotoMetricYCBCR());
  ASSERT_TRUE(!tdir->isExtractablePyramidImage());
  ASSERT_TRUE(!tdir->doImageDimensionsMatch(2220, 2967));
  ASSERT_TRUE(tdir->doImageDimensionsMatch(387, 463));
}

TEST(tiffDirectory, macroImage) {
  TiffFile tfile(tiffFileName);
  TiffDirectory *tdir = tfile.directory(3);
  ASSERT_EQ(tdir->directoryIndex(), 3);
  ASSERT_TRUE(!tdir->hasICCProfile());
  ASSERT_EQ(tdir->subfileType(), 0x9);
  ASSERT_EQ(tdir->imageWidth(), 1280);
  ASSERT_EQ(tdir->imageHeight(), 431);
  ASSERT_EQ(tdir->imageDepth(), 1);
  ASSERT_EQ(tdir->bitsPerSample(), 8);
  ASSERT_EQ(tdir->tileWidth(), -1);
  ASSERT_EQ(tdir->tileHeight(), -1);
  ASSERT_EQ(tdir->tileDepth(), -1);
  ASSERT_TRUE(tdir->hasJpegTableData());
  ASSERT_NE(tdir->jpegTableData(), nullptr);
  ASSERT_EQ(tdir->tilesPerColumn(), -1);
  ASSERT_EQ(tdir->tilesPerRow(), -1);
  ASSERT_EQ(tdir->tileCount(), 0);
  ASSERT_TRUE(!tdir->isTiled());
  ASSERT_TRUE(!tdir->isPyramidImage());
  ASSERT_TRUE(!tdir->isThumbnailImage());
  ASSERT_TRUE(tdir->isMacroImage());
  ASSERT_TRUE(!tdir->isLabelImage());
  ASSERT_TRUE(tdir->isJpegCompressed());
  ASSERT_TRUE(tdir->isPhotoMetricRGB());
  ASSERT_TRUE(!tdir->isPhotoMetricYCBCR());
  ASSERT_TRUE(!tdir->isExtractablePyramidImage());
  ASSERT_TRUE(!tdir->doImageDimensionsMatch(2220, 2967));
  ASSERT_TRUE(tdir->doImageDimensionsMatch(1280, 431));
}

}  // namespace wsiToDicomConverter
