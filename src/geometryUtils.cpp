// Copyright 2019 Google LLC
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
#include <stdlib.h>

#include "src/geometryUtils.h"

namespace wsiToDicomConverter {

void dimensionDownsampling(
    int64_t frameWidth, int64_t frameHeight, int64_t levelWidth,
    int64_t levelHeight, bool retile, int level, double downsampleOfLevel,
    int64_t *frameWidthDownsampled, int64_t *frameHeightDownsampled,
    int64_t *levelWidthDownsampled, int64_t *levelHeightDownsampled,
    int64_t *level_frameWidth, int64_t *level_frameHeight) {
  *frameWidthDownsampled = frameWidth;
  *frameHeightDownsampled = frameHeight;
  *levelWidthDownsampled = levelWidth;
  *levelHeightDownsampled = levelHeight;
  *level_frameWidth = frameWidth;
  *level_frameHeight = frameHeight;
  if (retile && level > 0) {
    *frameWidthDownsampled *= downsampleOfLevel;
    *frameHeightDownsampled *= downsampleOfLevel;
    *levelWidthDownsampled /= downsampleOfLevel;
    *levelHeightDownsampled /= downsampleOfLevel;
  }
  if (levelWidth < *frameWidthDownsampled) {
    /*
        level_frameWidth could optimally be set to *levelWidthDownsampled
        However, a bug in JPEG2000 Codec segfaults if the size of the allocated
       frame being compressed is too small.  JPEG Codec is fine.  The max is a
       bandaid fix for the isseue. JPEG2000 codec should be fixed and the max
       can be removed here and in levelHeight < To reproduce bug: remove max,
       run JPEG2000 test case in end-to-end tests.
    */
    *frameWidthDownsampled = levelWidth;
    *level_frameWidth = std::max<int64_t>(*levelWidthDownsampled, 50);
  }
  if (levelHeight < *frameHeightDownsampled) {
    *frameHeightDownsampled = levelHeight;
    *level_frameHeight = std::max<int64_t>(*levelHeightDownsampled, 50);
  }
}

}  // namespace wsiToDicomConverter
