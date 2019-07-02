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

#ifndef SRC_FRAME_H_
#define SRC_FRAME_H_
#include <openslide.h>
#include <stdlib.h>
#include <memory>
#include <vector>
#include "src/compressor.h"
#include "src/enums.h"
class Frame {
 private:
  std::unique_ptr<uint8_t[]> data_;
  size_t size_;
  bool done_;
  openslide_t *osr_;
  int64_t locationX_;
  int64_t locationY_;
  int level_;
  int64_t frameWidhtDownsampled_;
  int64_t frameHeightDownsampled_;
  int64_t frameWidht_;
  int64_t frameHeight_;
  double multiplicator_;
  std::unique_ptr<Compressor> compressor_;

 public:
  Frame(openslide_t *osr, int64_t locationX, int64_t locationY, int32_t level,
        int64_t frameWidhtDownsampled, int64_t frameHeightDownsampled,
        double multiplicator, int64_t frameWidht, int64_t frameHeight_,
        DCM_Compression compression, int quality);
  ~Frame();
  void sliceFrame();
  bool isDone();
  uint8_t *getData();
  size_t getSize();
};

#endif  // SRC_FRAME_H_
