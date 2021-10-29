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
#include <memory>

#include "tests/test_frame.h"

namespace wsiToDicomConverter {

TestFrame::TestFrame(int64_t width, int64_t height) {
  width_ = width;
  height_ = height;
  rawValue_ = NULL;
}

TestFrame::TestFrame(int64_t width, int64_t height, uint32_t value) {
  width_ = width;
  height_ = height;
  rawValue_ = std::make_unique<uint32_t[]>(width * height);
  for (size_t idx = 0; idx < width * height; ++idx) {
      rawValue_[idx] = value;
  }
}

void TestFrame::sliceFrame() {}

bool TestFrame::isDone() const {
  return true;
}

uint8_t *TestFrame::get_dicom_frame_bytes() {
  return NULL;
}

size_t TestFrame::getSize() const {
  return 0;
}

int64_t TestFrame::get_raw_frame_bytes(uint8_t *raw_memory,
                                       int64_t memorysize) const {
  const int64_t expected_memsize = width_ * height_ * sizeof(uint32_t);
  if (memorysize != expected_memsize) {
    return 0;
  }
  uint8_t *ptr_raw_value = reinterpret_cast<uint8_t *>(rawValue_.get());
  std::memcpy(raw_memory, ptr_raw_value, expected_memsize);
  return (expected_memsize);
}

int64_t TestFrame::get_frame_width() const {
  return width_;
}

int64_t TestFrame::get_frame_height() const {
  return height_;
}

void TestFrame::clear_dicom_mem() {}

bool TestFrame::has_compressed_raw_bytes() const {
  return true;
}

}  // namespace wsiToDicomConverter
