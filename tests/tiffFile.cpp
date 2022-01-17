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

#include "src/tiffDirectory.h"
#include "tests/testUtils.h"

#include "src/tiffFile.h"

namespace wsiToDicomConverter {

TEST(tiffFile, loadSVSFile) {
  TiffFile tfile(tiffFileName);
  ASSERT_TRUE(tfile.isLoaded());
}

TEST(tiffFile, loadInvalidSVSFile) {
  TiffFile tfile("garbagefile.jnk");
  ASSERT_TRUE(!tfile.isLoaded());
}

TEST(tiffFile, hasExtractablePyramidImages) {
  TiffFile tfile(tiffFileName);
  ASSERT_TRUE(tfile.hasExtractablePyramidImages());
}

TEST(tiffFile, DirIndexMatchingImageDimensionsForPyramidImage) {
  TiffFile tfile(tiffFileName);
  ASSERT_EQ(0, tfile.getDirectoryIndexMatchingImageDimensions(2220, 2967,
                                                                    true));
}

TEST(tiffFile, DirIndexMatchingImageDimensionsPyramidTrueForThumbnail) {
  TiffFile tfile(tiffFileName);
  ASSERT_EQ(-1, tfile.getDirectoryIndexMatchingImageDimensions(574, 768,
                                                                    true));
}

TEST(tiffFile, DirIndexMatchingImageDimensionsPyramidTrueForLabel) {
  TiffFile tfile(tiffFileName);
  ASSERT_EQ(-1, tfile.getDirectoryIndexMatchingImageDimensions(387, 463,
                                                                    true));
}

TEST(tiffFile, DirIndexMatchingImageDimensionsPyramidTrueForMacro) {
  TiffFile tfile(tiffFileName);
  ASSERT_EQ(-1, tfile.getDirectoryIndexMatchingImageDimensions(1280, 431,
                                                                     true));
}

TEST(tiffFile, DirIndexMatchingImageDimensionsPyramidTrueForInvalidImage) {
  TiffFile tfile(tiffFileName);
  ASSERT_EQ(-1, tfile.getDirectoryIndexMatchingImageDimensions(500, 500,
                                                                    true));
}

TEST(tiffFile, DirIndexMatchingImageDimensionsPyramidFalseForThumbnail) {
  TiffFile tfile(tiffFileName);
  ASSERT_EQ(1, tfile.getDirectoryIndexMatchingImageDimensions(574, 768,
                                                                   false));
}

TEST(tiffFile, DirIndexMatchingImageDimensionsPyramidFalseForLabel) {
  TiffFile tfile(tiffFileName);
  ASSERT_EQ(2, tfile.getDirectoryIndexMatchingImageDimensions(387, 463,
                                                                   false));
}

TEST(tiffFile, DirIndexMatchingImageDimensionsPyramidFalseForMacro) {
  TiffFile tfile(tiffFileName);
  ASSERT_EQ(3, tfile.getDirectoryIndexMatchingImageDimensions(1280, 431,
                                                                    false));
}

TEST(tiffFile, DirIndexMatchingImageDimensionsPyramidFalseForInvalidDim) {
  TiffFile tfile(tiffFileName);
  ASSERT_EQ(-1, tfile.getDirectoryIndexMatchingImageDimensions(98, 98, false));
}

TEST(tiffFile, getDirectory0) {
    TiffFile tfile(tiffFileName);
    ASSERT_NE(tfile.directory(0), nullptr);
}

TEST(tiffFile, getDirectory1) {
    TiffFile tfile(tiffFileName);
    ASSERT_NE(tfile.directory(1), nullptr);
}

TEST(tiffFile, getDirectory2) {
    TiffFile tfile(tiffFileName);
    ASSERT_NE(tfile.directory(2), nullptr);
}

TEST(tiffFile, getDirectory3) {
    TiffFile tfile(tiffFileName);
    ASSERT_NE(tfile.directory(3), nullptr);
}

TEST(tiffFile, getDirectoryCount) {
    TiffFile tfile(tiffFileName);
    ASSERT_EQ(4, tfile.directoryCount());
}

TEST(tiffFile, getTile) {
    TiffFile tfile(tiffFileName);
    const TiffDirectory *tdir = tfile.directory(0);
    const int tileCount = tdir->tileCount();
    for (int tileIndex = 0; tileIndex < tileCount; ++tileIndex) {
        ASSERT_NE(tfile.tile(0, tileIndex), nullptr);
    }
}

}  // namespace wsiToDicomConverter
