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
#include <gtest/gtest.h>

#include <memory>
#include <random>
#include <utility>

#include "src/zlibWrapper.h"

namespace wsiToDicomConverter {

TEST(zlibWrapper, small_mem) {
  int64_t raw_memory_size_bytes = 10;

  std::unique_ptr<uint8_t[]> raw_memory =
                        std::make_unique<uint8_t[]>(raw_memory_size_bytes);
  std::unique_ptr<uint8_t[]> raw_memory_out =
                        std::make_unique<uint8_t[]>(raw_memory_size_bytes);

  for (size_t idx = 0; idx < raw_memory_size_bytes; idx++) {
      raw_memory[idx] = idx;
      raw_memory_out[idx] = -1;
  }

  int64_t raw_compressed_bytes_size;
  std::unique_ptr<uint8_t[]> compressed_bytes =
                        std::move(compress_memory(raw_memory.get(),
                                  raw_memory_size_bytes,
                                  &raw_compressed_bytes_size));
  ASSERT_EQ(decompress_memory(compressed_bytes.get(),
                              raw_compressed_bytes_size,
                              raw_memory_out.get(),
                              raw_memory_size_bytes), raw_memory_size_bytes);
  for (size_t idx = 0; idx < raw_memory_size_bytes; idx++) {
    ASSERT_EQ(raw_memory_out[idx], raw_memory[idx]);
  }
}

TEST(zlibWrapper, large_mem) {
  int64_t raw_memory_size_bytes = 512*512*80;
  std::unique_ptr<uint8_t[]> raw_memory =
                          std::make_unique<uint8_t[]>(raw_memory_size_bytes);
  std::unique_ptr<uint8_t[]> raw_memory_out =
                          std::make_unique<uint8_t[]>(raw_memory_size_bytes);
  std::default_random_engine generator;
  std::uniform_int_distribution<int> distribution(0, 255);
  for (size_t idx = 0; idx < raw_memory_size_bytes; idx++) {
      raw_memory[idx] = distribution(generator);
      raw_memory_out[idx] = -1;
  }
  int64_t raw_compressed_bytes_size;
  std::unique_ptr<uint8_t[]> compressed_bytes =
                        std::move(compress_memory(raw_memory.get(),
                                                  raw_memory_size_bytes,
                                                  &raw_compressed_bytes_size));
  ASSERT_EQ(decompress_memory(compressed_bytes.get(),
                              raw_compressed_bytes_size,
                              raw_memory_out.get(),
                              raw_memory_size_bytes),
             raw_memory_size_bytes);
  for (size_t idx = 0; idx < raw_memory_size_bytes; idx++) {
    ASSERT_EQ(raw_memory_out[idx], raw_memory[idx]);
  }
}

TEST(zlibWrapper, empty) {
  int64_t raw_compressed_bytes_size;
  const unsigned char* null_ptr = NULL;
  ASSERT_EQ(null_ptr,
            compress_memory(NULL, 0, &raw_compressed_bytes_size).get());
  ASSERT_EQ(raw_compressed_bytes_size, 0);
  int64_t raw_memory_size_bytes = 10;
  std::unique_ptr<uint8_t[]> raw_memory_out =
                        std::make_unique<uint8_t[]>(raw_memory_size_bytes);
  ASSERT_EQ(decompress_memory(NULL, 0, raw_memory_out.get(),
                              raw_memory_size_bytes), 0);
}

}  // namespace wsiToDicomConverter
