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
#include <absl/strings/string_view.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcistrmb.h>
#include <dcmtk/dcmdata/dcostrma.h>
#include <dcmtk/dcmdata/dcostrmb.h>
#include <dcmtk/dcmdata/dcuid.h>
#include <dcmtk/dcmdata/dcvrat.h>
#include <dcmtk/ofstd/ofvector.h>
#include <gtest/gtest.h>
#include <boost/filesystem.hpp>
#include <math.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "tests/testUtils.h"

TEST(readTiff, simple) {
  std::vector<int>  downsamples;
  downsamples.push_back(1);
  std::string dcmFile = std::string(testPath) + "instance-1-frames-0-30.dcm";
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
  request.downsamples = std::move(downsamples);
  request.debug = true;
  wsiToDicomConverter::WsiToDcm converter(&request);
  converter.wsi2dcm();
  ASSERT_TRUE(boost::filesystem::exists(dcmFile));
  DcmFileFormat dcmFileFormat;
  dcmFileFormat.loadFile(dcmFile.c_str());
  char* stringValue;
  findElement(dcmFileFormat.getDataset(), DCM_LossyImageCompression)
      ->getString(stringValue);
  ASSERT_EQ("01", absl::string_view(stringValue));
}

TEST(readTiff, withJson) {
  std::string dcmFile = std::string(testPath) + "instance-1-frames-0-1.dcm";
  std::string jsonFile = std::string(testPath) + "testDateTags.json";
  std::vector<int>  downsamples;
  downsamples.push_back(1);
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
  request.downsamples = std::move(downsamples);;
  request.debug = true;
  wsiToDicomConverter::WsiToDcm converter(&request);
  converter.wsi2dcm();
  ASSERT_TRUE(boost::filesystem::exists(dcmFile));
  DcmFileFormat dcmFileFormat;
  dcmFileFormat.loadFile(dcmFile.c_str());
  absl::string_view date = "20190327";
  char* stringValue;
  findElement(dcmFileFormat.getDataset(), DCM_StudyDate)
      ->getString(stringValue);
  EXPECT_EQ(date, absl::string_view(stringValue));
  findElement(dcmFileFormat.getDataset(), DCM_SeriesDate)
      ->getString(stringValue);
  EXPECT_EQ(date, absl::string_view(stringValue));
}

TEST(readTiff, multiFile) {
  std::string dcmFile = std::string(testPath) + "instance-1-frames-";
  std::vector<int>  downsamples;
  downsamples.push_back(1);
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
  request.downsamples = std::move(downsamples);;
  request.debug = true;
  wsiToDicomConverter::WsiToDcm converter(&request);
  converter.wsi2dcm();
  EXPECT_TRUE(boost::filesystem::exists(dcmFile + "0-10.dcm"));
  EXPECT_TRUE(boost::filesystem::exists(dcmFile + "10-20.dcm"));
  EXPECT_TRUE(boost::filesystem::exists(dcmFile + "20-30.dcm"));
}

TEST(compressionString, jpeg) {
    EXPECT_EQ(dcmCompressionFromString("jpeg"), JPEG);
    EXPECT_EQ(dcmCompressionFromString("JPEG"), JPEG);
}

TEST(compressionString, jpeg2000) {
    EXPECT_EQ(dcmCompressionFromString("jpeg2000"), JPEG2000);
    EXPECT_EQ(dcmCompressionFromString("JPEG2000"), JPEG2000);
}

TEST(compressionString, none) {
    EXPECT_EQ(dcmCompressionFromString("none"), RAW);
    EXPECT_EQ(dcmCompressionFromString("raw"), RAW);
}

TEST(compressionString, unknown) {
    EXPECT_EQ(dcmCompressionFromString("unknown"), UNKNOWN);
    EXPECT_EQ(dcmCompressionFromString("random"), UNKNOWN);
    EXPECT_EQ(dcmCompressionFromString("jpeg/"), UNKNOWN);
    EXPECT_EQ(dcmCompressionFromString("jpeg2000."), UNKNOWN);
}

TEST(getOpenslideLevelForDownsample, image_with_one_level) {
  std::vector<int>  downsamples;
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
  request.retileLevels = 100;
  request.stopDownsamplingAtSingleFrame = true;
  request.downsamples = std::move(downsamples);
  request.debug = true;
  wsiToDicomConverter::WsiToDcm converter(&request);
  converter.initOpenSlide();
  // Not great test. Image has one level.
  EXPECT_EQ(0, converter.getOpenslideLevelForDownsample(1));
  EXPECT_EQ(0, converter.getOpenslideLevelForDownsample(99));
}

