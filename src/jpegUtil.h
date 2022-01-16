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
#ifndef SRC_JPEGUTIL_H_
#define SRC_JPEGUTIL_H_

#include <memory>

namespace jpegUtil {

bool canDecodeJpeg(const int64_t width, const int64_t height,
                   const J_COLOR_SPACE colorSpace,
                   uint8_t* rawBuffer, const uint64_t rawBufferSize);

std::unique_ptr<uint8_t[]> decodedJpeg(const int64_t width,
                                       const int64_t height,
                                       const J_COLOR_SPACE colorSpace,
                                       uint8_t* rawBuffer,
                                       const uint64_t rawBufferSize,
                                       uint64_t *decodedImageSizeBytes,
                                       uint8_t *returnMemoryBuffer = nullptr,
                                     const int64_t returnMemoryBufferSize = 0);

}  // namespace jpegUtil

#endif  // SRC_JPEGUTIL_H_
