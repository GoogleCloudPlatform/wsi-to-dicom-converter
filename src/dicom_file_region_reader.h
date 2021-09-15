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

#ifndef SRC_DICOM_FILE_REGION_READER_H_
#define SRC_DICOM_FILE_REGION_READER_H_

#include <memory>
#include <vector>

#include "src/dcmFileDraft.h"

namespace wsiToDicomConverter {

/*
DICOMFileFrameRegionReader provides an mechanism similar to OpenSlide 
openslide_read_region to extract an aribrary two dimensional patch of
memory from an an array of DICOMFrame objects.

DICOM frame array can be spread across multiple DICOM files.
Code assumes that frames are ordered using column ordering (tradtional C++)
memory layout and that if the frames flow across multiple DICOM files that
frames flow as if they were a continous block of memory.

Frame 1, 2, 3
      4, 5, 6  = [1, 2, 3, 4, 5, 6, 7 ,8 , 9]
      7, 8, 9 
*/
class DICOMFileFrameRegionReader {
 public :
  DICOMFileFrameRegionReader();
  virtual ~DICOMFileFrameRegionReader();

  // Number of DICOM files loaded in current instance of
  // DICOMFileFrameRegionReader.
  int64_t dicom_file_count() const;

  // Returns pointer to specified DICOM file.
  DcmFileDraft * get_dicom_file(size_t index);

  // Set DICOM list of files to be used by frame region reader.
  // Should be the complete set for given level.
  // All DICOM files must describe the same overall image, have
  // same global image width, height, and have frames which have same
  // dimensions.
  void set_dicom_files(std::vector<std::unique_ptr<DcmFileDraft>> dcm_files_);

  // Clears the list of dicm files.
  void clear_dicom_files();

  // Reads a sub region from the a set of DICOM frames spread across file(s).
  //
  // Memory pixels in ARGB format.
  // Memory pixel value = 0x00000000 for positions outside image dim.
  //
  // Args:
  //   layer_X : upper left X coordinate in image coordinates.
  //   layer_Y : upper left Y coordinate in image coordinates.
  //   mem_width : Width of memory to copy into.
  //   mem_height : Height of memory to copy into.
  //   memory : Memory to copy into .
  //
  // Returns: True if has files, false if no DICOM files set.
  bool read_region(int64_t layer_x, int64_t layer_y, int64_t mem_width,
                   int64_t mem_height, uint32_t *memory) const;

 private:
  // Reads a frame from as set of loaded DICOM files.
  //
  // Args:
  //  index : index of frame to read.
  //  frame_memory : memory buffer to read frame into
  //  frame_buffer_size_bytes : size of buffer in bytes
  //
  // Returns:
  //   true if frame memory initalized
  bool get_frame_bytes(int64_t index, uint32_t* frame_memory,
                       const int64_t frame_buffer_size_bytes) const;

  // Copies a memory region from a frame memory to memory buffer.
  //
  // Args:
  //   image_offset_X : global upper left coordinate in image
  //   image_offset_Y : global upper left coordinate in image
  //   frame_bytes: frame pixel memory
  //   fx : upper left coordinate of frame
  //   fy : upper left coordinate of frame
  //   copy_width : width to copy from frame
  //   copy_height : height to copy frome frame
  //   memory : pixel memory to copy frame data to.
  //   memory_width : total width of memory block.
  //   mx : upper left coordinate in memory to read from
  //   my : upper left coordinate in memory to read from
  void copy_region_from_frames(int64_t image_offset_X, int64_t image_offset_Y,
                               const uint32_t * const frame_bytes, int64_t fx,
                               int64_t fy, int64_t copy_width,
                               int64_t copy_height, uint32_t *memory,
                               int64_t memory_width, int64_t mx, int64_t my)
                               const;

  std::vector<std::unique_ptr<DcmFileDraft>> dcmFiles_;

  // Dimensions of single frame
  int64_t frameWidth_, frameHeight_;

  // Dimensions of image
  int64_t imageWidth_, imageHeight_;

  // # frames per image row & column
  int64_t framesPerRow_, framesPerColumn_;
};

}  // namespace wsiToDicomConverter

#endif  // SRC_DICOM_FILE_REGION_READER_H_
