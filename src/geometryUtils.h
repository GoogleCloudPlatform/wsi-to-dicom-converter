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

#ifndef SRC_GEOMETRYUTILS_H_
#define SRC_GEOMETRYUTILS_H_
#include <cstdint>

#include "src/enums.h"

namespace wsiToDicomConverter {

// Calculates sizes of frame and level to get from openslide
// based on base level, expected downsample and frame size
// frameWidth, frameHeight - expected size of frame
// levelWidth, levelHeight - size of level with source data
// frameWidthDownsampled, frameHeightDownsampled - size of frame to
//                                                 get from source level
// levelWidthDownsampled, levelHeightDownsampled - size of level
//                                                 according to downsampling
// level_frameWidth, level_frame: Frame Width and Height to use generate image
// typically = frameWidth & frameHeight.
// level_frameWidth, level_frame = levelWidthDownsampled, levelHeightDownsampled
//  when frame Width & Frame Height is larger than output level dim.
//  compression - compression to use to encode layer. Work around for issue
//  in current jpeg2000 codec.  Changes from jpeg2000 to RAW for very small
//  framesizes.
void dimensionDownsampling(
    int64_t frameWidth, int64_t frameHeight, int64_t levelWidth,
    int64_t levelHeight, bool retile, double downsampleOfLevel,
    int64_t *frameWidthDownsampled, int64_t *frameHeightDownsampled,
    int64_t *levelWidthDownsampled, int64_t *levelHeightDownsampled,
    int64_t *level_frameWidth, int64_t *level_frameHeight,
    DCM_Compression *compression);

}  // namespace wsiToDicomConverter
#endif  // SRC_GEOMETRYUTILS_H_
