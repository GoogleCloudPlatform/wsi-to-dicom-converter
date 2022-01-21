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
#include <boost/log/trivial.hpp>

#include <jpeglib.h>
#include <csetjmp>
#include <memory>
#include <utility>

#include "src/jpegUtil.h"

namespace jpegUtil {

struct jpegErrorManager {
  /* "public" fields */
  struct jpeg_error_mgr pub;
  /* for return to caller */
  jmp_buf setjmp_buffer;
};

char jpegLastErrorMsg[JMSG_LENGTH_MAX];

void jpegErrorExit(j_common_ptr cinfo) {
  // cinfo->err actually points to a jpegErrorManager struct
  jpegErrorManager* myerr = reinterpret_cast<jpegErrorManager*>(cinfo->err);
  // note : *(cinfo->err) is now equivalent to myerr->pub
  // output_message is a method to print an error message
  //(* (cinfo->err->output_message) ) (cinfo);
  // Create the message
  (*(cinfo->err->format_message))(cinfo, jpegLastErrorMsg);
  // Jump to the setjmp point
  longjmp(myerr->setjmp_buffer, 1);
}

std::unique_ptr<uint8_t[]> decodedJpeg(const int64_t width,
                                       const int64_t height,
                                       const J_COLOR_SPACE colorSpace,
                                       uint8_t* rawBuffer,
                                       const uint64_t rawBufferSize,
                                       uint64_t *decodedImageSizeBytes,
                                       uint8_t *returnMemoryBuffer,
                                       const int64_t returnMemoryBufferSize) {
  struct jpeg_decompress_struct cinfo;
  jpegErrorManager jerr;
  *decodedImageSizeBytes = 0;
  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = jpegErrorExit;
  // Establish the setjmp return context for my_error_exit to use.
  if (setjmp(jerr.setjmp_buffer)) {
    // If we get here, the JPEG code has signaled an error.
    BOOST_LOG_TRIVIAL(error) <<  "Error occured decompressing jpeg: " <<
                                  jpegLastErrorMsg;
    jpeg_destroy_decompress(&cinfo);
    return nullptr;
  }
  jpeg_create_decompress(&cinfo);
  jpeg_mem_src(&cinfo, rawBuffer, rawBufferSize);
  int rc = jpeg_read_header(&cinfo, TRUE);
  cinfo.jpeg_color_space = colorSpace;
  if (rc == 0) {
    BOOST_LOG_TRIVIAL(error) <<  "Not valid jpeg.";
    jpeg_destroy_decompress(&cinfo);
    return nullptr;
  }
  jpeg_start_decompress(&cinfo);
  const int row_stride = width * 3;
  const int bgrBufferSize = height * row_stride;
  std::unique_ptr<uint8_t[]> bmp_buffer =
                                    std::make_unique<uint8_t[]>(bgrBufferSize);
  while (cinfo.output_scanline < cinfo.output_height) {
  unsigned char *buffer_array[1];
  buffer_array[0] = bmp_buffer.get() + (cinfo.output_scanline) * row_stride;
    jpeg_read_scanlines(&cinfo, buffer_array, 1);
  }
  jpeg_finish_decompress(&cinfo);
  jpeg_destroy_decompress(&cinfo);

  *decodedImageSizeBytes = height * width * 4;
  std::unique_ptr<uint8_t[]> abgrBuffer = nullptr;
  if (returnMemoryBuffer == nullptr) {
    abgrBuffer = std::make_unique<uint8_t[]>(*decodedImageSizeBytes);
    returnMemoryBuffer = abgrBuffer.get();
  } else if (*decodedImageSizeBytes > returnMemoryBufferSize) {
      // size of memory buffer passed in != decoding size.
      // set returned memory size to 0 and return nullptr
      *decodedImageSizeBytes = 0;
      return nullptr;
  }
  uint64_t sourceCounter = 0;
  uint64_t destCounter = 0;
  const uint64_t sourceSize = height * width * 3;

  // Aperio imaging encoded with colorspace == JCS_RGB
  // requires colorspace  setting for correct decoding.
  // imaging is BGR.  Does not require byte reording.

  for (uint64_t sourceCounter = 0;
      sourceCounter < sourceSize;
      sourceCounter += 3) {
    // color generated in BGR ordering
    const uint8_t blue = bmp_buffer[sourceCounter];
    const uint8_t green = bmp_buffer[sourceCounter+1];
    const uint8_t red = bmp_buffer[sourceCounter+2];
    returnMemoryBuffer[destCounter] = blue;
    returnMemoryBuffer[destCounter+1] = green;
    returnMemoryBuffer[destCounter+2] = red;
    returnMemoryBuffer[destCounter+3] = 0xff;  // alpha
    destCounter += 4;
  }
  if (abgrBuffer != nullptr) {
    return std::move(abgrBuffer);
  }
  return nullptr;
}

bool canDecodeJpeg(const int64_t width, const int64_t height,
                   const J_COLOR_SPACE colorSpace,
                   uint8_t* rawBuffer, const uint64_t rawBufferSize) {
  uint64_t bufferSize;
  if (decodedJpeg(width, height, colorSpace, rawBuffer, rawBufferSize,
                  &bufferSize) != nullptr) {
    return true;
  }
  return false;
}

}  // namespace jpegUtil
