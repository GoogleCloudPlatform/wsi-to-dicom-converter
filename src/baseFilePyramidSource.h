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

#ifndef SRC_BASEFILEPYRAMIDSOURCE_H_
#define SRC_BASEFILEPYRAMIDSOURCE_H_
#include <absl/strings/string_view.h>
#include <memory>
#include <string>
#include <vector>
#include "src/abstractDcmFile.h"
#include "src/frame.h"

namespace wsiToDicomConverter {

template<class T>
class BaseFileFrame : public Frame {
 public:
  BaseFileFrame(int64_t locationX,
                 int64_t locationY,
                 T *pyramidSource);
  virtual ~BaseFileFrame();
  virtual void sliceFrame();
  absl::string_view photoMetrInt() const;
  virtual bool hasRawABGRFrameBytes() const;
  virtual void incSourceFrameReadCounter();
  virtual void setDicomFrameBytes(std::unique_ptr<uint8_t[]> dcmdata,
                                                         uint64_t size);
 protected:
  T* pyramidSource_;
};

// Represents single DICOM file with metadata
template<class T>
class BaseFilePyramidSource : public AbstractDcmFile {
 public:
  explicit BaseFilePyramidSource(absl::string_view filePath);
  virtual ~BaseFilePyramidSource();
  virtual int64_t frameWidth() const;
  virtual int64_t frameHeight() const;
  virtual int64_t imageWidth() const;
  virtual int64_t imageHeight() const;
  virtual int64_t fileFrameCount() const;
  virtual T* frame(int64_t idx) const;
  virtual int64_t downsample() const;
  virtual double imageHeightMM() const;
  virtual double imageWidthMM() const;
  virtual absl::string_view photometricInterpretation() const;
  virtual void debugLog() const = 0;
  virtual absl::string_view filename() const;

 protected:
  std::string filename_;
  int64_t frameWidth_;
  int64_t frameHeight_;
  int64_t imageWidth_;
  int64_t imageHeight_;
  double firstLevelWidthMm_;
  double firstLevelHeightMm_;
  std::string photometric_;
  std::vector<std::unique_ptr<T>> framesData_;
};

template<class T>
BaseFileFrame<T>::BaseFileFrame(int64_t locationX,
                               int64_t locationY,
                               T* pyramidSource) :
         Frame(locationX, locationY, pyramidSource->frameWidth(),
               pyramidSource->frameHeight(), NONE, -1, subsample_420,
               true) {
  size_ = 0;
  dcmPixelItem_ = nullptr;
  rawCompressedBytes_ = nullptr;
  done_ = true;
  pyramidSource_ = pyramidSource;
}

template<class T>
absl::string_view BaseFileFrame<T>::photoMetrInt() const {
  return pyramidSource_->photometricInterpretation();
}

template<class T>
BaseFileFrame<T>::~BaseFileFrame() {}

template<class T>
bool BaseFileFrame<T>::hasRawABGRFrameBytes() const {
  return true;
}

template<class T>
void BaseFileFrame<T>::sliceFrame() {}

template<class T>
void BaseFileFrame<T>::incSourceFrameReadCounter() {
  // Reads from DICOM FILE no source frame counter to increment.
}

template<class T>
void BaseFileFrame<T>::setDicomFrameBytes(std::unique_ptr<uint8_t[]> dcmdata,
                                        uint64_t size) {
}

template<class T>
BaseFilePyramidSource<T>::BaseFilePyramidSource(absl::string_view filePath) :
                                filename_(static_cast<std::string>(filePath)) {
  frameWidth_ = 0;
  frameHeight_ = 0;
  imageWidth_ = 0;
  imageHeight_ = 0;
  firstLevelWidthMm_ = 0.0;
  firstLevelHeightMm_ = 0.0;
  photometric_ = "RGB";
}

template<class T>
BaseFilePyramidSource<T>::~BaseFilePyramidSource() {
}

template<class T>
absl::string_view BaseFilePyramidSource<T>::filename() const {
  return filename_.c_str();
}

template<class T>
int64_t BaseFilePyramidSource<T>::downsample() const {
  return 1;
}

template<class T>
int64_t BaseFilePyramidSource<T>::frameWidth() const {
  return frameWidth_;
}

template<class T>
int64_t BaseFilePyramidSource<T>::frameHeight() const {
  return frameHeight_;
}

template<class T>
int64_t BaseFilePyramidSource<T>::imageWidth() const {
  return imageWidth_;
}

template<class T>
int64_t BaseFilePyramidSource<T>::imageHeight() const {
  return imageHeight_;
}

template<class T>
double BaseFilePyramidSource<T>::imageWidthMM() const {
  return firstLevelWidthMm_;
}

template<class T>
double BaseFilePyramidSource<T>::imageHeightMM() const {
  return firstLevelHeightMm_;
}

template<class T>
absl::string_view BaseFilePyramidSource<T>::photometricInterpretation() const {
  return photometric_.c_str();
}

template<class T>
int64_t BaseFilePyramidSource<T>::fileFrameCount() const {
  return framesData_.size();
}

template<class T>
T* BaseFilePyramidSource<T>::frame(int64_t idx) const {
  return framesData_[idx].get();
}


}  // namespace wsiToDicomConverter
#endif  // SRC_BASEFILEPYRAMIDSOURCE_H_
