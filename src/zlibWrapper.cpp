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
// See the License for the specific la
#include <boost/log/trivial.hpp>
#include <zlib.h>

#include <string>
#include <utility>

#include "src/zlibWrapper.h"

namespace wsiToDicomConverter {

std::unique_ptr<uint8_t[]> get_compressed_bytes(const z_stream &zs,
                                      const unsigned char * const outbuffer) {
  if (zs.total_out == 0) {
      return NULL;
    } else {
      std::unique_ptr<uint8_t[]> raw_compressed_bytes =
                                    std::make_unique<uint8_t[]>(zs.total_out);
      std::memcpy(raw_compressed_bytes.get(), outbuffer, zs.total_out);
      return std::move(raw_compressed_bytes);
    }
}

std::unique_ptr<uint8_t[]>  compress_memory(uint8_t *raw_bytes,
                                          const int64_t frame_mem_size_bytes,
                                          int64_t *raw_compressed_bytes_size) {
  /* ZLib compresses memory and returns compressed bytes.

  Args:
    raw_bytes : *to memory to compress
    frame_mem_size_bytes : memory size in bytes
    raw_compressed_bytes_size : size in bytes of compressed bits.

  Returns:
    compressed memory.
  */

  if (raw_bytes == NULL || frame_mem_size_bytes == 0) {
    *raw_compressed_bytes_size = 0;
    return NULL;
  }
  // Initalize ZLib
  z_stream zs;
  memset(&zs, 0, sizeof(zs));
  deflateInit(&zs, Z_DEFAULT_COMPRESSION);
  zs.next_in = reinterpret_cast<Bytef*>(raw_bytes);
  zs.avail_in = frame_mem_size_bytes;
  // Use fixed buffer to compress
  unsigned char outbuffer[512 * 512 * 4 * 2];
  zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
  zs.avail_out = sizeof(outbuffer);
  int ret = deflate(&zs, Z_FINISH);

  std::unique_ptr<uint8_t[]> raw_compressed_bytes;
  // try to compress in one shot.
  if (ret != Z_OK) {
    raw_compressed_bytes = std::move(get_compressed_bytes(zs, outbuffer));
  } else {  // If larger buffer is required then build.
    std::basic_string<unsigned char> mem;
    mem.append(outbuffer, zs.total_out);
    while (ret == Z_OK) {
      zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
      zs.avail_out = sizeof(outbuffer);
      ret = deflate(&zs, Z_FINISH);
      if (zs.total_out != mem.size()) {
        mem.append(outbuffer, zs.total_out - mem.size());
      }
    }
    raw_compressed_bytes = std::move(get_compressed_bytes(zs, mem.c_str()));
  }
  // Set size of bits being returned
  *raw_compressed_bytes_size = zs.total_out;
  deflateEnd(&zs);
  return std::move(raw_compressed_bytes);
}

int64_t decompress_memory(uint8_t *compressed_bytes,
                          const int64_t compressed_bytes_size,
                          uint8_t* raw_memory,
                          const int64_t raw_memory_size_bytes) {
  /* ZLib decompresses memory.

  Args:
    compressed_bytes : *to compressed memory
    compressed_bytes_size : size of compressed memory,
    raw_memory : memory to return uncompressed data into
    raw_memory_size_bytes : size of raw_memory (bytes).

  Returns:
    Actual size in bytes of data uncompressed.
  */
  if (compressed_bytes == NULL || compressed_bytes_size == 0) {
    return 0;
  }
  z_stream zs;                        // z_stream is zlib's control structure
  memset(&zs, 0, sizeof(zs));
  inflateInit(&zs);
  zs.next_in = reinterpret_cast<Bytef*>(compressed_bytes);
  zs.avail_in = compressed_bytes_size;
  zs.next_out = reinterpret_cast<Bytef*>(raw_memory);
  zs.avail_out = raw_memory_size_bytes;
  inflate(&zs, 0);
  inflateEnd(&zs);
  return zs.total_out;
}

}  // namespace wsiToDicomConverter
