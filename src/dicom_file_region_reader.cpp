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

#include <algorithm>
#include <utility>

#include "src/dicom_file_region_reader.h"

namespace wsiToDicomConverter {

DICOMFileFrameRegionReader::DICOMFileFrameRegionReader() {
    clearDicomFiles();
}

DICOMFileFrameRegionReader::~DICOMFileFrameRegionReader() {
  clearDicomFiles();
}

AbstractDcmFile * DICOMFileFrameRegionReader::dicomFile(size_t index) {
  return dcmFiles_.at(index).get();
}

void DICOMFileFrameRegionReader::setDicomFiles(
                      std::vector<std::unique_ptr<AbstractDcmFile>> dcmFiles,
                      std::unique_ptr<TiffFile> tiffFile) {
  // all files should have frames with same dimensions
  clearDicomFiles();
  dcmFiles_ = std::move(dcmFiles);
  tiffFile_ = std::move(tiffFile);  // Tiff file used to gen DICOM Frames
                                    // nullptr if frames generated from
                                    // prior level or openslide.
  if (dcmFiles_.size() <= 0) {
    clearDicomFiles();
    return;
  }
  const AbstractDcmFile & first_dcm_file = (*dcmFiles_.at(0));
  if (!first_dcm_file.frame(0)->hasRawABGRFrameBytes()) {
    clearDicomFiles();
    return;
  }
  frameWidth_ = first_dcm_file.frameWidth();
  frameHeight_ = first_dcm_file.frameHeight();
  imageWidth_ = first_dcm_file.imageWidth();
  imageHeight_ = first_dcm_file.imageHeight();
  // compute frames per row and column.  Not final frames may
  // not be used completely.
  framesPerRow_ = static_cast<int64_t>(
                                  std::ceil(static_cast<double>(imageWidth_) /
                                  static_cast<double>(frameWidth_)));
  framesPerColumn_ = static_cast<int64_t>(
                                  std::ceil(static_cast<double>(imageHeight_) /
                                  static_cast<double>(frameHeight_)));
}

void DICOMFileFrameRegionReader::clearDicomFiles() {
  dcmFiles_.clear();
  tiffFile_ = nullptr;
  frameWidth_ = 0;
  frameHeight_ = 0;
  imageWidth_ = 0;
  imageHeight_ = 0;
  framesPerRow_ = 0;
  framesPerColumn_ = 0;
}

int64_t DICOMFileFrameRegionReader::dicomFileCount() const {
  return dcmFiles_.size();
}

Frame* DICOMFileFrameRegionReader::framePtr(int64_t index) {
    // Reads a frame from as set of loaded dicom files.
    //
    // Args:
    //  index : index of frame to read.
    //
    // Returns:
    //   pointer to frame
    const int64_t fileCount = dcmFiles_.size();
    for (int64_t fileIdx = 0; fileIdx < fileCount; ++fileIdx) {
      const AbstractDcmFile & dcmFile = (*dcmFiles_.at(fileIdx));
      const int64_t frameCount = dcmFile.fileFrameCount();
      if (index >= frameCount) {
        index -= frameCount;
      } else {
        return dcmFile.frame(index);
      }
    }
    return nullptr;
  }

bool DICOMFileFrameRegionReader::frameBytes(int64_t index,
                                            uint32_t* frameMemory,
                                const int64_t frameBufferSizeBytes) {
    // Reads a frame from as set of loaded dicom files.
    //
    // Args:
    //  index : index of frame to read.
    //  frameMemory : memory buffer to read frame into
    //  frameBufferSizeBytes : size of buffer in bytes
    //
    // Returns:
    //   true if frame memory initalized
    Frame* fptr = framePtr(index);
    if (fptr != nullptr) {
        return fptr->rawABGRFrameBytes(
                reinterpret_cast<uint8_t *>(frameMemory),
                                            frameBufferSizeBytes) ==
                                            frameBufferSizeBytes;
    }
    return false;
  }

