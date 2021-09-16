// Copyright 2021 Google LLC
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

#include <memory>
#include <utility>
#include <vector>

#include "src/dcmFileDraft.h"
#include "src/dicom_file_region_reader.h"
#include "src/frame.h"
#include "tests/test_frame.h"
#include "tests/testUtils.h"

namespace wsiToDicomConverter {

TEST(DICOMFileRegionReader, base_test) {
  std::vector<std::unique_ptr<DcmFileDraft>> dcm_file_vec;
  std::vector<std::unique_ptr<Frame>> framesData;
  framesData.push_back(std::move(std::make_unique<TestFrame>(1, 1, 1)));
  std::unique_ptr<DcmFileDraft> dcm_file = std::make_unique<DcmFileDraft>(
      std::move(framesData), "./", 1, 1, 0, 0, 0, "study", "series", "image",
      JPEG, true, nullptr, 0.0, 0.0, 1, &dcm_file_vec);
  dcm_file_vec.push_back(std::move(dcm_file));
  uint32_t mem[1] = { 9 };

  DICOMFileFrameRegionReader region_reader;
  ASSERT_EQ(region_reader.dicom_file_count(), 0);
  ASSERT_FALSE(region_reader.read_region(0, 0, 1, 1, mem));

  region_reader.set_dicom_files(std::move(dcm_file_vec));
  ASSERT_EQ(region_reader.dicom_file_count(), 1);
  ASSERT_TRUE(region_reader.read_region(0, 0, 1, 1, mem));

  region_reader.clear_dicom_files();
  ASSERT_EQ(region_reader.dicom_file_count(), 0);
  ASSERT_FALSE(region_reader.read_region(0, 0, 1, 1, mem));
}

TEST(DICOMFileRegionReader, read_region) {
  std::vector<std::unique_ptr<Frame>> framesData;
  for (int index = 1; index <= 4; ++index) {
    framesData.push_back(std::move(std::make_unique<TestFrame>(1, 1, index)));
  }
  std::vector<std::unique_ptr<DcmFileDraft>> dcm_file_vec;
  std::unique_ptr<DcmFileDraft> dcm_file = std::make_unique<DcmFileDraft>(
      std::move(framesData), "./", 2, 2, 0, 0, 0, "study", "series", "image",
      JPEG, true, nullptr, 0.0, 0.0, 3, &dcm_file_vec);

  dcm_file_vec.push_back(std::move(dcm_file));

  DICOMFileFrameRegionReader region_reader;
  region_reader.set_dicom_files(std::move(dcm_file_vec));
  ASSERT_EQ(region_reader.dicom_file_count(), 1);
  uint32_t mem[5] = { 9, 9, 9, 9, 9 };
  ASSERT_TRUE(region_reader.read_region(0, 0, 2, 2, mem));
  uint32_t test_mem[5] = {1, 2, 3, 4, 9 };
  for (size_t idx = 0; idx < 5; ++idx) {
    ASSERT_EQ(test_mem[idx], mem[idx]);
  }
}

TEST(DICOMFileRegionReader, read_beyond_region) {
  std::vector<std::unique_ptr<Frame>> framesData;
  for (int index = 1; index <= 4; ++index) {
    framesData.push_back(std::move(std::make_unique<TestFrame>(1, 1, index)));
  }
  std::vector<std::unique_ptr<DcmFileDraft>> dcm_file_vec;
  std::unique_ptr<DcmFileDraft> dcm_file = std::make_unique<DcmFileDraft>(
      std::move(framesData), "./", 2, 2, 0, 0, 0, "study", "series", "image",
      JPEG, true, nullptr, 0.0, 0.0, 3, &dcm_file_vec);
  dcm_file_vec.push_back(std::move(dcm_file));

  DICOMFileFrameRegionReader region_reader;
  region_reader.set_dicom_files(std::move(dcm_file_vec));
  ASSERT_EQ(region_reader.dicom_file_count(), 1);
  uint32_t mem[9] = { 9, 9, 9, 9, 9, 9, 9, 9, 9 };
  ASSERT_TRUE(region_reader.read_region(0, 0, 3, 3, mem));
  uint32_t test_mem[9] = {1, 2, 0,  3, 4, 0, 0, 0, 0 };
  for (size_t idx = 0; idx < 9; ++idx) {
      ASSERT_EQ(test_mem[idx], mem[idx]);
  }
}

TEST(DICOMFileRegionReader, read_sub_region1) {
  std::vector<std::unique_ptr<Frame>> framesData;
  for (int index = 1; index <= 4; ++index) {
    framesData.push_back(std::move(std::make_unique<TestFrame>(2, 2, index)));
  }
  std::vector<std::unique_ptr<DcmFileDraft>> dcm_file_vec;
  std::unique_ptr<DcmFileDraft> dcm_file = std::make_unique<DcmFileDraft>(
      std::move(framesData), "./", 4, 4, 0, 0, 0, "study", "series", "image",
      JPEG, true, nullptr, 0.0, 0.0, 5, &dcm_file_vec);
  dcm_file_vec.push_back(std::move(dcm_file));

  DICOMFileFrameRegionReader region_reader;
  region_reader.set_dicom_files(std::move(dcm_file_vec));
  ASSERT_EQ(region_reader.dicom_file_count(), 1);
  uint32_t mem[9] = { 9, 9, 9, 9, 9, 9, 9, 9, 9 };
  ASSERT_TRUE(region_reader.read_region(1, 1, 3, 3, mem));
  uint32_t test_mem[9] = {1, 2, 2,  3, 4, 4, 3, 4, 4 };
  for (size_t idx = 0; idx < 9; ++idx) {
      ASSERT_EQ(test_mem[idx], mem[idx]);
  }
}

TEST(DICOMFileRegionReader, read_sub_region2) {
  std::vector<std::unique_ptr<Frame>> framesData;
  for (int index = 1; index <= 4; ++index) {
    framesData.push_back(std::move(std::make_unique<TestFrame>(2, 2, index)));
  }
  std::vector<std::unique_ptr<DcmFileDraft>> dcm_file_vec;
  std::unique_ptr<DcmFileDraft> dcm_file = std::make_unique<DcmFileDraft>(
      std::move(framesData), "./", 4, 4, 0, 0, 0, "study", "series", "image",
      JPEG, true, nullptr, 0.0, 0.0, 5, &dcm_file_vec);
  dcm_file_vec.push_back(std::move(dcm_file));

  DICOMFileFrameRegionReader region_reader;
  region_reader.set_dicom_files(std::move(dcm_file_vec));
  ASSERT_EQ(region_reader.dicom_file_count(), 1);
  uint32_t mem[9] = { 9, 9, 9, 9, 9, 9, 9, 9, 9 };
  ASSERT_TRUE(region_reader.read_region(0, 0, 3, 3, mem));
  uint32_t test_mem[9] = {1, 1, 2,  1, 1, 2, 3, 3, 4 };
  for (size_t idx = 0; idx < 9; ++idx) {
      ASSERT_EQ(test_mem[idx], mem[idx]);
  }
}

TEST(DICOMFileRegionReader, read_multi_files) {
  std::vector<std::unique_ptr<Frame>> framesData;
  std::vector<std::unique_ptr<DcmFileDraft>> dcm_file_vec;
  for (int index = 1; index <= 4; ++index) {
    framesData.push_back(std::move(std::make_unique<TestFrame>(2, 2, index)));
    std::unique_ptr<DcmFileDraft> dcm_file = std::make_unique<DcmFileDraft>(
      std::move(framesData), "./", 4, 4, 0, 0, 0, "study", "series", "image",
      JPEG, true, nullptr, 0.0, 0.0, 5, &dcm_file_vec);
    dcm_file_vec.push_back(std::move(dcm_file));
  }

  DICOMFileFrameRegionReader region_reader;
  region_reader.set_dicom_files(std::move(dcm_file_vec));
  ASSERT_EQ(region_reader.dicom_file_count(), 4);
  uint32_t mem[9] = { 9, 9, 9, 9, 9, 9, 9, 9, 9 };
  ASSERT_TRUE(region_reader.read_region(1, 1, 3, 3, mem));
  uint32_t test_mem[9] = {1, 2, 2,  3, 4, 4, 3, 4, 4 };
  for (size_t idx = 0; idx < 9; ++idx) {
      ASSERT_EQ(test_mem[idx], mem[idx]);
  }
}

}  // namespace wsiToDicomConverter
