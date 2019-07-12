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

#include "src/geometryUtils.h"

namespace wsiToDicomConverter {

void dimensionDownsampling(int64_t frameWidht, int64_t frameHeight,
                           int64_t levelWidht, int64_t levelHeight, bool retile,
                           int level, double downsampleOfLevel,
                           int64_t *frameWidhtDownsampled,
                           int64_t *frameHeightDownsampled,
                           int64_t *levelWidhtDownsampled,
                           int64_t *levelHeightDownsampled) {
  *frameWidhtDownsampled = frameWidht;
  *frameHeightDownsampled = frameHeight;
  *levelWidhtDownsampled = levelWidht;
  *levelHeightDownsampled = levelHeight;
  if (retile && level > 0) {
    *frameWidhtDownsampled *= downsampleOfLevel;
    *frameHeightDownsampled *= downsampleOfLevel;
    *levelWidhtDownsampled /=  downsampleOfLevel;
    *levelHeightDownsampled /= downsampleOfLevel;
  }
  if (levelWidht <= *frameWidhtDownsampled &&
      levelHeight <= *frameHeightDownsampled) {
    if (levelWidht >= levelHeight) {
      adjustFrameToLevel(frameWidhtDownsampled, frameHeightDownsampled,
                         levelWidht);
    } else {
      adjustFrameToLevel(frameHeightDownsampled, frameWidhtDownsampled,
                         levelHeight);
    }
  }
}

void adjustFrameToLevel(int64_t *frameFirstAxis, int64_t *frameSecondAxis,
                        int64_t levelFirstAxis) {
  double smallLevelDownsample = static_cast<double>(levelFirstAxis) /
                                static_cast<double>(*frameFirstAxis);
  *frameSecondAxis = *frameSecondAxis * smallLevelDownsample;
  *frameFirstAxis = levelFirstAxis;
}

}  // namespace wsiToDicomConverter