TEST(getOptimalDownSamplingOrder, smallest_slide) {
  std::vector<int>  downsamples;
  wsiToDicomConverter::WsiRequest request;
  request.inputFile = tiffFileName;
  request.outputFileMask = testPath;
  request.frameSizeX = 100;
  request.frameSizeY = 100;
  request.startOnLevel = 0;
  request.compression = dcmCompressionFromString("jpeg");
  request.quality = 80;
  request.imageName = "image";
  request.studyId = "study";
  request.seriesId = "series";
  request.batchLimit = 100;
  request.retileLevels = 100;
  request.stopDownsamplingAtSingleFrame = true;
  request.downsamples = std::move(downsamples);
  request.debug = true;
  wsiToDicomConverter::WsiToDcm converter(&request);
  converter.initOpenSlide();
  std::vector<wsiToDicomConverter::DownsamplingSlideState> slideLevelProp;
  std::vector<bool> saveLevelCompressedRaw;
  converter.getOptimalDownSamplingOrder(&slideLevelProp, nullptr);
  ASSERT_EQ(6, slideLevelProp.size());
  for (int level = 0; level < 6; ++level) {
      EXPECT_EQ(level,
                static_cast<int>(log2(slideLevelProp[level].downsample)));
  }
}

TEST(getOptimalDownSamplingOrder, smallest_slide_middle) {
  std::vector<int>  downsamples;
  downsamples.push_back(64);  // ingored; stopDownsamplingAtSingleFrame == true
  downsamples.push_back(32);  // Level = 5
  downsamples.push_back(16);  // Level = 4
  downsamples.push_back(8);   // Level = 3
  downsamples.push_back(4);   // Level = 2
  downsamples.push_back(1);   // Level = 0
  downsamples.push_back(2);   // Level = 1
  wsiToDicomConverter::WsiRequest request;
  request.inputFile = tiffFileName;
  request.outputFileMask = testPath;
  request.frameSizeX = 100;
  request.frameSizeY = 100;
  request.startOnLevel = 0;
  request.compression = dcmCompressionFromString("jpeg");
  request.quality = 80;
  request.imageName = "image";
  request.studyId = "study";
  request.seriesId = "series";
  request.batchLimit = 100;
  request.retileLevels = 7;
  request.stopDownsamplingAtSingleFrame = true;
  request.downsamples = std::move(downsamples);
  request.debug = true;
  wsiToDicomConverter::WsiToDcm converter(&request);
  converter.initOpenSlide();
  std::vector<wsiToDicomConverter::DownsamplingSlideState> slideLevelProp;
  converter.getOptimalDownSamplingOrder(&slideLevelProp, nullptr);
  ASSERT_EQ(6, slideLevelProp.size());
  // levels correspond to position in downsamples vector
  // level 64 ignored due to stopDownsamplingAtSingleFrame = true
  for (int level = 0; level < 6; ++level) {
      EXPECT_EQ(level,
                static_cast<int>(log2(slideLevelProp[level].downsample)));
  }
}

TEST(getDownsampledLevelDimensionMM, width) {
  std::vector<int>  downsamples;
  wsiToDicomConverter::WsiRequest request;
  request.inputFile = tiffFileName;
  request.outputFileMask = testPath;
  request.frameSizeX = 100;
  request.frameSizeY = 100;
  request.startOnLevel = 0;
  request.compression = dcmCompressionFromString("jpeg");
  request.quality = 80;
  request.imageName = "image";
  request.studyId = "study";
  request.seriesId = "series";
  request.batchLimit = 100;
  request.retileLevels = 100;
  request.stopDownsamplingAtSingleFrame = true;
  request.downsamples = std::move(downsamples);
  request.debug = true;
  wsiToDicomConverter::WsiToDcm converter(&request);
  converter.initOpenSlide();
  double osDim = converter.getOpenSlideDimensionMM("openslide.mpp-x");
  double dimX = converter.getDimensionMM(2220, osDim);
  ASSERT_TRUE(std::abs(dimX - 1.10778) < 0.0001);
}

