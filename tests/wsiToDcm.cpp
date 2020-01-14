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

#include "src/wsiToDcm.h"
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcistrmb.h>
#include <dcmtk/dcmdata/dcostrma.h>
#include <dcmtk/dcmdata/dcostrmb.h>
#include <dcmtk/dcmdata/dcuid.h>
#include <dcmtk/dcmdata/dcvrat.h>
#include <dcmtk/ofstd/ofvector.h>
#include <gtest/gtest.h>
#include <boost/filesystem.hpp>
#include <memory>
#include <string>
#include <vector>
#include "tests/testUtils.h"

TEST(readTiff, simple) {
  double downsample = 1.;
  std::string dcmFile = std::string(testPath) + "level-0-frames-0-30.dcm";
  wsiToDicomConverter::WsiRequest request;
  request.inputFile = tiffFileName;
  request.outputFileMask = testPath;
  request.frameSizeX = 500;
  request.frameSizeY = 500;
  request.startOnLevel = 0;
  request.compression = dcmCompressionFromString("jpeg");
  request.quality = 80;
  request.imageName = "image";
  request.studyId = "study";
  request.seriesId = "series";
  request.batchLimit = 100;
  request.downsamples = &downsample;
  request.debug = true;
  wsiToDicomConverter::WsiToDcm::wsi2dcm(request);
  ASSERT_TRUE(boost::filesystem::exists(dcmFile));
  DcmFileFormat dcmFileFormat;
  dcmFileFormat.loadFile(dcmFile.c_str());
  char* stringValue;
  findElement(dcmFileFormat.getDataset(), DCM_LossyImageCompression)
      ->getString(stringValue);
  ASSERT_EQ("01", std::string(stringValue));
}

TEST(readTiff, withJson) {
  std::string dcmFile = std::string(testPath) + "level-0-frames-0-1.dcm";
  std::string jsonFile = std::string(testPath) + "testDateTags.json";
  double downsample = 1.;
  wsiToDicomConverter::WsiRequest request;
  request.inputFile = tiffFileName;
  request.outputFileMask = testPath;
  request.frameSizeX = 2220;
  request.frameSizeY = 2967;
  request.compression = dcmCompressionFromString("raw");
  request.quality = 80;
  request.imageName = "image";
  request.jsonFile = jsonFile;
  request.studyId = "study";
  request.seriesId = "series";
  request.batchLimit = 100;
  request.downsamples = &downsample;
  request.debug = true;
  wsiToDicomConverter::WsiToDcm::wsi2dcm(request);
  ASSERT_TRUE(boost::filesystem::exists(dcmFile));
  DcmFileFormat dcmFileFormat;
  dcmFileFormat.loadFile(dcmFile.c_str());
  std::string date = "20190327";
  char* stringValue;
  findElement(dcmFileFormat.getDataset(), DCM_StudyDate)
      ->getString(stringValue);
  ASSERT_EQ(date, std::string(stringValue));
  findElement(dcmFileFormat.getDataset(), DCM_SeriesDate)
      ->getString(stringValue);
  ASSERT_EQ(date, std::string(stringValue));
}

TEST(readTiff, multiFile) {
  std::string dcmFile = std::string(testPath) + "level-0-frames-";
  double downsample = 1.;
  wsiToDicomConverter::WsiRequest request;
  request.inputFile = tiffFileName;
  request.outputFileMask = testPath;
  request.frameSizeX = 500;
  request.frameSizeY = 500;
  request.compression = dcmCompressionFromString("jpeg2000");
  request.quality = 0;
  request.imageName = "image";
  request.studyId = "study";
  request.seriesId = "series";
  request.batchLimit = 10;
  request.downsamples = &downsample;
  request.debug = true;
  wsiToDicomConverter::WsiToDcm::wsi2dcm(request);

  ASSERT_TRUE(boost::filesystem::exists(dcmFile + "0-10.dcm"));
  ASSERT_TRUE(boost::filesystem::exists(dcmFile + "10-20.dcm"));
  ASSERT_TRUE(boost::filesystem::exists(dcmFile + "20-30.dcm"));
}

TEST(compressionString, jpeg) {
    ASSERT_EQ(dcmCompressionFromString("jpeg"), JPEG);
    ASSERT_EQ(dcmCompressionFromString("JPEG"), JPEG);
}

TEST(compressionString, jpeg2000) {
    ASSERT_EQ(dcmCompressionFromString("jpeg2000"), JPEG2000);
    ASSERT_EQ(dcmCompressionFromString("JPEG2000"), JPEG2000);
}

TEST(compressionString, none) {
    ASSERT_EQ(dcmCompressionFromString("none"), RAW);
    ASSERT_EQ(dcmCompressionFromString("raw"), RAW);
}

TEST(compressionString, unknown) {
    ASSERT_EQ(dcmCompressionFromString("unknown"), UNKNOWN);
    ASSERT_EQ(dcmCompressionFromString("random"), UNKNOWN);
    ASSERT_EQ(dcmCompressionFromString("jpeg/"), UNKNOWN);
    ASSERT_EQ(dcmCompressionFromString("jpeg2000."), UNKNOWN);
}

