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
    clear_dicom_files();
}

DICOMFileFrameRegionReader::~DICOMFileFrameRegionReader() {
  clear_dicom_files();
}

DcmFileDraft * DICOMFileFrameRegionReader::get_dicom_file(size_t index) {
  return dcmFiles_.at(index).get();
}

void DICOMFileFrameRegionReader::set_dicom_files(
                      std::vector<std::unique_ptr<DcmFileDraft>> dcm_files) {
  // all files should have frames with same dimensions
  dcmFiles_ = std::move(dcm_files);
  if (dcmFiles_.size() <= 0) {
    clear_dicom_files();
    return;
  }
  const DcmFileDraft & first_dcm_file = (*dcmFiles_.at(0));
  if (!first_dcm_file.get_frame(0)->has_compressed_raw_bytes()) {
    clear_dicom_files();
    return;
  }
  frameWidth_ = first_dcm_file.get_frame_width();
  frameHeight_ = first_dcm_file.get_frame_height();
  imageWidth_ = first_dcm_file.get_image_width();
  imageHeight_ = first_dcm_file.get_image_height();
  // compute frames per row and column.  Not final frames may
  // not be used completely.
  framesPerRow_ = static_cast<int64_t>(
                                  std::ceil(static_cast<double>(imageWidth_) /
                                  static_cast<double>(frameWidth_)));
  framesPerColumn_ = static_cast<int64_t>(
                                  std::ceil(static_cast<double>(imageHeight_) /
                                  static_cast<double>(frameHeight_)));
}

void DICOMFileFrameRegionReader::clear_dicom_files() {
  dcmFiles_.clear();
  frameWidth_ = 0;
  frameHeight_ = 0;
  imageWidth_ = 0;
  imageHeight_ = 0;
  framesPerRow_ = 0;
  framesPerColumn_ = 0;
}

int64_t DICOMFileFrameRegionReader::dicom_file_count() const {
  return dcmFiles_.size();
}

