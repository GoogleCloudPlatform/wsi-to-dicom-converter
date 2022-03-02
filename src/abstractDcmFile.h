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

#ifndef SRC_ABSTRACTDCMFILE_H_
#define SRC_ABSTRACTDCMFILE_H_

#include "src/frame.h"

namespace wsiToDicomConverter {

// Represents single DICOM file with metadata
class AbstractDcmFile {
 public:
  virtual ~AbstractDcmFile() {}
  virtual int64_t frameWidth() const = 0;
  virtual int64_t frameHeight() const = 0;
  virtual int64_t imageWidth() const = 0;
  virtual int64_t imageHeight() const = 0;
  virtual int64_t fileFrameCount() const = 0;
  virtual int64_t downsample() const = 0;
  virtual Frame* frame(int64_t idx) const = 0;
};

}  // namespace wsiToDicomConverter

#endif  // SRC_ABSTRACTDCMFILE_H_
