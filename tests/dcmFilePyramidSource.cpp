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
#include <gtest/gtest.h>
#include <dcmtk/dcmdata/dcxfer.h>
#include <string>
#include "src/dcmFilePyramidSource.h"

namespace wsiToDicomConverter {

TEST(dcmFilePyramidSource, readJpegDicom) {
  DcmFilePyramidSource img("../tests/jpeg.dicom");
  EXPECT_EQ(img.frameWidth(), 256);
  EXPECT_EQ(img.frameHeight(), 256);
  EXPECT_EQ(img.imageWidth(), 957);
  EXPECT_EQ(img.imageHeight(), 715);
  EXPECT_EQ(img.fileFrameCount(), 12);
  EXPECT_EQ(img.downsample(), 1.0);
  EXPECT_EQ(img.imageHeightMM(), 12.0);
  EXPECT_TRUE(std::abs(img.imageWidthMM() - 16.06153846153846) <= 0.001);
  EXPECT_EQ(img.photometricInterpretation(), "YBR_FULL_422");
  std::string str = img.filename();
  EXPECT_EQ(str, "../tests/jpeg.dicom");
  EXPECT_TRUE(img.tiledFull());
  EXPECT_TRUE(!img.tiledSparse());
  EXPECT_EQ(img.transferSyntax(), EXS_JPEGProcess1);
  for (size_t idx = 0; idx < img.fileFrameCount(); ++idx) {
    EXPECT_TRUE(img.frame(idx) != nullptr);
  }
  EXPECT_TRUE(img.dataset() != nullptr);
  EXPECT_TRUE(img.datasetMutex() != nullptr);
  EXPECT_EQ(img.studyInstanceUID(),
    "1.2.276.0.7230010.3.1.2.296485632.4444.1646683084.97877");
  EXPECT_EQ(img.seriesInstanceUID(),
    "1.2.276.0.7230010.3.1.3.296485632.4444.1646683084.97878");
  EXPECT_EQ(img.seriesDescription(), "test13");
}

TEST(dcmFilePyramidSource, readJpeg2kDicom) {
  DcmFilePyramidSource img("../tests/jpeg2000.dicom");
  EXPECT_EQ(img.frameWidth(), 256);
  EXPECT_EQ(img.frameHeight(), 256);
  EXPECT_EQ(img.imageWidth(), 957);
  EXPECT_EQ(img.imageHeight(), 715);
  EXPECT_EQ(img.fileFrameCount(), 12);
  EXPECT_EQ(img.downsample(), 1.0);
  EXPECT_EQ(img.imageHeightMM(), 12.0);
  EXPECT_TRUE(std::abs(img.imageWidthMM() - 16.06153846153846) <= 0.001);
  EXPECT_EQ(img.photometricInterpretation(), "RGB");
  std::string str = img.filename();
  EXPECT_EQ(str, "../tests/jpeg2000.dicom");
  EXPECT_TRUE(img.tiledFull());
  EXPECT_TRUE(!img.tiledSparse());
  EXPECT_EQ(img.transferSyntax(), EXS_JPEG2000LosslessOnly);
  for (size_t idx = 0; idx < img.fileFrameCount(); ++idx) {
    EXPECT_TRUE(img.frame(idx) != nullptr);
  }
  EXPECT_TRUE(img.dataset() != nullptr);
  EXPECT_TRUE(img.datasetMutex() != nullptr);
  EXPECT_EQ(img.studyInstanceUID(),
    "1.2.276.0.7230010.3.1.2.296485632.4407.1646682987.791295");
  EXPECT_EQ(img.seriesInstanceUID(),
    "1.2.276.0.7230010.3.1.3.296485632.4407.1646682987.791296");
  EXPECT_EQ(img.seriesDescription(), "test13");
}

TEST(dcmFilePyramidSource, readRAWDicom) {
  DcmFilePyramidSource img("../tests/raw.dicom");
  EXPECT_EQ(img.frameWidth(), 256);
  EXPECT_EQ(img.frameHeight(), 256);
  EXPECT_EQ(img.imageWidth(), 957);
  EXPECT_EQ(img.imageHeight(), 715);
  EXPECT_EQ(img.fileFrameCount(), 12);
  EXPECT_EQ(img.downsample(), 1.0);
  EXPECT_EQ(img.imageHeightMM(), 12.0);
  EXPECT_TRUE(std::abs(img.imageWidthMM() - 16.06153846153846) <= 0.001);
  EXPECT_EQ(img.photometricInterpretation(), "RGB");
  std::string str = img.filename();
  EXPECT_EQ(str, "../tests/raw.dicom");
  EXPECT_TRUE(img.tiledFull());
  EXPECT_TRUE(!img.tiledSparse());
  EXPECT_EQ(img.transferSyntax(), EXS_LittleEndianExplicit);
  for (size_t idx = 0; idx < img.fileFrameCount(); ++idx) {
    EXPECT_TRUE(img.frame(idx) != nullptr);
  }
  EXPECT_TRUE(img.dataset() != nullptr);
  EXPECT_TRUE(img.datasetMutex() != nullptr);
  EXPECT_EQ(img.studyInstanceUID(),
    "1.2.276.0.7230010.3.1.2.296485376.51.1646689307.999330");
  EXPECT_EQ(img.seriesInstanceUID(),
    "1.2.276.0.7230010.3.1.3.296485376.51.1646689307.999331");
  EXPECT_EQ(img.seriesDescription(), "test12");
}

}  // namespace wsiToDicomConverter
