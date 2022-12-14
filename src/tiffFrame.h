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

#ifndef SRC_TIFFFRAME_H_
#define SRC_TIFFFRAME_H_
#include <absl/strings/string_view.h>
#include <jpeglib.h>
#include <memory>
#include <string>
#include <vector>

#include "src/frame.h"
#include "src/tiffFile.h"


namespace wsiToDicomConverter {

uint64_t frameIndexFromLocation(const TiffFile *tiffFile, const uint64_t level,
                                const int64_t xLoc, const int64_t yLoc);

// TiffFrame represents a image extracted without decompression
// from a SVS or Tiff file. Enables Tiff files composed of Lossy
// JPEG images to be added to DICOM directly; avoiding image
// uncompression & recompression artifacts otherwise introduced by
// use of OpenslideAPI.
class TiffFrame : public Frame {
 public:
  TiffFrame(TiffFile *tiffFile, const uint64_t tileIndex, bool storeRawBytes);
  TiffFrame(const TiffFrame &tiffFrame) = delete;
  TiffFrame &operator =(const TiffFrame &tiffFrame) = delete;

  bool canDecodeJpeg();
  const TiffDirectory *tiffDirectory() const;

  virtual ~TiffFrame();
  virtual void sliceFrame();
  virtual absl::string_view photoMetrInt() const;
  virtual int64_t rawABGRFrameBytes(uint8_t *raw_memory, int64_t memorysize);
  virtual void incSourceFrameReadCounter();
  TiffFile *tiffFile() const;
  uint64_t tileIndex() const;
  virtual void setDicomFrameBytes(std::unique_ptr<uint8_t[]> dcmdata,
                                                         uint64_t size);

  // Returns frame component of DCM_DerivationDescription
  // describes in text how frame imaging data was saved in frame.
  virtual std::string derivationDescription() const;

 private:
  TiffFile *tiffFile_;
  const uint64_t tileIndex_;
  J_COLOR_SPACE jpegDecodeColorSpace() const;
};

}  // namespace wsiToDicomConverter

#endif  // SRC_TIFFFRAME_H_