TEST(getDownsampledLevelDimensionMM, height) {
  std::vector<int>  downsamples;
  wsiToDicomConverter::WsiRequest request;
  request.inputFile = tiffFileName;
  request.outputFileMask = testPath;
  request.frameSizeX = 100;
  request.frameSizeY = 100;
  request.startOnLevel = 0;
  request.compression = dcmCompressionFromString("jpeg");
  request.quality = 80;
  request.imageName = "image";
  request.studyId = "study";
  request.seriesId = "series";
  request.batchLimit = 100;
  request.retileLevels = 100;
  request.stopDownsamplingAtSingleFrame = true;
  request.downsamples = std::move(downsamples);
  request.debug = true;
  wsiToDicomConverter::WsiToDcm converter(&request);
  converter.initOpenSlide();
  double osDim = converter.getOpenSlideDimensionMM("openslide.mpp-y");
  double dimY = converter.getDimensionMM(2967, osDim);
  ASSERT_TRUE(std::abs(dimY - 1.48053) < 0.0001);
}

TEST(getSlideLevelDim, no_progressive) {
  std::vector<int>  downsamples;
  wsiToDicomConverter::WsiRequest request;
  request.inputFile = tiffFileName;
  request.outputFileMask = testPath;
  request.frameSizeX = 100;
  request.frameSizeY = 100;
  request.startOnLevel = 0;
  request.compression = dcmCompressionFromString("jpeg");
  request.quality = 80;
  request.imageName = "image";
  request.studyId = "study";
  request.seriesId = "series";
  request.batchLimit = 100;
  request.retileLevels = 100;
  request.stopDownsamplingAtSingleFrame = true;
  request.downsamples = std::move(downsamples);
  request.debug = true;
  wsiToDicomConverter::WsiToDcm converter(&request);
  converter.initOpenSlide();
  std::unique_ptr<wsiToDicomConverter::SlideLevelDim> slide_dim;
  slide_dim = std::move(converter.getSlideLevelDim(2, nullptr));
  EXPECT_EQ(slide_dim->levelWidthDownsampled, 1110);
  EXPECT_EQ(slide_dim->levelHeightDownsampled, 1483);
  EXPECT_EQ(slide_dim->levelToGet, 0);
  EXPECT_EQ(slide_dim->downsample, 2);
  EXPECT_EQ(slide_dim->level, 1);
  EXPECT_EQ(slide_dim->multiplicator, 1);
  EXPECT_EQ(slide_dim->downsampleOfLevel, 2);
  EXPECT_EQ(slide_dim->levelWidth, 2220);
  EXPECT_EQ(slide_dim->levelHeight, 2967);
  EXPECT_EQ(slide_dim->levelFrameWidth, 100);
  EXPECT_EQ(slide_dim->levelFrameHeight, 100);
  EXPECT_EQ(slide_dim->frameWidthDownsampled, 200);
  EXPECT_EQ(slide_dim->frameHeightDownsampled, 200);
  EXPECT_TRUE(slide_dim->readOpenslide);
}

TEST(getSlideLevelDim, progressive) {
  std::vector<int>  downsamples;
  wsiToDicomConverter::WsiRequest request;
  request.inputFile = tiffFileName;
  request.outputFileMask = testPath;
  request.frameSizeX = 25;
  request.frameSizeY = 25;
  request.startOnLevel = 0;
  request.compression = dcmCompressionFromString("jpeg");
  request.quality = 80;
  request.imageName = "image";
  request.studyId = "study";
  request.seriesId = "series";
  request.batchLimit = 100;
  request.retileLevels = 100;
  request.stopDownsamplingAtSingleFrame = true;
  request.preferProgressiveDownsampling = true;
  request.downsamples = std::move(downsamples);
  request.debug = true;
  wsiToDicomConverter::WsiToDcm converter(&request);
  converter.initOpenSlide();
  std::vector<wsiToDicomConverter::DownsamplingSlideState> slideLevelProp;
  std::unique_ptr<wsiToDicomConverter::SlideLevelDim> slide_dim;
  converter.getOptimalDownSamplingOrder(&slideLevelProp, nullptr);
  EXPECT_EQ(slideLevelProp.size(), 8);
  slide_dim = std::move(converter.getSlideLevelDim(1, nullptr));
  slide_dim = std::move(converter.getSlideLevelDim(2, slide_dim.get()));
  EXPECT_EQ(slide_dim->levelWidthDownsampled,  1110);
  EXPECT_EQ(slide_dim->levelHeightDownsampled, 1483);
  EXPECT_EQ(slide_dim->levelToGet, 0);
  EXPECT_EQ(slide_dim->level, 1);
  EXPECT_EQ(slide_dim->downsample, 2);
  EXPECT_EQ(slide_dim->multiplicator, 1);
  EXPECT_EQ(slide_dim->downsampleOfLevel, 2);
  EXPECT_EQ(slide_dim->levelWidth, 2220);
  EXPECT_EQ(slide_dim->levelHeight, 2967);
  EXPECT_EQ(slide_dim->levelFrameWidth, 25);
  EXPECT_EQ(slide_dim->levelFrameHeight, 25);
  EXPECT_EQ(slide_dim->frameWidthDownsampled, 50);
  EXPECT_EQ(slide_dim->frameHeightDownsampled, 50);
  EXPECT_FALSE(slide_dim->readOpenslide);
}

