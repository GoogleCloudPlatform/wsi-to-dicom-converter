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
#include <utility>
#include <vector>
#include "tests/testUtils.h"

TEST(readTiff, simple) {
  std::vector<int>  downsamples;
  downsamples.push_back(1);
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
  ASSERT_EQ("01", std::string(stringValue));
}

TEST(readTiff, withJson) {
  std::string dcmFile = std::string(testPath) + "level-0-frames-0-1.dcm";
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

  // Not great test. Image has one level.
  ASSERT_EQ(0, converter.getOpenslideLevelForDownsample(1));
  ASSERT_EQ(0, converter.getOpenslideLevelForDownsample(99));
}

TEST(getSmallestSlideDim, smallest_slide) {
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
  std::vector<int32_t> levels;
  std::unique_ptr<wsiToDicomConverter::SlideLevelDim> slide_dim;
  slide_dim = std::move(converter.getSmallestSlideDim(&levels));
  ASSERT_EQ(6, levels.size());
  for (int level = 0; level < 6; ++level) {
      ASSERT_EQ(level, levels[level]);
  }
  ASSERT_EQ(slide_dim->levelToGet, 0);
  ASSERT_EQ(slide_dim->downsample, 32);
  ASSERT_EQ(slide_dim->multiplicator, 1);
  ASSERT_EQ(slide_dim->downsampleOfLevel, 32);
  ASSERT_EQ(slide_dim->levelWidth, 2220);
  ASSERT_EQ(slide_dim->levelHeight, 2967);
  ASSERT_EQ(slide_dim->levelWidthDownsampled, 69);
  ASSERT_EQ(slide_dim->levelHeightDownsampled, 92);
  ASSERT_EQ(slide_dim->levelFrameWidth, 69);
  ASSERT_EQ(slide_dim->levelFrameHeight, 92);
  ASSERT_EQ(slide_dim->frameWidthDownsampled, 2220);
  ASSERT_EQ(slide_dim->frameHeightDownsampled, 2967);
}

TEST(getSmallestSlideDim, smallest_slide_middle) {
  std::vector<int>  downsamples;
  downsamples.push_back(64);  // ingored; stopDownsamplingAtSingleFrame == true
  downsamples.push_back(32);  // Level = 1
  downsamples.push_back(16);  // Level = 2
  downsamples.push_back(8);   // Level = 3
  downsamples.push_back(4);   // Level = 4
  downsamples.push_back(1);   // Level = 5
  downsamples.push_back(2);   // Level = 6
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
  std::vector<int32_t> levels;
  std::unique_ptr<wsiToDicomConverter::SlideLevelDim> slide_dim;
  slide_dim = std::move(converter.getSmallestSlideDim(&levels));
  ASSERT_EQ(slide_dim->levelWidthDownsampled, 69);
  ASSERT_EQ(slide_dim->levelHeightDownsampled, 92);
  ASSERT_EQ(6, levels.size());
  // levels correspond to position in downsamples vector
  // level 64 ignored due to stopDownsamplingAtSingleFrame = true
  int expected_levels[] = {5, 6, 4, 3, 2, 1};
  for (int level = 0; level < 6; ++level) {
      ASSERT_EQ(expected_levels[level], levels[level]);
  }
  ASSERT_EQ(slide_dim->levelToGet, 0);
  ASSERT_EQ(slide_dim->downsample, 32);
  ASSERT_EQ(slide_dim->multiplicator, 1);
  ASSERT_EQ(slide_dim->downsampleOfLevel, 32);
  ASSERT_EQ(slide_dim->levelWidth, 2220);
  ASSERT_EQ(slide_dim->levelHeight, 2967);
  ASSERT_EQ(slide_dim->levelWidthDownsampled, 69);
  ASSERT_EQ(slide_dim->levelHeightDownsampled, 92);
  ASSERT_EQ(slide_dim->levelFrameWidth, 69);
  ASSERT_EQ(slide_dim->levelFrameHeight, 92);
  ASSERT_EQ(slide_dim->frameWidthDownsampled, 2220);
  ASSERT_EQ(slide_dim->frameHeightDownsampled, 2967);
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
  double dimX = converter.getDownsampledLevelDimensionMM(2220, 69, 32.0,
                                                         "openslide.mpp-x");
  ASSERT_TRUE(std::abs(dimX - 1.10179) < 0.0001);
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
  double dimY = converter.getDownsampledLevelDimensionMM(2967, 92, 32.0,
                                                         "openslide.mpp-y");
  ASSERT_TRUE(std::abs(dimY - 1.46906) < 0.0001);
}

