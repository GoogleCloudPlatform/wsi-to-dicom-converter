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

#include <absl/strings/string_view.h>
#include <dcmtk/dcmdata/dcdatset.h>
#include <dcmtk/dcmdata/dcofsetl.h>
#include <dcmtk/dcmdata/dcpixel.h>
#include <dcmtk/dcmdata/libi2d/i2dimgs.h>
#include <dcmtk/ofstd/ofcond.h>
#include <memory>
#include <string>
#include <vector>
#include "src/abstractDcmFile.h"
#include "src/dcmTags.h"
#include "src/dcmtkImgDataInfo.h"
#include "src/enums.h"
#include "src/frame.h"

namespace wsiToDicomConverter {

// Represents single DICOM file with metadata
class DcmFileDraft : public AbstractDcmFile {
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
               absl::string_view outputFileMask, int64_t imageWidth,
               int64_t imageHeight, int64_t instanceNumber, int64_t row,
               int64_t column,
               absl::string_view studyId, absl::string_view seriesId,
               absl::string_view imageName, DCM_Compression compression,
               bool tiled, DcmTags* additionalTags, double firstLevelWidthMm,
               double firstLevelHeightMmm, int64_t downsample,
              const std::vector<std::unique_ptr<AbstractDcmFile>>
              *prior_frame_batches, absl::string_view sourceImageDescription,
              bool saveDicomInstanceToDisk);

  virtual ~DcmFileDraft();
  virtual void saveFile();
  virtual void write(DcmOutputStream* outStream);
  virtual int64_t frameWidth() const;
  virtual int64_t frameHeight() const;
  virtual int64_t imageWidth() const;
  virtual int64_t imageHeight() const;
  virtual int64_t fileFrameCount() const;
  virtual int64_t downsample() const;
  virtual double imageHeightMM() const;
  virtual double imageWidthMM() const;
  virtual Frame* frame(int64_t idx) const;

 private:
  std::vector<std::unique_ptr<Frame> > framesData_;
  std::string outputFileMask_;
  std::string studyId_;
  std::string seriesId_;
  std::string imageName_;
  std::string sourceImageDescription_;
  DcmTags* additionalTags_;
  DCM_Compression compression_;
  int64_t prior_batch_frames_;
  int64_t imageWidth_;
  int64_t imageHeight_;
  int64_t instanceNumber_;
  int64_t batchNumber_;
  int64_t row_;
  int64_t column_;
  int64_t frameWidth_;
  int64_t frameHeight_;
  double firstLevelWidthMm_;
  double firstLevelHeightMm_;
  int64_t downsample_;
  bool tiled_;
  bool saveDicomInstanceToDisk_;
};
}  // namespace wsiToDicomConverter
#endif  // SRC_DCMFILEDRAFT_H_
