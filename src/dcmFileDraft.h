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

#ifndef SRC_DCMFILEDRAFT_H_
#define SRC_DCMFILEDRAFT_H_

#include <dcmtk/dcmdata/dcdatset.h>
#include <dcmtk/dcmdata/dcofsetl.h>
#include <dcmtk/dcmdata/dcpixel.h>
#include <dcmtk/dcmdata/libi2d/i2dimgs.h>
#include <dcmtk/ofstd/ofcond.h>
#include <memory>
#include <string>
#include <vector>
#include "src/dcmTags.h"
#include "src/dcmtkImgDataInfo.h"
#include "src/enums.h"
#include "src/frame.h"

namespace wsiToDicomConverter {

// Represents single DICOM file with metadata
class DcmFileDraft {
 public:
  // DcmTags* additionalTags - is read-only input, but can't be marked const
  // since contains and using dcmtk objects
  // std::vector<std::unique_ptr<Frame> > framesData - requre transfer of
  // ownership to perform efficent memory managment and thread-save execution
  // imageHeight, imageWidth - size of complete image
  // rowSize - how many frames in one row of this image
  // studyId, seriesId, imageName - required DICOM metadata
  // pixelData - pixel data, requre transfer of ownership
  // imgInfo - image metadata
  // numberOfFrames - how many frames in this file
  // row, column - position to start
  // level - which level represent this file
  // batchNumber - index number of this file for one level
  // offset - how many frames where already generated
  //          for this level
  // totalNumberOfFrames - how many frames in this level
  // tiled - frame organizationtype
  //           true:TILED_FULL
  //           false:TILED_SPARSE
  // additionalTags - additional DICOM metadata
  // firstLevelWidthMm, firstLevelHeightMm - physical size
  // of first level

  DcmFileDraft(std::vector<std::unique_ptr<Frame> > framesData,
               const std::string& outputFileMask, int64_t imageWidth,
               int64_t imageHeight, int64_t level, int64_t row, int64_t column,
               const std::string& studyId, const std::string& seriesId,
               const std::string& imageName, DCM_Compression compression,
               bool tiled, DcmTags* additionalTags, double firstLevelWidthMm,
               double firstLevelHeightMmm, int64_t downsample,
              const std::vector<std::unique_ptr<DcmFileDraft>>
              *prior_frame_batches);

  ~DcmFileDraft();

  void saveFile();
  void write(DcmOutputStream* outStream);

  int64_t get_frame_width() const;
  int64_t get_frame_height() const;
  int64_t get_image_width() const;
  int64_t get_image_height() const;
  int64_t get_file_frame_count() const;
  int64_t get_downsample() const;
  const Frame* const get_frame(int64_t idx) const;

 private:
  std::vector<std::unique_ptr<Frame> > framesData_;
  std::string outputFileMask_;
  std::string studyId_;
  std::string seriesId_;
  std::string imageName_;
  DcmTags* additionalTags_;
  DCM_Compression compression_;
  int64_t prior_batch_frames_;
  int64_t frame_count_;
  int64_t imageWidth_;
  int64_t imageHeight_;
  int64_t level_;
  int64_t batchNumber_;
  int64_t row_;
  int64_t column_;
  int64_t frameWidth_;
  int64_t frameHeight_;
  double firstLevelWidthMm_;
  double firstLevelHeightMm_;
  int64_t downsample_;
  bool tiled_;
};
}  // namespace wsiToDicomConverter
#endif  // SRC_DCMFILEDRAFT_H_
