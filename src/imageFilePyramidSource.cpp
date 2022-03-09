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
#include "src/imageFilePyramidSource.h"
#include <boost/log/trivial.hpp>
#include <opencv2/opencv.hpp>
#include <algorithm>
#include <string>
#include <memory>
#include <utility>
#include "src/zlibWrapper.h"

namespace wsiToDicomConverter {

ImageFileFrame::ImageFileFrame(int64_t locationX,
                               int64_t locationY,
                               ImageFilePyramidSource* pyramidSource) :
                            BaseFileFrame(locationX, locationY, pyramidSource) {
}

int64_t ImageFileFrame::rawABGRFrameBytes(uint8_t *rawMemory,
                                          int64_t memorySize) {
  const uint64_t locX = locationX();
  const uint64_t locY = locationY();
  const uint64_t fWidth = frameWidth();
  const uint64_t fHeight = frameHeight();
  cv::Mat *wholeImage = pyramidSource_->image();
  uint64_t width = std::min(wholeImage->cols - locX, fWidth);
  uint64_t height = std::min(wholeImage->rows - locY, fHeight);
  cv::Mat patch(*wholeImage,
                cv::Range(locY, locY + height),
                cv::Range(locX, locX + width));
  uint64_t padright = std::max(static_cast<uint64_t>(0), fWidth - width);
  uint64_t padbottom = std::max(static_cast<uint64_t>(0), fHeight - height);
  cv::Mat bgraMemory(fHeight, fWidth, CV_8UC4, rawMemory);
  if (padright > 0 || padbottom > 0) {
    cv::Mat paddedPatch(fHeight, fWidth, patch.depth());
    cv::copyMakeBorder(patch, paddedPatch, 0, padbottom, 0, padright,
                       cv::BORDER_CONSTANT, cv::Scalar(0, 0, 0));
    cv::cvtColor(paddedPatch, bgraMemory, cv::COLOR_RGB2BGRA);
  } else {
    cv::cvtColor(patch, bgraMemory, cv::COLOR_RGB2BGRA);
  }
  return memorySize;
}

void ImageFileFrame::debugLog() const {
  BOOST_LOG_TRIVIAL(info) << "Image File Frame: ";
}

// Returns frame component of DCM_DerivationDescription
// describes in text how frame imaging data was saved in frame.
std::string ImageFileFrame::derivationDescription() const {
  // todo fix
  return "Generated from DICOM";
}

ImageFilePyramidSource::ImageFilePyramidSource(absl::string_view filePath,
                                               uint64_t frameWidth,
                                               uint64_t frameHeight,
                                               double HeightMm) :
                              BaseFilePyramidSource<ImageFileFrame>(filePath) {
  frameWidth_ = frameWidth;
  frameHeight_ = frameHeight;
  std::string filename = std::move(static_cast<std::string>(filePath));
  wholeimage_ = cv::imread(filename.c_str(), cv::IMREAD_COLOR);
  if (wholeimage_.depth() != CV_8U) {
    BOOST_LOG_TRIVIAL(error) << "Cannot build DICOM Pyramid from image: " <<
                                filename <<
                                ". Image does not have unsigned 8-bit "
                                "channels.";
    return;
  }
  imageWidth_ = wholeimage_.cols;
  imageHeight_ = wholeimage_.rows;
  firstLevelHeightMm_ = HeightMm;
  firstLevelWidthMm_ = (HeightMm * static_cast<double>(imageWidth_)) /
         static_cast<double>(imageHeight_);
  int64_t locationX = 0;
  int64_t locationY = 0;
  uint64_t frameCount =
                static_cast<uint64_t>(ceil(static_cast<double>(imageWidth_) /
                                           static_cast<double>(frameWidth_))) *
                static_cast<uint64_t>(ceil(static_cast<double>(imageHeight_) /
                                           static_cast<double>(frameHeight_)));
  framesData_.reserve(frameCount);
  for (size_t idx = 0; idx < frameCount; ++idx) {
    if (locationX > imageWidth_) {
      locationX = 0;
      locationY += frameHeight_;
    }
    framesData_.push_back(std::make_unique<ImageFileFrame>(locationX,
                                                           locationY,
                                                           this));
    locationX += frameWidth_;
  }
}

cv::Mat *ImageFilePyramidSource::image() {
  return &wholeimage_;
}

void ImageFilePyramidSource::debugLog() const {
  BOOST_LOG_TRIVIAL(info) << "Image Dim: " << imageWidth() << ", " <<
                             imageHeight() <<  "\n" << "Dim mm: " <<
                             imageHeightMM() << ", " << imageWidthMM() <<
                             "\n" << "Downsample: " << downsample() << "\n" <<
                             "\n" << "Frame Count: " << fileFrameCount() <<
                             "Frame Dim: " << frameWidth() << ", " <<
                             frameHeight();
}

}  // namespace wsiToDicomConverter
