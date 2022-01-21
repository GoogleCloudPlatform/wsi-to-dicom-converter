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

#include <memory>
#include <utility>

#include "src/tiffDirectory.h"
#include "src/tiffFile.h"
#include "src/tiffTile.h"
#include "tests/testUtils.h"

namespace wsiToDicomConverter {

TEST(TiffTile, getTile) {
  const int imageIndex = 0;
  TiffFile tfile(tiffFileName, imageIndex);
  const TiffDirectory *tdir = tfile.fileDirectory();
  int tileIndex = 1;
  std::unique_ptr<TiffTile> tile = std::move(tfile.tile(tileIndex));
  ASSERT_NE(tile->rawBuffer_, nullptr);
  ASSERT_EQ(tile->rawBufferSize_, 2345);
  ASSERT_EQ(tile->tileIndex_, tileIndex);
  ASSERT_EQ(tile->tiffDirectory_, tdir);
}

}  // namespace wsiToDicomConverter