TEST(getSlideLevelDim, no_cropping_no_progressive) {
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
  request.cropFrameToGenerateUniformPixelSpacing = true;
  request.downsamples = std::move(downsamples);
  request.debug = true;
  wsiToDicomConverter::WsiToDcm converter(&request);
  std::unique_ptr<wsiToDicomConverter::SlideLevelDim> slide_dim;
  slide_dim = std::move(converter.getSlideLevelDim(1, NULL, NULL, false));
  ASSERT_EQ(slide_dim->levelWidthDownsampled, 1110);
  ASSERT_EQ(slide_dim->levelHeightDownsampled, 1483);
  ASSERT_EQ(slide_dim->levelToGet, 0);
  ASSERT_EQ(slide_dim->downsample, 2);
  ASSERT_EQ(slide_dim->level, 1);
  ASSERT_EQ(slide_dim->multiplicator, 1);
  ASSERT_EQ(slide_dim->downsampleOfLevel, 2);
  ASSERT_EQ(slide_dim->levelWidth, 2220);
  ASSERT_EQ(slide_dim->levelHeight, 2967);
  ASSERT_EQ(slide_dim->levelFrameWidth, 100);
  ASSERT_EQ(slide_dim->levelFrameHeight, 100);
  ASSERT_EQ(slide_dim->frameWidthDownsampled, 200);
  ASSERT_EQ(slide_dim->frameHeightDownsampled, 200);
  ASSERT_EQ(slide_dim->cropSourceLevelWidth, 0);
  ASSERT_EQ(slide_dim->cropSourceLevelHeight, 0);
  ASSERT_TRUE(slide_dim->readOpenslide);
}

TEST(getSlideLevelDim, croping_no_progressive) {
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
  request.cropFrameToGenerateUniformPixelSpacing = true;
  request.downsamples = std::move(downsamples);
  request.debug = true;
  wsiToDicomConverter::WsiToDcm converter(&request);
  std::vector<int32_t> levels;
  std::unique_ptr<wsiToDicomConverter::SlideLevelDim> smallestSlideDim;
  std::unique_ptr<wsiToDicomConverter::SlideLevelDim> slide_dim;
  smallestSlideDim = std::move(converter.getSmallestSlideDim(&levels));
  ASSERT_EQ(levels.size(), 6);
  ASSERT_EQ(smallestSlideDim->levelWidthDownsampled, 69);
  ASSERT_EQ(smallestSlideDim->levelHeightDownsampled, 92);
  slide_dim = std::move(converter.getSlideLevelDim(1,
                                                   NULL,
                                                   smallestSlideDim.get(),
                                                   false));
  ASSERT_EQ(slide_dim->levelWidthDownsampled, 1104);
  ASSERT_EQ(slide_dim->levelHeightDownsampled, 1472);
  ASSERT_EQ(slide_dim->levelToGet, 0);
  ASSERT_EQ(slide_dim->downsample, 2);
  ASSERT_EQ(slide_dim->level, 1);
  ASSERT_EQ(slide_dim->multiplicator, 1);
  ASSERT_EQ(slide_dim->downsampleOfLevel, 2);
  ASSERT_EQ(slide_dim->levelWidth, 2220);
  ASSERT_EQ(slide_dim->levelHeight, 2967);
  ASSERT_EQ(slide_dim->levelFrameWidth, 100);
  ASSERT_EQ(slide_dim->levelFrameHeight, 100);
  ASSERT_EQ(slide_dim->frameWidthDownsampled, 200);
  ASSERT_EQ(slide_dim->frameHeightDownsampled, 200);
  // 2220 = width of source imaging being downsampled
  // 2208 = width of smallestSlideDim scaled to source level(0)
  // downsampling factor. 2208 = 69 (width pixels) * 32 (downsample factor)
  ASSERT_EQ(slide_dim->cropSourceLevelWidth, 2220 - 2208);
  // 2967 = height of source imaging being downsampled
  // 2944 = height of smallestSlideDim scaled to source level(0)
  // downsampling factor. 2944 = 92 (height pixels) * 32 (downsample factor)
  ASSERT_EQ(slide_dim->cropSourceLevelHeight, 2967 - 2944);
  ASSERT_TRUE(slide_dim->readOpenslide);
}

