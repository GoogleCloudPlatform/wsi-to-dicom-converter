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
#include "src/abstractDcmFile.h"
#include "src/frame.h"

namespace wsiToDicomConverter {

class ImageFilePyramidSource;

class ImageFileFrame : public Frame {
 public:
  ImageFileFrame(int64_t locationX,
                int64_t locationY,
                ImageFilePyramidSource* pyramidSource);
  virtual ~ImageFileFrame();
  virtual void sliceFrame();
  virtual std::string photoMetrInt() const;
  virtual void incSourceFrameReadCounter();
  virtual void setDicomFrameBytes(std::unique_ptr<uint8_t[]> dcmdata,
                                                         uint64_t size);
  virtual void debugLog() const;
  // Returns frame component of DCM_DerivationDescription
  // describes in text how frame imaging data was saved in frame.
  virtual std::string derivationDescription() const;

  virtual bool hasRawABGRFrameBytes() const;
  virtual int64_t rawABGRFrameBytes(uint8_t *rawMemory, int64_t memorySize);

 private:
  ImageFilePyramidSource * pyramidSource_;
};

// Represents single DICOM file with metadata
class ImageFilePyramidSource : public AbstractDcmFile {
 public:
  explicit ImageFilePyramidSource(absl::string_view filePath,
                                  uint64_t frameWidth, uint64_t frameHeight,
                                  double HeightMm);
  virtual ~ImageFilePyramidSource();
  virtual int64_t frameWidth() const;
  virtual int64_t frameHeight() const;
  virtual int64_t imageWidth() const;
  virtual int64_t imageHeight() const;
  virtual int64_t fileFrameCount() const;
  virtual int64_t downsample() const;
  virtual ImageFileFrame* frame(int64_t idx) const;
  virtual double imageHeightMM() const;
  virtual double imageWidthMM() const;
  virtual std::string photometricInterpretation() const;
  void debugLog() const;
  const char *filename() const;
  cv::Mat *image();

 private:
  int64_t frameWidth_;
  int64_t frameHeight_;
  int64_t imageWidth_;
  int64_t imageHeight_;

  double firstLevelHeightMm_;
  std::vector<std::unique_ptr<ImageFileFrame>> framesData_;
  std::string filename_;
  cv::Mat wholeimage_;
};

}  // namespace wsiToDicomConverter
#endif  // SRC_IMAGEFILEPYRAMIDSOURCE_H_