TEST(getSlideLevelDim, progressive_virtual_levels) {
  std::vector<int>  downsamples;
  downsamples.push_back(1);   // Level = 0
  downsamples.push_back(32);  // Level = 5
  wsiToDicomConverter::WsiRequest request;
  request.inputFile = tiffFileName;
  request.outputFileMask = testPath;
  request.frameSizeX = 25;
  request.frameSizeY = 25;
  request.startOnLevel = 0;
  request.compression = dcmCompressionFromString("jpeg");
  request.quality = 80;
  request.imageName = "image";
  request.studyId = "study";
  request.seriesId = "series";
  request.batchLimit = 100;
  request.retileLevels = 100;
  request.stopDownsamplingAtSingleFrame = true;
  request.preferProgressiveDownsampling = true;
  request.downsamples = std::move(downsamples);
  request.debug = true;
  wsiToDicomConverter::WsiToDcm converter(&request);
  converter.initOpenSlide();
  std::vector<wsiToDicomConverter::DownsamplingSlideState> slideLevelProp;
  std::unique_ptr<wsiToDicomConverter::SlideLevelDim> slide_dim;
  converter.getOptimalDownSamplingOrder(&slideLevelProp, nullptr);
  EXPECT_EQ(slideLevelProp.size(), 3);  
  EXPECT_EQ(slideLevelProp[0].generateCompressedRaw, false);
  EXPECT_EQ(slideLevelProp[0].saveDicom, true);
  EXPECT_EQ(slideLevelProp[0].instanceNumber, 1);
  EXPECT_EQ(slideLevelProp[0].downsample, 1);

  EXPECT_EQ(slideLevelProp[1].generateCompressedRaw, true);
  EXPECT_EQ(slideLevelProp[1].saveDicom, false);
  EXPECT_EQ(slideLevelProp[1].instanceNumber, 0);
  EXPECT_EQ(slideLevelProp[1].downsample, 8);

  EXPECT_EQ(slideLevelProp[2].generateCompressedRaw, false);
  EXPECT_EQ(slideLevelProp[2].saveDicom, true);
  EXPECT_EQ(slideLevelProp[2].instanceNumber, 2);
  EXPECT_EQ(slideLevelProp[2].downsample, 32);
}

TEST(getSlideLevelDim, progressive_virtual_level_start) {  
  std::vector<int>  downsamples;  
  downsamples.push_back(32);  // Level = 5
  wsiToDicomConverter::WsiRequest request;
  request.inputFile = tiffFileName;
  request.outputFileMask = testPath;
  request.frameSizeX = 25;
  request.frameSizeY = 25;
  request.startOnLevel = 0;
  request.compression = dcmCompressionFromString("jpeg");
  request.quality = 80;
  request.imageName = "image";
  request.studyId = "study";
  request.seriesId = "series";
  request.batchLimit = 100;
  request.retileLevels = 100;
  request.stopDownsamplingAtSingleFrame = true;
  request.preferProgressiveDownsampling = true;
  request.downsamples = std::move(downsamples);
  request.debug = true;
  wsiToDicomConverter::WsiToDcm converter(&request);
  converter.initOpenSlide();
  std::vector<wsiToDicomConverter::DownsamplingSlideState> slideLevelProp;
  std::unique_ptr<wsiToDicomConverter::SlideLevelDim> slide_dim;
  converter.getOptimalDownSamplingOrder(&slideLevelProp, nullptr);
  EXPECT_EQ(slideLevelProp.size(), 2);  
  EXPECT_EQ(slideLevelProp[0].generateCompressedRaw, true);
  EXPECT_EQ(slideLevelProp[0].saveDicom, false);
  EXPECT_EQ(slideLevelProp[0].instanceNumber, 0);
  EXPECT_EQ(slideLevelProp[0].downsample, 8);  
  EXPECT_EQ(slideLevelProp[1].generateCompressedRaw, false);
  EXPECT_EQ(slideLevelProp[1].saveDicom, true);
  EXPECT_EQ(slideLevelProp[1].instanceNumber, 1);
  EXPECT_EQ(slideLevelProp[1].downsample, 32);
}