TEST(getSlideLevelDim, cropping_progressive) {
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
  request.cropFrameToGenerateUniformPixelSpacing = true;
  request.downsamples = std::move(downsamples);
  request.debug = true;
  wsiToDicomConverter::WsiToDcm converter(&request);
  std::vector<int32_t> levels;
  std::unique_ptr<wsiToDicomConverter::SlideLevelDim> smallestSlideDim;
  std::unique_ptr<wsiToDicomConverter::SlideLevelDim> slide_dim;
  smallestSlideDim = std::move(converter.getSmallestSlideDim(&levels));
  ASSERT_EQ(levels.size(), 8);
  ASSERT_EQ(smallestSlideDim->levelWidthDownsampled, 17);
  ASSERT_EQ(smallestSlideDim->levelHeightDownsampled, 23);
  slide_dim = std::move(converter.getSlideLevelDim(0,
                                                   NULL,
                                                   smallestSlideDim.get(),
                                                   false));
  slide_dim = std::move(converter.getSlideLevelDim(1,
                                                   slide_dim.get(),
                                                   smallestSlideDim.get(),
                                                   true));
  ASSERT_EQ(slide_dim->levelWidthDownsampled,  1088);
  ASSERT_EQ(slide_dim->levelHeightDownsampled, 1472);
  ASSERT_EQ(slide_dim->levelToGet, 0);
  ASSERT_EQ(slide_dim->level, 1);
  ASSERT_EQ(slide_dim->downsample, 2);
  ASSERT_EQ(slide_dim->multiplicator, 1);
  ASSERT_EQ(slide_dim->downsampleOfLevel, 2);
  ASSERT_EQ(slide_dim->levelWidth, 2176);
  ASSERT_EQ(slide_dim->levelHeight, 2944);
  ASSERT_EQ(slide_dim->levelFrameWidth, 25);
  ASSERT_EQ(slide_dim->levelFrameHeight, 25);
  ASSERT_EQ(slide_dim->frameWidthDownsampled, 50);
  ASSERT_EQ(slide_dim->frameHeightDownsampled, 50);
  ASSERT_EQ(slide_dim->cropSourceLevelWidth, 0);
  ASSERT_EQ(slide_dim->cropSourceLevelHeight, 0);
  ASSERT_FALSE(slide_dim->readOpenslide);
}


TEST(getSlideLevelDim, largest_when_levels_are_out_of_order) {
  std::vector<int>  downsamples;
  downsamples.push_back(16);  // Level = 0
  downsamples.push_back(8);   // Level = 1
  downsamples.push_back(4);   // Level = 2
  downsamples.push_back(32);  // Level = 3
  downsamples.push_back(1);   // Level = 4
  downsamples.push_back(2);   // Level = 5
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
  request.retileLevels = 6;
  request.stopDownsamplingAtSingleFrame = true;
  request.preferProgressiveDownsampling = true;
  request.cropFrameToGenerateUniformPixelSpacing = true;
  request.downsamples = std::move(downsamples);
  request.debug = true;
  wsiToDicomConverter::WsiToDcm converter(&request);
  std::vector<int32_t> levels;
  std::unique_ptr<wsiToDicomConverter::SlideLevelDim> slide_dim;
  std::unique_ptr<wsiToDicomConverter::SlideLevelDim> smallestSlideDim;
  smallestSlideDim = std::move(converter.getSmallestSlideDim(&levels));
  ASSERT_EQ(levels.size(), 6);
  // levels correspond to position in downsamples vector
  int expected_levels[] = {4, 5, 2, 1, 0, 3};
  for (int level = 0; level < 6; ++level) {
      ASSERT_EQ(expected_levels[level], levels[level]);
  }
  // -1 = get largest slice.
  ASSERT_EQ(smallestSlideDim->levelWidthDownsampled, 69);
  ASSERT_EQ(smallestSlideDim->levelHeightDownsampled, 92);
  slide_dim = std::move(converter.getSlideLevelDim(-1,
                                                   NULL,
                                                   smallestSlideDim.get(),
                                                   false));
  ASSERT_EQ(slide_dim->levelWidthDownsampled, 2208);
  ASSERT_EQ(slide_dim->levelHeightDownsampled, 2944);
  ASSERT_EQ(slide_dim->levelToGet, 0);
  ASSERT_EQ(slide_dim->downsample, 1);
  ASSERT_EQ(slide_dim->level, 0);
  ASSERT_EQ(slide_dim->multiplicator, 1);
  ASSERT_EQ(slide_dim->downsampleOfLevel, 1);
  ASSERT_EQ(slide_dim->levelWidth, 2220);
  ASSERT_EQ(slide_dim->levelHeight, 2967);
  ASSERT_EQ(slide_dim->levelFrameWidth, 100);
  ASSERT_EQ(slide_dim->levelFrameHeight, 100);
  ASSERT_EQ(slide_dim->frameWidthDownsampled, 100);
  ASSERT_EQ(slide_dim->frameHeightDownsampled, 100);
  // 2220 = width of source imaging being downsampled
  // 2208 = width of smallestSlideDim scaled to source level(0)
  // downsampling factor. 2208 = 69 (width pixels) * 32 (downsample factor)
  ASSERT_EQ(slide_dim->cropSourceLevelWidth, 2220 - 2208);
  // 2967 = height of source imaging being downsampled
  // 2944 = height of smallestSlideDim scaled to source level(0)
  // downsampling factor. 2944 = 92 (height pixels) * 32 (downsample factor)
  ASSERT_EQ(slide_dim->cropSourceLevelHeight, 2967 - 2944);
  ASSERT_TRUE(slide_dim->readOpenslide);
}

