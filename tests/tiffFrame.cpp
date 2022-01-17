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
#include <jpeglib.h>

#include "tests/testUtils.h"

#include "src/tiffDirectory.h"
#include "src/tiffFile.h"
#include "src/tiffFrame.h"

namespace wsiToDicomConverter {

TEST(tiffFrame, canDecodeImageFrames) {
  // Tests that all frames in test svs can be decoded.
  // SVS has jpeg table which requires re-constructing
  // jpeg byte memory from missing headers, tables, and frame data.

  TiffFile tf(tiffFileName);
  const int tiffImageLevel = 0;
  TiffDirectory * dir = tf.directory(tiffImageLevel);
  ASSERT_TRUE(dir->hasJpegTableData());
  for (int y = 0; y < dir->imageHeight(); y += dir->tileHeight()) {
    for (int x = 0; x < dir->imageWidth(); x += dir->tileWidth()) {
        TiffFrame frame1(&tf, x, y, tiffImageLevel, dir->tileWidth(),
                                                    dir->tileHeight());
        ASSERT_TRUE(frame1.canDecodeJpeg());
    }
  }
}

}  // namespace wsiToDicomConverter
