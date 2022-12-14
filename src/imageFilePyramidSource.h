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

#ifndef SRC_IMAGEFILEPYRAMIDSOURCE_H_
#define SRC_IMAGEFILEPYRAMIDSOURCE_H_

#include <absl/strings/string_view.h>
#include <jpeglib.h>
#include <opencv2/opencv.hpp>
#include <string>
#include <memory>
#include <vector>
#include "src/baseFilePyramidSource.h"

namespace wsiToDicomConverter {

// ImageFilePyramidSource forward declared. Defined below in header file.
class ImageFilePyramidSource;

class ImageFileFrame : public BaseFileFrame<ImageFilePyramidSource> {
 public:
  ImageFileFrame(int64_t locationX,
                int64_t locationY,
                ImageFilePyramidSource* pyramidSource);
  virtual void debugLog() const;
  // Returns frame component of DCM_DerivationDescription
  // describes in text how frame imaging data was saved in frame.
  virtual std::string derivationDescription() const;
  virtual int64_t rawABGRFrameBytes(uint8_t *rawMemory, int64_t memorySize);
};

// Represents single DICOM file with metadata
class ImageFilePyramidSource : public BaseFilePyramidSource<ImageFileFrame> {
 public:
  explicit ImageFilePyramidSource(absl::string_view filePath,
                                  uint64_t frameWidth, uint64_t frameHeight,
                                  double HeightMm);
  virtual void debugLog() const;
  cv::Mat *image();

 private:
  cv::Mat wholeimage_;
};

}  // namespace wsiToDicomConverter
#endif  // SRC_IMAGEFILEPYRAMIDSOURCE_H_