  void DICOMFileFrameRegionReader::copyRegionFromFrames(
                    int64_t imageOffsetX, int64_t imageOffsetY,
                    const uint32_t * const frameBytes, int64_t fx, int64_t fy,
                    int64_t copyWidth, int64_t copyHeight, uint32_t *memory,
                    int64_t memoryWidth, int64_t mx, int64_t my) const {
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

      const int64_t endMX = mx + copyWidth;  // end position to copy to
      int64_t myOffset = my * memoryWidth;   // y offset to start writing to
      int64_t frameOffset = fy * frameWidth_;  // y offset to read from.
      int64_t imageY = imageOffsetY;         // global position in image
      for (int64_t row = 0; row < copyHeight; ++row) {
        int64_t frameCursor = fx + frameOffset;  // frame memory read index
        for (int64_t column = mx; column < endMX; ++column) {
            int64_t imageX = column + imageOffsetX;
            if (imageX >= imageWidth_ ||   // if frame memory is nullptr or
                imageY >= imageHeight_ ||  // outside of image bounds
                frameBytes == nullptr) {  // set copied pixel memory ARGB to 0
              memory[column + myOffset] = 0;
            } else {   // otherwise copy memory
              memory[column + myOffset] = frameBytes[frameCursor];
            }
            frameCursor += 1;  // increment frame read index
        }
        myOffset += memoryWidth;  // increment row offsets
        frameOffset += frameWidth_;
        imageY  += 1;
      }
  }

  void DICOMFileFrameRegionReader::xyFrameSpan(int64_t layerX,
                                               int64_t layerY,
                                               int64_t memWidth,
                                               int64_t memHeight,
                                               int64_t *firstFrameX,
                                               int64_t *firstFrameY,
                                               int64_t *lastFrameX,
                                               int64_t *lastFrameY) {
    // compute first and last frames to read.
    *firstFrameY = (layerY / frameHeight_);
    *firstFrameX = (layerX / frameWidth_);
    *lastFrameY = static_cast<int64_t>(static_cast<double>(
          std::min<int64_t>(layerY + memHeight - 1, imageHeight_)) /
          static_cast<double>(frameHeight_));
    *lastFrameX = static_cast<int64_t>(static_cast<double>(
          std::min<int64_t>(layerX + memWidth - 1, imageWidth_)) /
          static_cast<double>(frameWidth_));
  }

  bool DICOMFileFrameRegionReader::incSourceFrameReadCounter(int64_t layerX,
                                               int64_t layerY,
                                               int64_t memWidth,
                                               int64_t memHeight) {
    // Reads a sub region from the a set of dicom frames spread across file(s).
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
    if (dicomFileCount() <= 0) {
      return false;
    }
    // compute first and last frames to read.
    int64_t firstFrameX, firstFrameY, lastFrameX, lastFrameY;
    xyFrameSpan(layerX, layerY, memWidth, memHeight, &firstFrameX,
                &firstFrameY, &lastFrameX, &lastFrameY);

    int64_t frameYCOffset = firstFrameY * framesPerRow_;
    // increment over frame rows
    for (int64_t frameYC = firstFrameY; frameYC <= lastFrameY; ++frameYC) {
      // iterate over frame columns.
      for (int64_t frameXC = firstFrameX; frameXC <= lastFrameX; ++frameXC) {
        if ((frameXC < framesPerRow_) && (frameYC < framesPerColumn_)) {
          Frame* fptr = framePtr(frameXC + frameYCOffset);
          if (fptr != nullptr) {
            fptr->incReadCounter();
          }
        }
      }
      // increment row offset into frame buffer memory.
      frameYCOffset += framesPerRow_;
    }
    return true;
  }

