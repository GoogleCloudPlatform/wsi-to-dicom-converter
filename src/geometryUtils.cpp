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
    int64_t levelHeight, bool retile, double downsampleOfLevel,
    int64_t *frameWidthDownsampled, int64_t *frameHeightDownsampled,
    int64_t *levelWidthDownsampled, int64_t *levelHeightDownsampled,
    int64_t *level_frameWidth, int64_t *level_frameHeight,
    DCM_Compression *compression) {
  *frameWidthDownsampled = frameWidth;
  *frameHeightDownsampled = frameHeight;
  *levelWidthDownsampled = levelWidth;
  *levelHeightDownsampled = levelHeight;
  *level_frameWidth = frameWidth;
  *level_frameHeight = frameHeight;
  if (retile) {
    *frameWidthDownsampled *= downsampleOfLevel;
    *frameHeightDownsampled *= downsampleOfLevel;
    *levelWidthDownsampled /= downsampleOfLevel;
    *levelHeightDownsampled /= downsampleOfLevel;
  }
  /*
    Frames (frameWidthDownsampled, frameHeightDownsampled) are sampled from
    source Layers (levelWidth, levelHeight) and downsampled to represent
    target layer (levelWidthDownsampled, levelHeightDownsampled).  To do this
    frames are downsampled to dim (level_frameWidth, level_frameHeight).
    This logic resides in frame.c[[]]

    Normally frame dim < output layer dim. However if frame dim > than layer dim
    then frame dim = layer dim.
  */
  if (levelWidth < *frameWidthDownsampled) {
    *frameWidthDownsampled = levelWidth;
    *level_frameWidth = *levelWidthDownsampled;
    if (*compression == JPEG2000 && *level_frameWidth < 40) {
      /*
        A bug in JPEG2000 Codec segfaults if the size of the allocated
        frame being compressed is too small. JPEG Codec is fine. As a
        workaround. Changing the codec to raw for very small frame sizes. This
        fix should be removed and fixed corrected in the JPEG2000 code base.
        This code path is hit if the whole slide image is being downsampled to
        size < 40 pixels x 40 pixels
      */
      *compression = RAW;
    }
  }
  if (levelHeight < *frameHeightDownsampled) {
    *frameHeightDownsampled = levelHeight;
    *level_frameHeight = *levelHeightDownsampled;
    if (*compression == JPEG2000 && *level_frameWidth < 40) {
      /*
        A bug in JPEG2000 Codec segfaults if the size of the allocated
        frame being compressed is too small. JPEG Codec is fine. As a
        workaround. Changing the codec to raw for very small frame sizes. This
        fix should be removed and fixed corrected in the JPEG2000 code base.
        This code path is hit if the whole slide image is being downsampled to
        size < 40 pixels x 40 pixels
      */
      *compression = RAW;
    }
  }
}

}  // namespace wsiToDicomConverter
