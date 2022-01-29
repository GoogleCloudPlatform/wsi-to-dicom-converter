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
  const TiffDirectory *tdir = tfile.directory(0);
  int64_t numi = 5;
  double  numd = 5.0;
  EXPECT_TRUE(tdir->isSet(numi));
  EXPECT_TRUE(tdir->isSet(numd));
  EXPECT_TRUE(tdir->isSet("abc"));
  numi = -1;
  numd = -1.0;
  EXPECT_TRUE(!tdir->isSet(numi));
  EXPECT_TRUE(!tdir->isSet(numd));
  EXPECT_TRUE(!tdir->isSet(""));
}

TEST(tiffDirectory, pyramidImage) {
  TiffFile tfile(tiffFileName);
  const TiffDirectory *tdir = tfile.directory(0);
  EXPECT_EQ(tdir->directoryIndex(), 0);
  EXPECT_TRUE(!tdir->hasICCProfile());
  EXPECT_EQ(tdir->subfileType(), 0);
  EXPECT_EQ(tdir->imageWidth(), 2220);
  EXPECT_EQ(tdir->imageHeight(), 2967);
  EXPECT_EQ(tdir->imageDepth(), 1);
  EXPECT_EQ(tdir->bitsPerSample(), 8);
  EXPECT_EQ(tdir->tileWidth(), 240);
  EXPECT_EQ(tdir->tileHeight(), 240);
  EXPECT_EQ(tdir->tileDepth(), -1);
  EXPECT_TRUE(tdir->hasJpegTableData());
  EXPECT_NE(tdir->jpegTableData(), nullptr);
  EXPECT_EQ(tdir->tilesPerColumn(), 13);
  EXPECT_EQ(tdir->tilesPerRow(), 10);
  EXPECT_EQ(tdir->tileCount(), 130);
  EXPECT_TRUE(tdir->isTiled());
  EXPECT_TRUE(tdir->isPyramidImage());
  EXPECT_TRUE(!tdir->isThumbnailImage());
  EXPECT_TRUE(!tdir->isMacroImage());
  EXPECT_TRUE(!tdir->isLabelImage());
  EXPECT_TRUE(tdir->isJpegCompressed());
  EXPECT_TRUE(tdir->isPhotoMetricRGB());
  EXPECT_TRUE(!tdir->isPhotoMetricYCBCR());
  EXPECT_TRUE(tdir->isExtractablePyramidImage());
  EXPECT_TRUE(tdir->doImageDimensionsMatch(2220, 2967));
  EXPECT_TRUE(!tdir->doImageDimensionsMatch(500, 2967));
}

TEST(tiffDirectory, thumbnailImage) {
  TiffFile tfile(tiffFileName);
  const TiffDirectory *tdir = tfile.directory(1);
  EXPECT_EQ(tdir->directoryIndex(), 1);
  EXPECT_TRUE(!tdir->hasICCProfile());
  EXPECT_EQ(tdir->subfileType(), 0);
  EXPECT_EQ(tdir->imageWidth(), 574);
  EXPECT_EQ(tdir->imageHeight(), 768);
  EXPECT_EQ(tdir->imageDepth(), 1);
  EXPECT_EQ(tdir->bitsPerSample(), 8);
  EXPECT_EQ(tdir->tileWidth(), -1);
  EXPECT_EQ(tdir->tileHeight(), -1);
  EXPECT_EQ(tdir->tileDepth(), -1);
  EXPECT_TRUE(tdir->hasJpegTableData());
  EXPECT_NE(tdir->jpegTableData(), nullptr);
  EXPECT_EQ(tdir->tilesPerColumn(), -1);
  EXPECT_EQ(tdir->tilesPerRow(), -1);
  EXPECT_EQ(tdir->tileCount(), 0);
  EXPECT_TRUE(!tdir->isTiled());
  EXPECT_TRUE(!tdir->isPyramidImage());
  EXPECT_TRUE(tdir->isThumbnailImage());
  EXPECT_TRUE(!tdir->isMacroImage());
  EXPECT_TRUE(!tdir->isLabelImage());
  EXPECT_TRUE(tdir->isJpegCompressed());
  EXPECT_TRUE(tdir->isPhotoMetricRGB());
  EXPECT_TRUE(!tdir->isPhotoMetricYCBCR());
  EXPECT_TRUE(!tdir->isExtractablePyramidImage());
  EXPECT_TRUE(!tdir->doImageDimensionsMatch(2220, 2967));
  EXPECT_TRUE(tdir->doImageDimensionsMatch(574, 768));
}

