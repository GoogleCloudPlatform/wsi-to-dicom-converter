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
#include <boost/log/trivial.hpp>

#include <cstring>
#include <memory>

#include "tests/test_frame.h"

namespace wsiToDicomConverter {

TestFrame::TestFrame(int64_t width, int64_t height) : Frame(0,
                                                            0,
                                                            width,
                                                            height,
                                                            RAW,
                                                            100,
                                                            true) {
  rawValue_ = NULL;
  done_ = true;
  data_  = NULL;
  size_ = 0;
}

TestFrame::TestFrame(int64_t width, int64_t height, uint32_t value) : Frame(0,
                                                                            0,
                                                                        width,
                                                                       height,
                                                                          RAW,
                                                                          100,
                                                                        true) {
  done_ = true;
  data_  = NULL;
  size_ = 0;
  rawValue_ = std::make_unique<uint32_t[]>(width * height);
  for (size_t idx = 0; idx < width * height; ++idx) {
      rawValue_[idx] = value;
  }
}

void TestFrame::incSourceFrameReadCounter() {
}

void TestFrame::sliceFrame() {}

int64_t TestFrame::rawABGRFrameBytes(uint8_t *raw_memory,
                                       int64_t memorysize) {
  const int64_t expected_memsize = frameWidth_ *
                                   frameHeight_ *
                                   sizeof(uint32_t);
  if (memorysize != expected_memsize) {
    return 0;
  }
  uint8_t *ptr_raw_value = reinterpret_cast<uint8_t *>(rawValue_.get());
  std::memcpy(raw_memory, ptr_raw_value, expected_memsize);
  return (expected_memsize);
}

bool TestFrame::has_compressed_raw_bytes() const {
  return true;
}

}  // namespace wsiToDicomConverter
