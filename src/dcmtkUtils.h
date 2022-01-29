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

#ifndef SRC_DCMTKUTILS_H_
#define SRC_DCMTKUTILS_H_
#include <absl/strings/string_view.h>
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

class DcmtkUtils {
 public:
  // Generates DICOM file based on outputFileName
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
  static OFCondition startConversion(
      int64_t imageHeight, int64_t imageWidth, uint32_t rowSize,
      absl::string_view studyId, absl::string_view seriesId,
      absl::string_view imageName, std::unique_ptr<DcmPixelData> pixelData,
      const DcmtkImgDataInfo& imgInfo, uint32_t numberOfFrames, uint32_t row,
      uint32_t column, int level, int batchNumber, unsigned int offset,
      uint32_t totalNumberOfFrames, bool tiled, DcmTags* additionalTags,
      double firstLevelWidthMm, double firstLevelHeightMm,
      DcmOutputStream* outStream);

  // Wrapper for startConversion without additional parameters.
  static OFCondition startConversion(
      int64_t imageHeight, int64_t imageWidth, uint32_t rowSize,
      absl::string_view studyId, absl::string_view seriesId,
      absl::string_view imageName, std::unique_ptr<DcmPixelData> pixelData,
      const DcmtkImgDataInfo& imgInfo, uint32_t numberOfFrames, uint32_t row,
      uint32_t column, int level, int batchNumber, uint32_t offset,
      uint32_t totalNumberOfFrames, bool tiled, DcmOutputStream* outStream);

  // Generates DICOM file object.
  static OFCondition populateDataSet(
      const int64_t imageHeight, const int64_t imageWidth,
      const uint32_t rowSize, absl::string_view studyId,
      absl::string_view seriesId, absl::string_view imageName,
      std::unique_ptr<DcmPixelData> pixelData, const DcmtkImgDataInfo& imgInfo,
      const uint32_t numberOfFrames, const uint32_t row, const uint32_t column,
      const int level, const int batchNumber, const uint32_t offset,
      const uint32_t totalNumberOfFrames, const bool tiled,
      DcmTags* additionalTags, const double firstLevelWidthMm,
      const double firstLevelHeightMm, DcmDataset* dataSet);

  // Inserts current date/time into DCM_ContentDate/Time.
  static OFCondition generateDateTags(DcmDataset* dataSet);

  // Inserts which is same for pathology DICOMs.
  static OFCondition insertStaticTags(DcmDataset* dataSet, int level);
  // Inserts studyId and seriesId by params and generated instanceId.
  static OFCondition insertIds(absl::string_view studyId,
                               absl::string_view seriesId,
                               DcmDataset* dataSet);
  // Inserts tags related to base image.
  static OFCondition insertBaseImageTags(absl::string_view imageName,
                                         const int64_t imageHeight,
                                         const int64_t imageWidth,
                                         const double firstLevelWidthMm,
                                         const double firstLevelHeightMm,
                                         DcmDataset* dataSet);
  // Inserts tags which is required for multi frame DICOM.
  static OFCondition insertMultiFrameTags(
      const DcmtkImgDataInfo& imgInfo, const uint32_t numberOfFrames,
      const uint32_t rowSize, const uint32_t row, const uint32_t column,
      const int level, const int batchNumber, const uint32_t offset,
      const uint32_t totalNumberOfFrames, const bool tiled,
      absl::string_view seriesId, DcmDataset* dataSet);
};
}  // namespace wsiToDicomConverter
#endif  // SRC_DCMTKUTILS_H_
