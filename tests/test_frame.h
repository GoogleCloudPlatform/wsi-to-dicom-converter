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
#ifndef TESTS_TEST_FRAME_H_
#define TESTS_TEST_FRAME_H_
#include <atomic>
#include <memory>

#include "src/frame.h"

namespace wsiToDicomConverter {

class TestFrame : public Frame {
 public:
  TestFrame(int64_t width, int64_t height);
  TestFrame(int64_t width, int64_t height, uint32_t value);
  virtual void sliceFrame();

  virtual int64_t rawABGRFrameBytes(uint8_t *rawMemory, int64_t memorySize);
  virtual bool hasRawABGRFrameBytes() const;

  virtual void incSourceFrameReadCounter();

 private:
  std::unique_ptr<uint32_t[]> rawValue_;
};

}  // namespace wsiToDicomConverter

#endif  // TESTS_TEST_FRAME_H_
