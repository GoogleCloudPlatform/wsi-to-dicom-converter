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
  wsiToDicomConverter::WsiToDcm::wsi2dcm(
      tiffFileName, testPath, 500, 500, "jpeg", 0, 0, -1, "image", "study",
      "series", "", true, &downsample, false, 100, -1, false);
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
  wsiToDicomConverter::WsiToDcm::wsi2dcm(
      tiffFileName, testPath, 2220, 2967, "raw", 0, 0, -1, "image", "study",
      "series", jsonFile, true, &downsample, false, 100, -1, false);
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
  wsiToDicomConverter::WsiToDcm::wsi2dcm(
      tiffFileName, testPath, 500, 500, "jpeg2000", 0, 0, -1, "image", "study",
      "series", "", true, &downsample, false, 10, -1, false);

  ASSERT_TRUE(boost::filesystem::exists(dcmFile + "0-10.dcm"));
  ASSERT_TRUE(boost::filesystem::exists(dcmFile + "10-20.dcm"));
  ASSERT_TRUE(boost::filesystem::exists(dcmFile + "20-30.dcm"));
}
