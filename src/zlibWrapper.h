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

#ifndef SRC_ZLIBWRAPPER_H_
#define SRC_ZLIBWRAPPER_H_

#include <memory>

namespace wsiToDicomConverter {

 /* ZLib compresses memory and returns compressed bytes.

  Args:
    raw_bytes : *to memory to compress
    frame_mem_size_bytes : memory size in bytes
    raw_compressed_bytes_size : size in bytes of compressed bits.

  Returns:
    compressed memory. 
*/
std::unique_ptr<uint8_t[]> compress_memory(uint8_t *raw_bytes,
                                           const int64_t frame_mem_size_bytes,
                                           int64_t *raw_compressed_bytes_size);

/* ZLib decompresses memory.

  Args:
    compressed_bytes : *to compressed memory
    compressed_bytes_size : size of compressed memory,
    raw_memory : memory to return uncompressed data into
    raw_memory_size_bytes : size of raw_memory (bytes).

  Returns:
    Actual size in bytes of data uncompressed.
*/
int64_t decompress_memory(uint8_t *compressed_bytes,
                          int64_t compressed_bytes_size,
                          uint8_t* raw_memory,
                          int64_t raw_memory_size_bytes);

}  // namespace wsiToDicomConverter

#endif  // SRC_ZLIBWRAPPER_H_
