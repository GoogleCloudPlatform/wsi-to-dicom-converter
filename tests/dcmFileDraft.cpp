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

#include "src/dcmFileDraft.h"
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcistrmb.h>
#include <dcmtk/dcmdata/dcostrma.h>
#include <dcmtk/dcmdata/dcostrmb.h>
#include <dcmtk/dcmdata/dcuid.h>
#include <dcmtk/ofstd/ofvector.h>
#include <gtest/gtest.h>
#include <boost/filesystem.hpp>
#include <memory>
#include <utility>
#include <vector>
#include <string>
#include "tests/testUtils.h"

static int bufferSize = 10000;
TEST(fileGeneration, withoutConcatenation) {
  // emptyPixelData
  std::vector<std::unique_ptr<Frame> > framesData;

  wsiToDicomConverter::DcmFileDraft draft(
      std::move(framesData), "./", 100, 50000, 50000, 0, 0, 100, 0, 0, 500, 500,
      "study", "series", "image", JPEG, 0, nullptr, 0.0, 0.0);

  OFVector<Uint8> writeBuffer(bufferSize);
  std::unique_ptr<DcmOutputBufferStream> output =
      std::make_unique<DcmOutputBufferStream>(&writeBuffer[0],
                                              writeBuffer.size());
  draft.write(output.get());
  DcmFileFormat dcmFileFormat;
  DcmInputBufferStream input;
  input.setBuffer(&writeBuffer[0], writeBuffer.size());
  dcmFileFormat.read(input);
  char* stringValue;
  findElement(dcmFileFormat.getDataset(), DCM_LossyImageCompression)
      ->getString(stringValue);

  ASSERT_EQ("01", std::string(stringValue));
  findElement(dcmFileFormat.getDataset(), DCM_SeriesDescription)
      ->getString(stringValue);
  ASSERT_EQ("image", std::string(stringValue));

  DcmSequenceOfItems* element =
      reinterpret_cast<DcmSequenceOfItems*>(findElement(
          dcmFileFormat.getDataset(), DCM_PerFrameFunctionalGroupsSequence));

  ASSERT_NE(nullptr, element);
}

TEST(fileGeneration, withConcatenation) {
  // emptyPixelData
  std::vector<std::unique_ptr<Frame> > framesData;

  wsiToDicomConverter::DcmFileDraft draft(
      std::move(framesData), "./", 100, 50000, 50000, 0, 1, 50, 0, 0, 500, 500,
      "study", "series", "image", JPEG2000, 1, nullptr, 0.0, 0.0);

  OFVector<Uint8> writeBuffer(bufferSize);
  std::unique_ptr<DcmOutputBufferStream> output =
      std::make_unique<DcmOutputBufferStream>(&writeBuffer[0],
                                              writeBuffer.size());
  draft.write(output.get());
  DcmFileFormat dcmFileFormat;
  DcmInputBufferStream input;
  input.setBuffer(&writeBuffer[0], writeBuffer.size());
  dcmFileFormat.read(input);
  char* stringValue;
  Uint16 batchNumber;
  findElement(dcmFileFormat.getDataset(), DCM_InConcatenationNumber)
      ->getUint16(batchNumber);
  ASSERT_EQ(2, batchNumber);
  findElement(dcmFileFormat.getDataset(), DCM_LossyImageCompression)
      ->getString(stringValue);
  ASSERT_EQ("00", std::string(stringValue));
}

TEST(fileGeneration, fileSave) {
  // emptyPixelData
  std::vector<std::unique_ptr<Frame> > framesData;

  wsiToDicomConverter::DcmFileDraft draft(
      std::move(framesData), "./", 100, 50000, 50000, 0, 0, 100, 0, 0, 500, 500,
      "study", "series", "image", JPEG2000, 1, nullptr, 0.0, 0.0);

  draft.saveFile();
  ASSERT_TRUE(boost::filesystem::exists("./level-0-frames-0-100.dcm"));
}

TEST(fileGeneration, fileSaveBatch) {
  // emptyPixelData
  std::vector<std::unique_ptr<Frame> > framesData;

  wsiToDicomConverter::DcmFileDraft draft(
      std::move(framesData), "./", 1000, 50000, 50000, 2, 5, 100, 0, 0, 50, 50,
      "study", "series", "image", JPEG2000, 1, nullptr, 0.0, 0.0);

  draft.saveFile();
  ASSERT_TRUE(boost::filesystem::exists("./level-2-frames-900-1000.dcm"));
}
