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

#include "src/dcmtkUtils.h"
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcdict.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcostrmf.h>
#include <dcmtk/dcmdata/dcpixseq.h>
#include <dcmtk/dcmdata/dcpxitem.h>
#include <dcmtk/dcmdata/dcuid.h>
#include <dcmtk/dcmdata/dcvrat.h>
#include <dcmtk/dcmdata/dcwcache.h>
#include <dcmtk/dcmdata/libi2d/i2d.h>
#include <dcmtk/dcmdata/libi2d/i2doutpl.h>
#include <dcmtk/dcmdata/libi2d/i2dplsc.h>
#include <boost/chrono.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/log/trivial.hpp>
#include <boost/thread/thread.hpp>
#include <ctime>
#include <iomanip>
#include <string>
#include <utility>
#include <vector>

namespace wsiToDicomConverter {

inline OFCondition generateFramePositionMetadata(DcmDataset* resultObject,
                                                 uint32_t numberOfFrames,
                                                 uint32_t rowSize, uint32_t row,
                                                 uint32_t column, uint32_t x,
                                                 uint32_t y) {
  std::unique_ptr<DcmSequenceOfItems> PerFrameFunctionalGroupsSequence =
      std::make_unique<DcmSequenceOfItems>(
          DCM_PerFrameFunctionalGroupsSequence);
  for (uint32_t frameNumber = 0; frameNumber < numberOfFrames; frameNumber++) {
    if (column > rowSize) {
      column = 1;
      row++;
    }
    std::string index = std::to_string(column) + "\\" + std::to_string(row);
    std::unique_ptr<DcmItem> dimension = std::make_unique<DcmItem>();
    dimension->putAndInsertString(DCM_DimensionIndexValues, index.c_str());

    std::unique_ptr<DcmSequenceOfItems> sequenceDimension =
        std::make_unique<DcmSequenceOfItems>(DCM_FrameContentSequence);
    sequenceDimension->insert(dimension.release());

    std::unique_ptr<DcmItem> pixelPosition = std::make_unique<DcmItem>();
    pixelPosition->putAndInsertSint32(DCM_ColumnPositionInTotalImagePixelMatrix,
                                      (column - 1) * x + 1);
    pixelPosition->putAndInsertSint32(DCM_RowPositionInTotalImagePixelMatrix,
                                      (row - 1) * y + 1);

    std::unique_ptr<DcmSequenceOfItems> sequencePosition =
        std::make_unique<DcmSequenceOfItems>(DCM_PlanePositionSlideSequence);
    sequencePosition->insert(pixelPosition.release());

    std::unique_ptr<DcmItem> positionItem = std::make_unique<DcmItem>();
    positionItem->insert(sequenceDimension.release());
    positionItem->insert(sequencePosition.release());
    PerFrameFunctionalGroupsSequence->insert(positionItem.release());
    column++;
  }
  return resultObject->insert(PerFrameFunctionalGroupsSequence.release());
}

inline OFCondition generateSharedFunctionalGroupsSequence(
    DcmDataset* resultObject, double pixelSizeMm) {
  if (pixelSizeMm <= 0) {
    pixelSizeMm = 0.1;
  }
  std::unique_ptr<DcmItem> sharedFunctionalGroupsSequence =
      std::make_unique<DcmItem>();
  std::unique_ptr<DcmItem> pixelMeasuresSequence = std::make_unique<DcmItem>();
  std::string pixelSizeMmStr = std::to_string(pixelSizeMm);
  pixelSizeMmStr = pixelSizeMmStr + "\\" + pixelSizeMmStr;
  pixelMeasuresSequence->putAndInsertString(DCM_PixelSpacing,
                                            pixelSizeMmStr.c_str());
  sharedFunctionalGroupsSequence->insertSequenceItem(
      DCM_PixelMeasuresSequence, pixelMeasuresSequence.release());
  return resultObject->insertSequenceItem(
      DCM_SharedFunctionalGroupsSequence,
      sharedFunctionalGroupsSequence.release());
}

inline std::unique_ptr<DcmItem> pointerItem(char* dimensionOrganizationUIDstr) {
  std::unique_ptr<DcmItem> pointerItem = std::make_unique<DcmItem>();
  pointerItem->putAndInsertOFStringArray(DCM_DimensionOrganizationUID,
                                         dimensionOrganizationUIDstr);
  std::unique_ptr<DcmAttributeTag> slideSequence =
      std::make_unique<DcmAttributeTag>(DCM_FunctionalGroupPointer);
  slideSequence->putTagVal(DCM_PlanePositionSlideSequence);
  std::unique_ptr<DcmAttributeTag> columnPosition =
      std::make_unique<DcmAttributeTag>(DCM_DimensionIndexPointer);
  columnPosition->putTagVal(DCM_ColumnPositionInTotalImagePixelMatrix);
  pointerItem->insert(slideSequence.release());
  pointerItem->insert(columnPosition.release());
  return pointerItem;
}

inline OFCondition generateDimensionIndexSequence(DcmDataset* resultObject) {
  std::unique_ptr<DcmItem> dimensionOrganizationUID =
      std::make_unique<DcmItem>();
  char dimensionOrganizationUIDstr[100];
  dimensionOrganizationUID->putAndInsertOFStringArray(
      DCM_DimensionOrganizationUID,
      dcmGenerateUniqueIdentifier(dimensionOrganizationUIDstr,
                                  SITE_STUDY_UID_ROOT));
  std::unique_ptr<DcmSequenceOfItems> dimensionOrganizationSequence =
      std::make_unique<DcmSequenceOfItems>(DCM_DimensionOrganizationSequence);
  dimensionOrganizationSequence->insert(dimensionOrganizationUID.release());
  resultObject->insert(dimensionOrganizationSequence.release());

  std::unique_ptr<DcmSequenceOfItems> sequence =
      std::make_unique<DcmSequenceOfItems>(DCM_DimensionIndexSequence);
  sequence->insert(pointerItem(dimensionOrganizationUIDstr).release());
  sequence->insert(pointerItem(dimensionOrganizationUIDstr).release());
  return resultObject->insert(sequence.release());
}

std::string formatTime(std::string format) {
  std::stringstream stringStream;
  struct tm utcTime;
  time_t timeNow = time(nullptr);
  gmtime_r(&timeNow, &utcTime);
  stringStream << std::put_time(&utcTime, format.c_str());
  return stringStream.str();
}

std::string currentDate() { return formatTime("%Y%m%d"); }
std::string currentTime() { return formatTime("%OH%OM%OS"); }

OFCondition insertPixelMetadata(DcmDataset* dataset,
                                const DcmtkImgDataInfo& imgInfo,
                                uint32_t numberOfFrames) {
  OFCondition cond =
      dataset->putAndInsertUint16(DCM_SamplesPerPixel, imgInfo.samplesPerPixel);
  if (cond.bad()) return cond;

  cond = dataset->putAndInsertOFStringArray(DCM_PhotometricInterpretation,
                                            imgInfo.photoMetrInt);
  if (cond.bad()) return cond;

  cond = dataset->putAndInsertUint16(DCM_PlanarConfiguration, imgInfo.planConf);
  if (cond.bad()) return cond;

  cond = dataset->putAndInsertUint16(DCM_Rows, imgInfo.rows);
  if (cond.bad()) return cond;

  cond = dataset->putAndInsertUint16(DCM_Columns, imgInfo.cols);
  if (cond.bad()) return cond;

  cond = dataset->putAndInsertUint16(DCM_BitsAllocated, imgInfo.bitsAlloc);
  if (cond.bad()) return cond;

  cond = dataset->putAndInsertUint16(DCM_BitsStored, imgInfo.bitsStored);
  if (cond.bad()) return cond;

  cond = dataset->putAndInsertUint16(DCM_HighBit, imgInfo.highBit);
  if (cond.bad()) return cond;

  if (numberOfFrames >= 1) {
    char buf[10];
    int err = snprintf(buf, sizeof(buf), "%u", numberOfFrames);
    if (err == -1) return EC_IllegalCall;
    cond = dataset->putAndInsertOFStringArray(DCM_NumberOfFrames, buf);
    if (cond.bad()) return cond;

    cond = dataset->putAndInsertString(DCM_FrameIncrementPointer, "");
    if (cond.bad()) return cond;
  }

  return dataset->putAndInsertUint16(DCM_PixelRepresentation,
                                     imgInfo.pixelRepr);
}

OFCondition generateDcmDataset(I2DOutputPlug* outPlug, DcmDataset* resultDset,
                               const DcmtkImgDataInfo& imgInfo,
                               uint32_t numberOfFrames) {
  if (!outPlug) return EC_IllegalParameter;

  OFCondition cond = insertPixelMetadata(resultDset, imgInfo, numberOfFrames);

  if (cond.bad()) {
    return cond;
  }

  std::string lossy = "00";
  if (imgInfo.transSyn == EXS_JPEGProcess1) {
    lossy = "01";
  }
  cond = resultDset->putAndInsertOFStringArray(DCM_LossyImageCompression,
                                               lossy.c_str(), true);

  if (cond.bad()) return cond;

  cond = outPlug->convert(*resultDset);
  if (cond.bad()) {
    return cond;
  }

  if (cond.bad()) {
    return cond;
  }
  return EC_Normal;
}

OFCondition DcmtkUtils::populateDataSet(
    const int64_t imageHeight, const int64_t imageWidth, const uint32_t rowSize,
    const std::string& studyId, const std::string& seriesId,
    const std::string& imageName, std::unique_ptr<DcmPixelData> pixelData,
    const DcmtkImgDataInfo& imgInfo, const uint32_t numberOfFrames,
    const uint32_t row, const uint32_t column, const int level,
    const int batchNumber, const uint32_t offset,
    const uint32_t totalNumberOfFrames, const bool tiled,
    DcmTags* additionalTags, const double firstLevelWidthMm,
    const double firstLevelHeightMm, DcmDataset* dataSet) {
  std::unique_ptr<I2DOutputPlug> outPlug;

  OFString pixDataFile, outputFile;

  outPlug = std::make_unique<I2DOutputPlugSC>();
  if (outPlug.get() == nullptr) return EC_MemoryExhausted;

  OFBool insertType2 = OFTrue;
  OFBool inventType1 = OFTrue;
  OFBool doChecks = OFTrue;

  outPlug->setValidityChecking(doChecks, insertType2, inventType1);

  if (!dcmDataDict.isDictionaryLoaded()) {
    dcmDataDict.wrlock().reloadDictionaries(true, false);
    dcmDataDict.unlock();
  }

  OFCondition cond =
      generateDcmDataset(outPlug.get(), dataSet, imgInfo, numberOfFrames);

  if (cond.bad()) return cond;

  cond = dataSet->insert(pixelData.release());
  if (cond.bad()) return cond;

  cond = generateDateTags(dataSet);

  if (cond.bad()) return cond;

  cond = insertIds(studyId, seriesId, dataSet);

  if (cond.bad()) return cond;

  cond = insertBaseImageTags(imageName, imageHeight, imageWidth,
                             firstLevelWidthMm, firstLevelHeightMm, dataSet);

  if (cond.bad()) return cond;

  insertMultiFrameTags(imgInfo, numberOfFrames, rowSize, row, column, level,
                       batchNumber, offset, totalNumberOfFrames, tiled,
                       seriesId, dataSet);
  if (cond.bad()) return cond;

  cond = insertStaticTags(dataSet, level);

  if (cond.bad()) return cond;

  cond = generateSharedFunctionalGroupsSequence(
      dataSet, firstLevelHeightMm / imageHeight);
  if (cond.bad()) return cond;

  cond = generateDimensionIndexSequence(dataSet);

  if (cond.bad()) return cond;

  if (additionalTags != nullptr) {
    additionalTags->populateDataset(dataSet);
  }

  return cond;
}

OFCondition DcmtkUtils::generateDateTags(DcmDataset* dataSet) {
  OFCondition cond = dataSet->putAndInsertOFStringArray(DCM_ContentDate,
                                                        currentDate().c_str());
  if (cond.good())
    cond = dataSet->putAndInsertOFStringArray(DCM_ContentTime,
                                              currentTime().c_str());
  return cond;
}

OFCondition DcmtkUtils::insertStaticTags(DcmDataset* dataSet, int level) {
  OFCondition cond = dataSet->putAndInsertOFStringArray(
      DCM_SOPClassUID, UID_VLWholeSlideMicroscopyImageStorage);
  if (cond.bad()) return cond;
  cond = dataSet->putAndInsertOFStringArray(DCM_Modality, "SM");
  if (cond.bad()) return cond;
  if (level == 0) {
    cond = dataSet->putAndInsertOFStringArray(DCM_ImageType,
                                          "DERIVED\\PRIMARY\\VOLUME\\NONE");
  } else {
    cond = dataSet->putAndInsertOFStringArray(DCM_ImageType,
                                     "DERIVED\\PRIMARY\\VOLUME\\RESAMPLED");
  }
  if (cond.bad()) return cond;
  cond = dataSet->putAndInsertOFStringArray(DCM_ImageOrientationSlide,
                                            "0\\-1\\0\\-1\\0\\0");
  if (cond.bad()) return cond;
  dataSet->putAndInsertUint16(DCM_RepresentativeFrameNumber, 1);

  return cond;
}

OFCondition DcmtkUtils::insertIds(const std::string& studyId,
                                  const std::string& seriesId,
                                  DcmDataset* dataSet) {
  char instanceUidGenerated[100];
  dcmGenerateUniqueIdentifier(instanceUidGenerated, SITE_INSTANCE_UID_ROOT);
  OFCondition cond = dataSet->putAndInsertOFStringArray(DCM_SOPInstanceUID,
                                                        instanceUidGenerated);
  if (cond.bad()) return cond;
  cond =
      dataSet->putAndInsertOFStringArray(DCM_StudyInstanceUID, studyId.c_str());
  if (cond.bad()) return cond;
  cond = dataSet->putAndInsertOFStringArray(DCM_SeriesInstanceUID,
                                            seriesId.c_str());
  return cond;
}

OFCondition DcmtkUtils::insertBaseImageTags(const std::string& imageName,
                                            const int64_t imageHeight,
                                            const int64_t imageWidth,
                                            const double firstLevelWidthMm,
                                            const double firstLevelHeightMm,
                                            DcmDataset* dataSet) {
  OFCondition cond = dataSet->putAndInsertOFStringArray(DCM_SeriesDescription,
                                                        imageName.c_str());
  if (cond.bad()) return cond;
  cond = dataSet->putAndInsertUint32(DCM_TotalPixelMatrixColumns, imageWidth);
  if (cond.bad()) return cond;
  cond = dataSet->putAndInsertUint32(DCM_TotalPixelMatrixRows, imageHeight);
  if (firstLevelWidthMm > 0 && firstLevelHeightMm > 0 && cond.good()) {
    cond =
        dataSet->putAndInsertFloat32(DCM_ImagedVolumeWidth, firstLevelWidthMm);
    if (cond.bad()) return cond;
    cond = dataSet->putAndInsertFloat32(DCM_ImagedVolumeHeight,
                                        firstLevelHeightMm);
  }
  return cond;
}

OFCondition DcmtkUtils::insertMultiFrameTags(
    const DcmtkImgDataInfo& imgInfo, const uint32_t numberOfFrames,
    const uint32_t rowSize, const uint32_t row, const uint32_t column,
    const int level, const int batchNumber, const uint32_t offset,
    const uint32_t totalNumberOfFrames, const bool tiled,
    const std::string& seriesId, DcmDataset* dataSet) {
  unsigned int concatenationTotalNumber;

  if (totalNumberOfFrames - offset == numberOfFrames) {
    concatenationTotalNumber = batchNumber + 1;
  } else {
    concatenationTotalNumber =
        std::ceil(static_cast<double>(totalNumberOfFrames) /
                  static_cast<double>(numberOfFrames));
  }

  OFCondition cond = dataSet->putAndInsertOFStringArray(
      DCM_InstanceNumber, std::to_string(level + 1).c_str());
  if (concatenationTotalNumber > 1 && cond.good()) {
    cond =
        dataSet->putAndInsertUint32(DCM_ConcatenationFrameOffsetNumber, offset);
    if (cond.bad()) return cond;
    cond =
        dataSet->putAndInsertUint16(DCM_InConcatenationNumber, batchNumber + 1);
    if (cond.bad()) return cond;
    cond = dataSet->putAndInsertUint16(DCM_InConcatenationTotalNumber,
                                       concatenationTotalNumber);
    if (cond.bad()) return cond;
    cond = dataSet->putAndInsertOFStringArray(
        DCM_ConcatenationUID,
        (seriesId + "." + std::to_string(level + 1)).c_str());
  }
  if (cond.bad()) return cond;
  cond = dataSet->putAndInsertOFStringArray(
      DCM_FrameOfReferenceUID,
      (seriesId + "." + std::to_string(level + 1)).c_str());
  if (cond.bad()) return cond;
  if (tiled) {
    cond = dataSet->putAndInsertOFStringArray(DCM_DimensionOrganizationType,
                                              "TILED_FULL");
  } else {
    cond = dataSet->putAndInsertOFStringArray(DCM_DimensionOrganizationType,
                                              "TILED_SPARSE");
    if (cond.bad()) return cond;
    cond = generateFramePositionMetadata(dataSet, numberOfFrames, rowSize, row,
                                         column, imgInfo.rows, imgInfo.cols);
  }
  return cond;
}

OFCondition DcmtkUtils::startConversion(
    int64_t imageHeight, int64_t imageWidth, uint32_t rowSize,
    const std::string& studyId, const std::string& seriesId,
    const std::string& imageName, std::unique_ptr<DcmPixelData> pixelData,
    const DcmtkImgDataInfo& imgInfo, uint32_t numberOfFrames, uint32_t row,
    uint32_t column, int level, int batchNumber, uint32_t offset,
    uint32_t totalNumberOfFrames, bool tiled, DcmOutputStream* outStream) {
  return startConversion(imageHeight, imageWidth, rowSize, studyId, seriesId,
                         imageName, std::move(pixelData), imgInfo,
                         numberOfFrames, row, column, level, batchNumber,
                         offset, totalNumberOfFrames, tiled, nullptr, 0.0, 0.0,
                         outStream);
}

OFCondition DcmtkUtils::startConversion(
    int64_t imageHeight, int64_t imageWidth, uint32_t rowSize,
    const std::string& studyId, const std::string& seriesId,
    const std::string& imageName, std::unique_ptr<DcmPixelData> pixelData,
    const DcmtkImgDataInfo& imgInfo, uint32_t numberOfFrames, uint32_t row,
    uint32_t column, int level, int batchNumber, unsigned int offset,
    uint32_t totalNumberOfFrames, bool tiled, DcmTags* additionalTags,
    double firstLevelWidthMm, double firstLevelHeightMm,
    DcmOutputStream* outStream) {
  E_GrpLenEncoding grpLenEncoding = EGL_recalcGL;
  E_EncodingType encodingType = EET_ExplicitLength;
  E_PaddingEncoding paddingEncoding = EPD_noChange;
  OFCmdUnsignedInt filepad = 0;
  OFCmdUnsignedInt itempad = 0;
  E_FileWriteMode writeMode = EWM_fileformat;

  std::unique_ptr<DcmDataset> resultObject = std::make_unique<DcmDataset>();

  OFCondition cond = populateDataSet(
      imageHeight, imageWidth, rowSize, studyId, seriesId, imageName,
      std::move(pixelData), imgInfo, numberOfFrames, row, column, level,
      batchNumber, offset, totalNumberOfFrames, tiled, additionalTags,
      firstLevelWidthMm, firstLevelHeightMm, resultObject.get());

  DcmFileFormat dcmFileFormat(resultObject.get());

  DcmWriteCache wcache;
  cond = outStream->status();
  if (cond.good()) {
    dcmFileFormat.transferInit();
    cond = dcmFileFormat.write(*outStream, imgInfo.transSyn, encodingType,
                               &wcache, grpLenEncoding, paddingEncoding,
                               OFstatic_cast(Uint32, filepad),
                               OFstatic_cast(Uint32, itempad), 0, writeMode);
    dcmFileFormat.transferEnd();
  }
  if (cond.bad()) {
    BOOST_LOG_TRIVIAL(error) << "error"
                             << ": " << cond.text();
  }
  return cond;
}

}  // namespace wsiToDicomConverter
