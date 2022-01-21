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
#include "src/tiffFile.h"

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
  int64_t dicomFileCount() const;

  // Returns pointer to specified DICOM file.
  DcmFileDraft * dicomFile(size_t index);

  // Set DICOM list of files to be used by frame region reader.
  // Should be the complete set for given level.
  // All DICOM files must describe the same overall image, have
  // same global image width, height, and have frames which have same
  // dimensions.
  void setDicomFiles(std::vector<std::unique_ptr<DcmFileDraft>> dcmFiles,
                     std::unique_ptr<TiffFile> tiffFile);

  // Clears the list of dicm files.
  void clearDicomFiles();

  // Reads a sub region from the a set of DICOM frames spread across file(s).
  //
  // Memory pixels in ARGB format.
  // Memory pixel value = 0x00000000 for positions outside image dim.
  //
  // Args:
  //   layerX : upper left X coordinate in image coordinates.
  //   layerY : upper left Y coordinate in image coordinates.
  //   memWidth : Width of memory to copy into.
  //   memHeight : Height of memory to copy into.
  //   memory : Memory to copy into .
  //
  // Returns: True if has files, false if no DICOM files set.
  bool readRegion(int64_t layerX, int64_t layerY, int64_t memWidth,
                  int64_t memHeight, uint32_t *memory);

  bool incSourceFrameReadCounter(int64_t layerX, int64_t layerY,
                                 int64_t memWidth, int64_t memHeight);

 private:
  // Reads a frame from as set of loaded DICOM files.
  //
  // Args:
  //  index : index of frame to read.
  //  frameMemory : memory buffer to read frame into
  //  frameBufferSizeBytes : size of buffer in bytes
  //
  // Returns:
  //   true if frame memory initalized
  bool frameBytes(int64_t index, uint32_t* frameMemory,
                  const int64_t frameBufferSizeBytes);

  Frame* framePtr(int64_t index);

  // Copies a memory region from a frame memory to memory buffer.
  //
  // Args:
  //   imageOffsetX : global upper left coordinate in image
  //   imageOffsetY : global upper left coordinate in image
  //   frameBytes: frame pixel memory
  //   fx : upper left coordinate of frame
  //   fy : upper left coordinate of frame
  //   copyWidth : width to copy from frame
  //   copyHeight : height to copy frome frame
  //   memory : pixel memory to copy frame data to.
  //   memoryWidth : total width of memory block.
  //   mx : upper left coordinate in memory to read from
  //   my : upper left coordinate in memory to read from
  void copyRegionFromFrames(int64_t imageOffsetX, int64_t imageOffsetY,
                            const uint32_t * const frameBytes, int64_t fx,
                            int64_t fy, int64_t copyWidth,
                            int64_t copyHeight, uint32_t *memory,
                            int64_t memoryWidth, int64_t mx, int64_t my) const;

  void xyFrameSpan(int64_t layerX, int64_t layerY, int64_t memWidth,
                   int64_t memHeight, int64_t *firstFrameX,
                   int64_t *firstFrameY, int64_t *lastFrameX,
                   int64_t *lastFrameY);


  std::vector<std::unique_ptr<DcmFileDraft>> dcmFiles_;
  std::unique_ptr<TiffFile> tiffFile_;  // Tiff file used to gen DICOM Frames
                                        // nullptr if frames generated from
                                        // prior level or openslide.

  // Dimensions of single frame
  int64_t frameWidth_, frameHeight_;

  // Dimensions of image
  int64_t imageWidth_, imageHeight_;

  // # frames per image row & column
  int64_t framesPerRow_, framesPerColumn_;
};

}  // namespace wsiToDicomConverter

#endif  // SRC_DICOM_FILE_REGION_READER_H_
