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

#ifndef DCMFILEDRAFT_H
#define DCMFILEDRAFT_H

#include <dcmtk/dcmdata/dcdatset.h>
#include <dcmtk/dcmdata/dcofsetl.h>
#include <dcmtk/dcmdata/dcpixel.h>
#include <dcmtk/dcmdata/libi2d/i2dimgs.h>
#include <dcmtk/ofstd/ofcond.h>
#include <string>
#include <vector>
#include "dcmTags.h"
#include "dcmtkImgDataInfo.h"
#include "enums.h"
#include "frame.h"

class dcmFileDraft {
 private:
  std::vector<Frame*> framesData_;
  std::string outputFileMask_;
  std::string studyId_;
  std::string seriesId_;
  std::string imageName_;
  DcmTags* additionalTags_;
  DCM_Compression compression_;
  uint32_t numberOfFrames_;
  int64_t imageWidth_;
  int64_t imageHeight_;
  int32_t level_;
  int32_t batchNumber_;
  uint32_t batchSize_;
  uint32_t row_;
  uint32_t column_;
  int64_t frameWidth_;
  int64_t frameHeight_;
  double firstLevelWidthMm_;
  double firstLevelHeightMm_;
  double downsample_;
  bool tiled_;

 public:
  dcmFileDraft(std::vector<Frame*> framesData, std::string outputFileMask,
               uint32_t numberOfFrames, int64_t imageWidth, int64_t imageHeight,
               int32_t level, int32_t batchNumber, uint32_t batch, uint32_t row,
               uint32_t column, int64_t frameWidth, int64_t frameHeight,
               std::string studyId, std::string seriesId, std::string imageName,
               DCM_Compression compression, bool tiled, DcmTags* additionalTags,
               double firstLevelWidthMm, double firstLevelHeightMm);

  static OFCondition startConversion(
      OFString outputFileName, int64_t imageHeight, int64_t imageWidth,
      uint32_t rowSize, std::string studyId, std::string seriesId,
      std::string imageName, DcmPixelData* pixelData, DcmtkImgDataInfo& imgInfo,
      uint32_t numberOfFrames, uint32_t row, uint32_t column, int level,
      int batchNumber, unsigned int offset, uint32_t totalNumberOfFrames,
      bool tiled, DcmTags* additionalTags, double firstLevelWidthMm,
      double firstLevelHeightMm);

  ~dcmFileDraft();

  void saveFile();
};

#endif  // DCMFILEDRAFT_H