TEST(tiffDirectory, labelImage) {
  TiffFile tfile(tiffFileName);
  const TiffDirectory *tdir = tfile.directory(2);
  EXPECT_EQ(tdir->directoryIndex(), 2);
  EXPECT_TRUE(!tdir->hasICCProfile());
  EXPECT_EQ(tdir->subfileType(), 0x1);
  EXPECT_EQ(tdir->imageWidth(), 387);
  EXPECT_EQ(tdir->imageHeight(), 463);
  EXPECT_EQ(tdir->imageDepth(), 1);
  EXPECT_EQ(tdir->bitsPerSample(), 8);
  EXPECT_EQ(tdir->tileWidth(), -1);
  EXPECT_EQ(tdir->tileHeight(), -1);
  EXPECT_EQ(tdir->tileDepth(), -1);
  EXPECT_TRUE(!tdir->hasJpegTableData());
  EXPECT_EQ(tdir->jpegTableData(), nullptr);
  EXPECT_EQ(tdir->tilesPerColumn(), -1);
  EXPECT_EQ(tdir->tilesPerRow(), -1);
  EXPECT_EQ(tdir->tileCount(), 0);
  EXPECT_TRUE(!tdir->isTiled());
  EXPECT_TRUE(!tdir->isPyramidImage());
  EXPECT_TRUE(!tdir->isThumbnailImage());
  EXPECT_TRUE(!tdir->isMacroImage());
  EXPECT_TRUE(tdir->isLabelImage());
  EXPECT_TRUE(!tdir->isJpegCompressed());
  EXPECT_TRUE(tdir->isPhotoMetricRGB());
  EXPECT_TRUE(!tdir->isPhotoMetricYCBCR());
  EXPECT_TRUE(!tdir->isExtractablePyramidImage());
  EXPECT_TRUE(!tdir->doImageDimensionsMatch(2220, 2967));
  EXPECT_TRUE(tdir->doImageDimensionsMatch(387, 463));
}

TEST(tiffDirectory, copyConstructor) {
  TiffFile tfile(tiffFileName);
  const TiffDirectory tdir(*tfile.directory(2));
  EXPECT_EQ(tdir.directoryIndex(), 2);
  EXPECT_TRUE(!tdir.hasICCProfile());
  EXPECT_EQ(tdir.subfileType(), 0x1);
  EXPECT_EQ(tdir.imageWidth(), 387);
  EXPECT_EQ(tdir.imageHeight(), 463);
  EXPECT_EQ(tdir.imageDepth(), 1);
  EXPECT_EQ(tdir.bitsPerSample(), 8);
  EXPECT_EQ(tdir.tileWidth(), -1);
  EXPECT_EQ(tdir.tileHeight(), -1);
  EXPECT_EQ(tdir.tileDepth(), -1);
  EXPECT_TRUE(!tdir.hasJpegTableData());
  EXPECT_EQ(tdir.jpegTableData(), nullptr);
  EXPECT_EQ(tdir.tilesPerColumn(), -1);
  EXPECT_EQ(tdir.tilesPerRow(), -1);
  EXPECT_EQ(tdir.tileCount(), 0);
  EXPECT_TRUE(!tdir.isTiled());
  EXPECT_TRUE(!tdir.isPyramidImage());
  EXPECT_TRUE(!tdir.isThumbnailImage());
  EXPECT_TRUE(!tdir.isMacroImage());
  EXPECT_TRUE(tdir.isLabelImage());
  EXPECT_TRUE(!tdir.isJpegCompressed());
  EXPECT_TRUE(tdir.isPhotoMetricRGB());
  EXPECT_TRUE(!tdir.isPhotoMetricYCBCR());
  EXPECT_TRUE(!tdir.isExtractablePyramidImage());
  EXPECT_TRUE(!tdir.doImageDimensionsMatch(2220, 2967));
  EXPECT_TRUE(tdir.doImageDimensionsMatch(387, 463));
}

TEST(tiffDirectory, macroImage) {
  TiffFile tfile(tiffFileName);
  const TiffDirectory *tdir = tfile.directory(3);
  EXPECT_EQ(tdir->directoryIndex(), 3);
  EXPECT_TRUE(!tdir->hasICCProfile());
  EXPECT_EQ(tdir->subfileType(), 0x9);
  EXPECT_EQ(tdir->imageWidth(), 1280);
  EXPECT_EQ(tdir->imageHeight(), 431);
  EXPECT_EQ(tdir->imageDepth(), 1);
  EXPECT_EQ(tdir->bitsPerSample(), 8);
  EXPECT_EQ(tdir->tileWidth(), -1);
  EXPECT_EQ(tdir->tileHeight(), -1);
  EXPECT_EQ(tdir->tileDepth(), -1);
  EXPECT_TRUE(tdir->hasJpegTableData());
  EXPECT_NE(tdir->jpegTableData(), nullptr);
  EXPECT_EQ(tdir->tilesPerColumn(), -1);
  EXPECT_EQ(tdir->tilesPerRow(), -1);
  EXPECT_EQ(tdir->tileCount(), 0);
  EXPECT_TRUE(!tdir->isTiled());
  EXPECT_TRUE(!tdir->isPyramidImage());
  EXPECT_TRUE(!tdir->isThumbnailImage());
  EXPECT_TRUE(tdir->isMacroImage());
  EXPECT_TRUE(!tdir->isLabelImage());
  EXPECT_TRUE(tdir->isJpegCompressed());
  EXPECT_TRUE(tdir->isPhotoMetricRGB());
  EXPECT_TRUE(!tdir->isPhotoMetricYCBCR());
  EXPECT_TRUE(!tdir->isExtractablePyramidImage());
  EXPECT_TRUE(!tdir->doImageDimensionsMatch(2220, 2967));
  EXPECT_TRUE(tdir->doImageDimensionsMatch(1280, 431));
}

}  // namespace wsiToDicomConverter
