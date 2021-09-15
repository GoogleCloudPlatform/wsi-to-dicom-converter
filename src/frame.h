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

#ifndef SRC_FRAME_H_
#define SRC_FRAME_H_

namespace wsiToDicomConverter {

// Frame represents a single image frame from the OpenSlide library
class Frame {
 public:
  virtual ~Frame() {}

  // Gets frame by openslide library, performs scaling it and compressing
  virtual void sliceFrame() = 0;
  virtual bool isDone() const = 0;
  virtual uint8_t *get_dicom_frame_bytes() = 0;
  virtual size_t getSize() const = 0;
  virtual int64_t get_raw_frame_bytes(uint8_t *raw_memory,
                                      int64_t memorysize) const = 0;
  virtual int64_t get_frame_width() const = 0;
  virtual int64_t get_frame_height() const = 0;
  virtual void clear_dicom_mem() = 0;
  virtual bool has_compressed_raw_bytes() const = 0;
};

}  // namespace wsiToDicomConverter

#endif  // SRC_FRAME_H_
