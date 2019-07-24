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
// frameWidht, frameHeight - expected size of frame
// levelWidht, levelHeight - size of level with source data
// frameWidhtDownsampled, frameHeightDownsampled - size of frame to
//                                                 get from source level
// levelWidhtDownsampled, levelHeightDownsampled - size of level
//                                                 according to downsampling
void dimensionDownsampling(int64_t frameWidht, int64_t frameHeight,
                           int64_t levelWidht, int64_t levelHeight, bool retile,
                           int level, double downsampleOfLevel,
                           int64_t *frameWidhtDownsampled,
                           int64_t *frameHeightDownsampled,
                           int64_t *levelWidhtDownsampled,
                           int64_t *levelHeightDownsampled);

// Calculates size of level to fit into frame
void adjustFrameToLevel(int64_t *frameFirstAxis, int64_t *frameSecondAxis,
                        int64_t levelFirstAxis);

}  // namespace wsiToDicomConverter
#endif  // SRC_GEOMETRYUTILS_H_