bool DICOMFileFrameRegionReader::get_frame_bytes(int64_t index,
                                                 uint32_t* frame_memory,
                                const int64_t frame_buffer_size_bytes) const {
    // Reads a frame from as set of loaded dicom files.
    //
    // Args:
    //  index : index of frame to read.
    //  frame_memory : memory buffer to read frame into
    //  frame_buffer_size_bytes : size of buffer in bytes
    //
    // Returns:
    //   true if frame memory initalized
    const int64_t file_count = dcmFiles_.size();
    for (int64_t file_idx = 0; file_idx < file_count; ++file_idx) {
      const DcmFileDraft & dcm_file = (*dcmFiles_.at(file_idx));
      const int64_t frame_count = dcm_file.get_file_frame_count();
      if (index >= frame_count) {
        index -= frame_count;
      } else {
        return dcm_file.get_frame(index)->get_raw_frame_bytes(
                reinterpret_cast<uint8_t *>(frame_memory),
                                            frame_buffer_size_bytes) ==
                                            frame_buffer_size_bytes;
      }
    }
    return false;
  }

  void DICOMFileFrameRegionReader::copy_region_from_frames(
                    int64_t image_offset_X, int64_t image_offset_Y,
                    const uint32_t * const frame_bytes, int64_t fx, int64_t fy,
                    int64_t copy_width, int64_t copy_height, uint32_t *memory,
                    int64_t memory_width, int64_t mx, int64_t my) const {
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

      const int64_t end_mx = mx + copy_width;  // end position to copy to
      int64_t my_offset = my * memory_width;   // y offset to start writing to
      int64_t frame_offset = fy * frameWidth_;  // y offset to read from.
      int64_t imageY = image_offset_Y;         // global position in image
      for (int64_t row = 0; row < copy_height; ++row) {
        int64_t frame_cursor = fx + frame_offset;  // frame memory read index
        for (int64_t column = mx; column < end_mx; ++column) {
            int64_t imageX = column + image_offset_X;
            if (imageX >= imageWidth_ ||   // if frame memory is null or
                imageY >= imageHeight_ ||  // outside of image bounds
                frame_bytes == NULL) {    // set copied pixel memory ARGB to 0
              memory[column + my_offset] = 0;
            } else {   // otherwise copy memory
              memory[column + my_offset] = frame_bytes[frame_cursor];
            }
            frame_cursor += 1;  // increment frame read index
        }
        my_offset += memory_width;  // increment row offsets
        frame_offset += frameWidth_;
        imageY  += 1;
      }
  }

  bool DICOMFileFrameRegionReader::read_region(int64_t layer_x,
                                               int64_t layer_y,
                                               int64_t mem_width,
                                               int64_t mem_height,
                                               uint32_t *memory) const {
    // Reads a sub region from the a set of dicom frames spread across file(s).
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
    if (dicom_file_count() <= 0) {
      return false;
    }
    const uint64_t frame_mem_size_bytes = frameWidth_ * frameHeight_ *
                                                       sizeof(uint32_t);
    std::unique_ptr<uint32_t[]> frame_mem = std::make_unique<uint32_t[]>(
                                                    frameWidth_ * frameHeight_);
    // compute first and last frames to read.
    const int64_t first_frame_y = (layer_y / frameHeight_);
    const int64_t first_frame_x = (layer_x / frameWidth_);
    const int64_t last_frame_y = static_cast<int64_t>(
                      std::ceil(static_cast<double>(layer_y + mem_height - 1) /
                                static_cast<double>(frameHeight_)));
    const int64_t last_frame_x = static_cast<int64_t>(
                      std::ceil(static_cast<double>(layer_x + mem_width - 1) /
                                static_cast<double>(frameWidth_)));

    BOOST_LOG_TRIVIAL(debug) << "DICOMFileFrameRegionReader::read_region" <<
                                "\n" << "layer_x, layer_y: " << layer_x <<
                                ", " << layer_y << "\n" <<
                                "mem_width, height: " << mem_width <<
                                ", " << mem_height << "\n" <<
                                "frameWidth, frameHeight: " << frameWidth_ <<
                                ", " << frameHeight_ << "\n" <<
                                "imageWidth, imageHeight: " << imageWidth_ <<
                                ", " << imageHeight_ << "\n" <<
                                "framesPerRow: " << framesPerRow_ << "\n" <<
                                "first_frame_x, first_frame_y: " <<
                                first_frame_x << ", " << first_frame_y  <<
                                "\n" << "last_frame_x, last_frame_y: " <<
                                last_frame_x << ", " << last_frame_y;

    // read offset in first frame.
    const int64_t frame_start_x_init = layer_x % frameWidth_;
    int64_t frame_start_y = layer_y % frameHeight_;
    int64_t frame_yc_offset = first_frame_y * framesPerRow_;

    // write position in memory buffer
    int64_t my_start = 0;
    // increment over frame rows
    for (int64_t frame_yc = first_frame_y; frame_yc <= last_frame_y;
                                                                  ++frame_yc) {
      // init starting position to read from in frame and write to in mem.
      int64_t frame_start_x = frame_start_x_init;
      int64_t mx_start = 0;

      // height to copy from frame to mem buffer.  clip to data in frame
      // or remaining in memory buffer.  Which ever is smaller.
      const int64_t height_copied = std::min(frameHeight_ - frame_start_y,
                                            mem_height - my_start);

      // iterate over frame columns.
      for (int64_t frame_xc = first_frame_x; frame_xc <= last_frame_x;
                                                                  ++frame_xc) {
        // Get Frame memory
        uint32_t *raw_frame_bytes = NULL;
        if ((frame_xc <= framesPerRow_) && (frame_yc <= framesPerColumn_)) {
          if (get_frame_bytes(frame_xc + frame_yc_offset, frame_mem.get(),
                              frame_mem_size_bytes)) {
            raw_frame_bytes = frame_mem.get();
          }
        }
        // width to copy from frame to mem buffer.  clip to data in frame
        // or remaining in memory buffer.  Which ever is smaller.
        const int64_t width_copeid = std::min(frameWidth_ - frame_start_x,
                                             mem_width - mx_start);

        // copy frame memory to buffer mem.
        copy_region_from_frames(layer_x, layer_y, raw_frame_bytes,
                                frame_start_x, frame_start_y, width_copeid,
                                height_copied, memory, mem_width, mx_start,
                                my_start);

        // increment write cusor X position
        mx_start += width_copeid;

        // set next frame to start reading at the begining frame column
        frame_start_x = 0;
      }
      // increment write cusor Y position
      my_start += height_copied;
      // set next frame to start reading at the begining frame row
      frame_start_y = 0;
      // increment row offset into frame buffer memory.
      frame_yc_offset += framesPerRow_;
    }
    return true;
  }
}  // namespace wsiToDicomConverter
