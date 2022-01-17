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

#ifndef SRC_TIFFFRAME_H_
#define SRC_TIFFFRAME_H_
#include <jpeglib.h>
#include <memory>
#include <string>
#include <vector>

#include "src/frame.h"
#include "src/tiffFile.h"


namespace wsiToDicomConverter {

uint64_t frameIndexFromLocation(const TiffFile *tiffFile, const uint64_t level,
                                const int64_t xLoc, const int64_t yLoc);

// Frame represents a DICOM image frame from the OpenSlide library or
// downsampled from level captured at higher magnification.
class TiffFrame : public Frame {
 public:
  TiffFrame(TiffFile *tiffFile, const int64_t level, const uint64_t tileIndex);

  bool canDecodeJpeg();
  const TiffDirectory *tiffDirectory() const;

  virtual ~TiffFrame();
  // Gets frame by openslide library, performs scaling it and compressing
  virtual void sliceFrame();
  virtual std::string photoMetrInt() const;
  virtual int64_t rawABGRFrameBytes(uint8_t *raw_memory, int64_t memorysize);
  virtual bool hasRawABGRFrameBytes() const;
  virtual void clearDicomMem();
  virtual void clearRawABGRMem();
  virtual void incSourceFrameReadCounter();
  TiffFile *tiffFile() const;
  int64_t tiffFileLevel() const;
  uint64_t tileIndex() const;

 private:
  TiffFile *tiffFile_;
  int64_t level_;
  uint64_t tileIndex_;
  const J_COLOR_SPACE jpegDecodeColorSpace() const;
};

}  // namespace wsiToDicomConverter

#endif  // SRC_TIFFFRAME_H_
