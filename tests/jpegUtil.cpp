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
#include <jpeglib.h>

#include <memory>
#include <utility>

#include "src/jpegUtil.h"
#include "src/tiffDirectory.h"
#include "src/tiffFile.h"
#include "tests/testUtils.h"

namespace wsiToDicomConverter {

TEST(jpegUtil, canDecodeJpegValidJpeg) {
  FILE *file = fopen("../tests/bone.jpeg", "rb");
  fseek(file , 0 , SEEK_END);
  uint64_t lSize = ftell(file);
  rewind(file);
  std::unique_ptr<uint8_t[]> jpegMem = std::make_unique<uint8_t[]>(lSize);
  fread(jpegMem.get(), lSize, 1, file);
  fclose(file);
  ASSERT_TRUE(jpegUtil::canDecodeJpeg(957, 715, JCS_RGB, jpegMem.get(), lSize));
}

TEST(jpegUtil, detectInvalidJpeg) {
  FILE *file = fopen("../tests/bone.jpeg", "rb");
  // obtain file size:
  fseek(file , 0 , SEEK_END);
  uint64_t lSize = ftell(file);
  rewind(file);
  std::unique_ptr<uint8_t[]> jpegMem = std::make_unique<uint8_t[]>(lSize);
  fread(jpegMem.get(), lSize, 1, file);
  fclose(file);
  ASSERT_TRUE(!jpegUtil::canDecodeJpeg(957, 715, JCS_RGB, &(jpegMem[100]),
                                                          lSize-100));
}

TEST(jpegUtil, decodeJpegValidJpeg) {
  FILE *file = fopen("../tests/bone.jpeg", "rb");
  // obtain file size:
  fseek(file , 0 , SEEK_END);
  uint64_t lSize = ftell(file);
  rewind(file);
  std::unique_ptr<uint8_t[]> jpegMem = std::make_unique<uint8_t[]>(lSize);
  fread(jpegMem.get(), lSize, 1, file);
  fclose(file);
  uint64_t *decodedImageSizeBytes;
  std::unique_ptr<uint8_t[]> returnMemoryBuffer =
                                    std::make_unique<uint8_t[]>(4*957*715 + 3);
  for (int idx = 0; idx < 4*957*715 + 3; ++idx) {
    returnMemoryBuffer[idx] = 0;
  }
  // add sential to end of buffer to detect overwrite.
  returnMemoryBuffer[4*957*715] = 0xba;
  returnMemoryBuffer[4*957*715+1] = 0xdf;
  returnMemoryBuffer[4*957*715+2] = 0x0d;
  uint64_t decodedBufferSize;
  std::unique_ptr<uint8_t[]> retVal =
                std::move(jpegUtil::decodedJpeg(957, 715, JCS_RGB,
                                                jpegMem.get(), lSize,
                                                &decodedBufferSize,
                                                returnMemoryBuffer.get(),
                                                4*957*715 + 3));
  // Test if return buffer was used nullptr is returned
  ASSERT_EQ(retVal, nullptr);
  // Test return buffer size
  ASSERT_EQ(decodedBufferSize, 4*957*715);
  // Test buffer contains non-zero value;
  bool foundNonZeroValue = false;
  for (int idx = 0; idx < 4*957*715; ++idx) {
    if (returnMemoryBuffer[idx] != 0) {
      foundNonZeroValue = true;
      break;
    }
  }
  ASSERT_TRUE(foundNonZeroValue);
  // Test Read into preallocated buffer does not overflow
  ASSERT_EQ(returnMemoryBuffer[4*957*715], 0xba);
  ASSERT_EQ(returnMemoryBuffer[4*957*715 + 1], 0xdf);
  ASSERT_EQ(returnMemoryBuffer[4*957*715 + 2], 0x0d);

  // Test read with no return buffer returns allocated memory that
  // equals what previously was returned.
  uint64_t secondReadBufferSize;
  std::unique_ptr<uint8_t[]> secondRead =
                std::move(jpegUtil::decodedJpeg(957, 715, JCS_RGB,
                          jpegMem.get(), lSize, &secondReadBufferSize));
  ASSERT_EQ(decodedBufferSize, secondReadBufferSize);
  for (int idx = 0; idx < 4*957*715; ++idx) {
    ASSERT_EQ(returnMemoryBuffer[idx], secondRead[idx]);
  }
}

}  // namespace wsiToDicomConverter