  bool DICOMFileFrameRegionReader::readRegion(int64_t layerX,
                                              int64_t layerY,
                                              int64_t memWidth,
                                              int64_t memHeight,
                                              uint32_t *memory) {
    // Reads a sub region from the a set of dicom frames spread across file(s).
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
    if (dicomFileCount() <= 0) {
      return false;
    }
    const uint64_t frameMemSizeBytes = frameWidth_ * frameHeight_ *
                                                       sizeof(uint32_t);
    std::unique_ptr<uint32_t[]> frameMem = std::make_unique<uint32_t[]>(
                                                    frameWidth_ * frameHeight_);
    // compute first and last frames to read.
    int64_t firstFrameX, firstFrameY, lastFrameX, lastFrameY;
    xyFrameSpan(layerX, layerY, memWidth, memHeight, &firstFrameX,
                &firstFrameY, &lastFrameX, &lastFrameY);

    /*BOOST_LOG_TRIVIAL(debug) << "DICOMFileFrameRegionReader::readRegion" <<
                                "\n" << "layerX, layerY: " << layerX <<
                                ", " << layerY << "\n" <<
                                "memWidth, height: " << memWidth <<
                                ", " << memHeight << "\n" <<
                                "frameWidth, frameHeight: " << frameWidth_ <<
                                ", " << frameHeight_ << "\n" <<
                                "imageWidth, imageHeight: " << imageWidth_ <<
                                ", " << imageHeight_ << "\n" <<
                                "framesPerRow: " << framesPerRow_ << "\n" <<
                                "firstFrameX, firstFrameY: " <<
                                firstFrameX << ", " << firstFrameY  <<
                                "\n" << "lastFrameX, lastFrameY: " <<
                                lastFrameX << ", " << lastFrameY;*/

    // read offset in first frame.
    const int64_t frameStartXInit = layerX % frameWidth_;
    int64_t frameStartY = layerY % frameHeight_;
    int64_t frameYCOffset = firstFrameY * framesPerRow_;

    // write position in memory buffer
    int64_t myStart = 0;
    // increment over frame rows
    for (int64_t frameYC = firstFrameY; frameYC <= lastFrameY; ++frameYC) {
      // init starting position to read from in frame and write to in mem.
      int64_t frameStartX = frameStartXInit;
      int64_t mxStart = 0;

      // height to copy from frame to mem buffer.  clip to data in frame
      // or remaining in memory buffer.  Which ever is smaller.
      const int64_t heightCopied =
        std::min<int64_t>(frameHeight_ - frameStartY, memHeight - myStart);

      // iterate over frame columns.
      for (int64_t frameXC = firstFrameX; frameXC <= lastFrameX; ++frameXC) {
        // Get Frame memory
        uint32_t *rawFrameBytes = nullptr;
        if ((frameXC < framesPerRow_) && (frameYC < framesPerColumn_)) {
          if (frameBytes(frameXC + frameYCOffset, frameMem.get(),
                         frameMemSizeBytes)) {
            rawFrameBytes = frameMem.get();
          } else {
            // if unable to read region. e.g., jpeg decode failed.
            return false;
          }
        }
        // width to copy from frame to mem buffer.  clip to data in frame
        // or remaining in memory buffer.  Which ever is smaller.
        const int64_t widthCopeid =
          std::min<int64_t>(frameWidth_ - frameStartX, memWidth - mxStart);

        // copy frame memory to buffer mem.
        copyRegionFromFrames(layerX, layerY, rawFrameBytes,
                             frameStartX, frameStartY, widthCopeid,
                             heightCopied, memory, memWidth, mxStart,
                             myStart);

        // increment write cusor X position
        mxStart += widthCopeid;

        // set next frame to start reading at the begining frame column
        frameStartX = 0;
      }
      // increment write cusor Y position
      myStart += heightCopied;
      // set next frame to start reading at the begining frame row
      frameStartY = 0;
      // increment row offset into frame buffer memory.
      frameYCOffset += framesPerRow_;
    }
    return true;
  }
}  // namespace wsiToDicomConverter
