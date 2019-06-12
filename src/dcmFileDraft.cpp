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

#include "dcmFileDraft.h"
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcdict.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcpixseq.h>
#include <dcmtk/dcmdata/dcpxitem.h>
#include <dcmtk/dcmdata/dcuid.h>
#include <dcmtk/dcmdata/dcvrat.h>
#include <dcmtk/dcmdata/libi2d/i2d.h>
#include <dcmtk/dcmdata/libi2d/i2doutpl.h>
#include <dcmtk/dcmdata/libi2d/i2dplsc.h>
#include <boost/chrono.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/log/trivial.hpp>
#include <boost/thread/thread.hpp>
#include <ctime>
#include <iomanip>
#include <vector>
using namespace std;

inline DcmItem *generateFramePositionMetadata(DcmDataset *resultObject,
                                              uint32_t numberOfFrames,
                                              uint32_t rowSize, uint32_t row,
                                              uint32_t column, uint32_t x,
                                              uint32_t y) {
  DcmSequenceOfItems *PerFrameFunctionalGroupsSequence =
      new DcmSequenceOfItems(DCM_PerFrameFunctionalGroupsSequence);
  for (uint32_t frameNumber = 0; frameNumber < numberOfFrames; frameNumber++) {
    if (column > rowSize) {
      column = 1;
      row++;
    }
    string index = to_string(column) + "\\" + to_string(row);
    DcmItem *dimension = new DcmItem;
    dimension->putAndInsertString(DCM_DimensionIndexValues, index.c_str());

    DcmSequenceOfItems *sequenceDimension =
        new DcmSequenceOfItems(DCM_FrameContentSequence);
    sequenceDimension->insert(dimension);

    DcmItem *pixelPosition = new DcmItem;
    pixelPosition->putAndInsertSint32(DCM_ColumnPositionInTotalImagePixelMatrix,
                                      (column - 1) * x + 1);
    pixelPosition->putAndInsertSint32(DCM_RowPositionInTotalImagePixelMatrix,
                                      (row - 1) * y + 1);

    DcmSequenceOfItems *sequencePosition =
        new DcmSequenceOfItems(DCM_PlanePositionSlideSequence);
    sequencePosition->insert(pixelPosition);

    DcmItem *positionItem = new DcmItem;
    positionItem->insert(sequenceDimension);
    positionItem->insert(sequencePosition);
    PerFrameFunctionalGroupsSequence->insert(positionItem);
    column++;
  }
  resultObject->insert(PerFrameFunctionalGroupsSequence);
  return resultObject;
}

inline DcmItem *generateSharedFunctionalGroupsSequence(DcmDataset *resultObject,
                                                       double pixelSizeMm) {
  if (pixelSizeMm <= 0) {
    pixelSizeMm = 0.1;
  }
  DcmItem *sharedFunctionalGroupsSequence = new DcmItem();
  DcmItem *pixelMeasuresSequence = new DcmItem();
  string pixelSizeMmStr = to_string(pixelSizeMm);
  pixelSizeMmStr = pixelSizeMmStr + "\\" + pixelSizeMmStr;
  pixelMeasuresSequence->putAndInsertString(DCM_PixelSpacing,
                                            pixelSizeMmStr.c_str());
  sharedFunctionalGroupsSequence->insertSequenceItem(DCM_PixelMeasuresSequence,
                                                     pixelMeasuresSequence);
  resultObject->insertSequenceItem(DCM_SharedFunctionalGroupsSequence,
                                   sharedFunctionalGroupsSequence);
  return resultObject;
}

DcmPixelSequence *addFrame(uint8_t *bytes, uint64_t size,
                           DcmOffsetList &offsetList,
                           DcmPixelSequence *compressedPixelSequence) {
  compressedPixelSequence->storeCompressedFrame(offsetList, bytes, size, 0U);

  return compressedPixelSequence;
}

inline DcmItem *pointerItem(char *dimensionOrganizationUIDstr) {
  DcmItem *ponteritem = new DcmItem;
  ponteritem->putAndInsertOFStringArray(DCM_DimensionOrganizationUID,
                                        dimensionOrganizationUIDstr);
  DcmAttributeTag *slideSequence =
      new DcmAttributeTag(DCM_FunctionalGroupPointer);
  slideSequence->putTagVal(DCM_PlanePositionSlideSequence);
  DcmAttributeTag *columnPosition =
      new DcmAttributeTag(DCM_DimensionIndexPointer);
  columnPosition->putTagVal(DCM_ColumnPositionInTotalImagePixelMatrix);
  ponteritem->insert(slideSequence);
  ponteritem->insert(columnPosition);
  return ponteritem;
}
inline DcmItem *generateDimensionIndexSequence(DcmDataset *resultObject) {
  DcmItem *dimensionOrganizationUID = new DcmItem;
  char dimensionOrganizationUIDstr[100];
  dimensionOrganizationUID->putAndInsertOFStringArray(
      DCM_DimensionOrganizationUID,
      dcmGenerateUniqueIdentifier(dimensionOrganizationUIDstr,
                                  SITE_STUDY_UID_ROOT));
  DcmSequenceOfItems *dimensionOrganizationSequence =
      new DcmSequenceOfItems(DCM_DimensionOrganizationSequence);
  dimensionOrganizationSequence->insert(dimensionOrganizationUID);
  resultObject->insert(dimensionOrganizationSequence);

  DcmSequenceOfItems *sequence =
      new DcmSequenceOfItems(DCM_DimensionIndexSequence);
  sequence->insert(pointerItem(dimensionOrganizationUIDstr));
  sequence->insert(pointerItem(dimensionOrganizationUIDstr));
  resultObject->insert(sequence);
  return resultObject;
}

string formatTime(string format) {
  stringstream stringStream;
  time_t timeNow = time(nullptr);
  stringStream << put_time(gmtime(&timeNow), format.c_str());
  return stringStream.str();
}

string currentDate() { return formatTime("%Y%m%d"); }
string currentTime() { return formatTime("%OH%OM%OS"); }

OFCondition insertPixelMetadata(DcmDataset *dset, DcmtkImgDataInfo &imgInfo,
                                uint32_t numberOfFrames) {
  DCMDATA_LIBI2D_DEBUG("Image2Dcm: Inserting Image Pixel module information");

  OFCondition cond = dset->putAndInsertUint16(DCM_SamplesPerPixel,
                                              imgInfo.samplesPerPixel);  // 3
  if (cond.bad()) return cond;

  cond = dset->putAndInsertOFStringArray(DCM_PhotometricInterpretation,
                                         imgInfo.photoMetrInt);  // RGB
  if (cond.bad()) return cond;

  cond =
      dset->putAndInsertUint16(DCM_PlanarConfiguration, imgInfo.planConf);  // 0
  if (cond.bad()) return cond;

  cond = dset->putAndInsertUint16(DCM_Rows, imgInfo.rows);
  if (cond.bad()) return cond;

  cond = dset->putAndInsertUint16(DCM_Columns, imgInfo.cols);
  if (cond.bad()) return cond;

  cond = dset->putAndInsertUint16(DCM_BitsAllocated, imgInfo.bitsAlloc);  // 8
  if (cond.bad()) return cond;

  cond = dset->putAndInsertUint16(DCM_BitsStored, imgInfo.bitsStored);  // 8
  if (cond.bad()) return cond;

  cond = dset->putAndInsertUint16(DCM_HighBit, imgInfo.highBit);  // 7
  if (cond.bad()) return cond;

  if (numberOfFrames >= 1) {
    char buf[200];
    int err = sprintf(buf, "%u", numberOfFrames);
    if (err == -1) return EC_IllegalCall;
    cond = dset->putAndInsertOFStringArray(DCM_NumberOfFrames, buf);
    if (cond.bad()) return cond;

    cond = dset->putAndInsertString(DCM_FrameIncrementPointer, "");
    if (cond.bad()) return cond;
  }

  return dset->putAndInsertUint16(DCM_PixelRepresentation,
                                  imgInfo.pixelRepr);  // 0
}

OFCondition generateDcmDataset(I2DOutputPlug *outPlug, DcmDataset *&resultDset,
                               DcmtkImgDataInfo &imgInfo,
                               uint32_t numberOfFrames) {
  if (!outPlug) return EC_IllegalParameter;

  OFCondition cond;

  OFunique_ptr<DcmDataset> tempDataset(resultDset);

  if (!tempDataset.get()) return EC_MemoryExhausted;

  if (cond.bad()) return cond;

  cond = insertPixelMetadata(tempDataset.get(), imgInfo, numberOfFrames);

  if (cond.bad()) {
    return cond;
  }

  OFBool srcIsLossy = imgInfo.transSyn == EXS_JPEGProcess1;

  if (srcIsLossy) {
    cond = tempDataset->putAndInsertOFStringArray(DCM_LossyImageCompression,
                                                  "01", true);
    if (cond.bad())
      return makeOFCondition(
          OFM_dcmdata, 18, OF_error,
          "Unable to write attribute Lossy Image Compression and/or Lossy "
          "Image Compression Method to result dataset");
  }

  cond = outPlug->convert(*tempDataset);
  if (cond.bad()) {
    return cond;
  }

  if (cond.bad()) {
    return cond;
  }

  resultDset = tempDataset.release();
  return EC_Normal;
}

dcmFileDraft::dcmFileDraft(
    vector<Frame *> framesData, string outputFileMask, uint32_t numberOfFrames,
    int64_t imageWidth, int64_t imageHeight, int32_t level, int32_t batchNumber,
    uint32_t batch, uint32_t row, uint32_t column, int64_t frameWidth,
    int64_t frameHeight, string studyId, string seriesId, string imageName,
    DCM_Compression compression, bool tiled, DcmTags *additionalTags,
    double firstLevelWidthMm, double firstLevelHeightMm) {
  framesData_ = framesData;
  outputFileMask_ = outputFileMask;
  numberOfFrames_ = numberOfFrames;
  imageWidth_ = imageWidth;
  imageHeight_ = imageHeight;
  level_ = level;
  batchNumber_ = batchNumber;
  batchSize_ = batch;
  row_ = row;
  column_ = column;
  frameWidth_ = frameWidth;
  frameHeight_ = frameHeight;
  studyId_ = studyId;
  seriesId_ = seriesId;
  imageName_ = imageName;
  compression_ = compression;
  tiled_ = tiled;
  additionalTags_ = additionalTags;
  firstLevelWidthMm_ = firstLevelWidthMm;
  firstLevelHeightMm_ = firstLevelHeightMm;
}

dcmFileDraft::~dcmFileDraft() {
  for (Frame *frameData : framesData_) {
    delete frameData;
  }
  framesData_.clear();
}

void dcmFileDraft::saveFile() {
  DcmPixelData *pixelData = new DcmPixelData(DCM_PixelData);
  DcmOffsetList offsetList;
  DcmPixelSequence *compressedPixelSequence =
      new DcmPixelSequence(DCM_PixelSequenceTag);
  DcmPixelItem *offsetTable = new DcmPixelItem(DcmTag(DCM_Item, EVR_OB));
  compressedPixelSequence->insert(offsetTable);
  DcmtkImgDataInfo imgInfo;
  vector<uint8_t> frames;
  for (size_t frameNumber = 0; frameNumber < framesData_.size();
       frameNumber++) {
    while (!framesData_[frameNumber]->isDone()) {
      boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
    }
    if (compression_ == JPEG || compression_ == JPEG2000) {
      addFrame(framesData_[frameNumber]->getData(),
               framesData_[frameNumber]->getSize(), offsetList,
               compressedPixelSequence);
    } else {
      frames.insert(frames.end(), framesData_[frameNumber]->getData(),
                    framesData_[frameNumber]->getData() +
                        framesData_[frameNumber]->getSize());
    }
  }

  switch (compression_) {
    case JPEG:
      imgInfo.transSyn = EXS_JPEGProcess1;
      pixelData->putOriginalRepresentation(imgInfo.transSyn, nullptr,
                                           compressedPixelSequence);
      break;
    case JPEG2000:
      imgInfo.transSyn = EXS_JPEG2000LosslessOnly;
      pixelData->putOriginalRepresentation(imgInfo.transSyn, nullptr,
                                           compressedPixelSequence);
      break;
    default:
      imgInfo.transSyn = EXS_LittleEndianExplicit;
      pixelData->putUint8Array(&frames[0], frames.size());
      delete compressedPixelSequence;
  }
  imgInfo.samplesPerPixel = 3;
  imgInfo.photoMetrInt = "RGB";
  imgInfo.planConf = 0;
  imgInfo.rows = frameHeight_;
  imgInfo.cols = frameWidth_;
  imgInfo.bitsAlloc = 8;
  imgInfo.bitsStored = 8;
  imgInfo.highBit = 7;
  imgInfo.pixelRepr = 0;
  OFString fileName =
      OFString((outputFileMask_ + "/level-" + to_string(level_) + "-frames-" +
                to_string(numberOfFrames_ - batchSize_) + "-" +
                to_string(numberOfFrames_) + ".dcm")
                   .c_str());
  uint32_t rowSize = 1 + ((imageWidth_ - 1) / frameWidth_);
  uint32_t totalNumberOfFrames =
      rowSize * (1 + ((imageHeight_ - 1) / frameHeight_));
  dcmFileDraft::startConversion(
      fileName, imageHeight_, imageWidth_, rowSize, studyId_, seriesId_,
      imageName_, pixelData, imgInfo, batchSize_, row_, column_, level_,
      batchNumber_, numberOfFrames_ - batchSize_, totalNumberOfFrames, tiled_,
      additionalTags_, firstLevelWidthMm_, firstLevelHeightMm_);
}

OFCondition dcmFileDraft::startConversion(
    OFString outputFileName, int64_t imageHeight, int64_t imageWidth,
    uint32_t rowSize, string studyId, string seriesId, string imageName,
    DcmPixelData *pixelData, DcmtkImgDataInfo &imgInfo, uint32_t numberOfFrames,
    uint32_t row, uint32_t column, int level, int batchNumber,
    unsigned int offset, uint32_t totalNumberOfFrames, bool tiled,
    DcmTags *additionalTags, double firstLevelWidthMm,
    double firstLevelHeightMm) {
  I2DOutputPlug *outPlug = nullptr;
  E_GrpLenEncoding grpLenEncoding = EGL_recalcGL;
  E_EncodingType encodingType = EET_ExplicitLength;
  E_PaddingEncoding paddingEncoding = EPD_noChange;
  OFCmdUnsignedInt filepad = 0;
  OFCmdUnsignedInt itempad = 0;
  E_FileWriteMode writeMode = EWM_fileformat;
  OFString pixDataFile, outputFile;

  if (outputFileName.empty()) {
    return EC_IllegalCall;
  } else {
    outputFile = outputFileName;
  }
  outPlug = new I2DOutputPlugSC();

  if (outPlug == nullptr) return EC_MemoryExhausted;

  OFBool insertType2 = OFTrue;
  OFBool inventType1 = OFTrue;
  OFBool doChecks = OFTrue;

  outPlug->setValidityChecking(doChecks, insertType2, inventType1);

  OFCondition cond;

  DcmDataset *resultObject = new DcmDataset();

  if (!dcmDataDict.isDictionaryLoaded()) {
    dcmDataDict.wrlock().reloadDictionaries(true, false);
    dcmDataDict.unlock();
  }

  generateDcmDataset(outPlug, resultObject, imgInfo, numberOfFrames);
  resultObject->insert(pixelData);

  resultObject->putAndInsertOFStringArray(DCM_ContentDate,
                                          currentDate().c_str());

  resultObject->putAndInsertOFStringArray(DCM_ContentTime,
                                          currentTime().c_str());
  if (studyId.size() > 0) {
    resultObject->putAndInsertOFStringArray(DCM_StudyInstanceUID,
                                            studyId.c_str());
  }

  if (seriesId.size() > 0) {
    resultObject->putAndInsertOFStringArray(DCM_SeriesInstanceUID,
                                            seriesId.c_str());
  }
  char instanceUidGenerated[100];
  dcmGenerateUniqueIdentifier(instanceUidGenerated, SITE_INSTANCE_UID_ROOT);
  resultObject->putAndInsertOFStringArray(DCM_SOPInstanceUID,
                                          instanceUidGenerated);
  resultObject->putAndInsertOFStringArray(DCM_SeriesDescription,
                                          imageName.c_str());
  resultObject->putAndInsertOFStringArray(DCM_Modality, "SM");
  resultObject->putAndInsertOFStringArray(
      DCM_SOPClassUID, UID_VLWholeSlideMicroscopyImageStorage);
  resultObject->putAndInsertUint32(DCM_TotalPixelMatrixColumns, imageWidth);
  resultObject->putAndInsertUint32(DCM_TotalPixelMatrixRows, imageHeight);

  resultObject->putAndInsertOFStringArray(DCM_InstanceNumber,
                                          to_string(level + 1).c_str());
  resultObject->putAndInsertUint16(DCM_RepresentativeFrameNumber, 0);

  unsigned int concatenationTotalNumber;
  if (totalNumberOfFrames - offset == numberOfFrames) {
    concatenationTotalNumber = batchNumber + 1;
  } else {
    concatenationTotalNumber = (totalNumberOfFrames + 1) / (numberOfFrames - 1);
  }
  if (concatenationTotalNumber > 1) {
    resultObject->putAndInsertUint32(DCM_ConcatenationFrameOffsetNumber,
                                     offset);
    resultObject->putAndInsertUint16(DCM_InConcatenationNumber,
                                     batchNumber + 1);
    resultObject->putAndInsertUint16(DCM_InConcatenationTotalNumber,
                                     concatenationTotalNumber);
    resultObject->putAndInsertOFStringArray(
        DCM_ConcatenationUID, (seriesId + "." + to_string(level + 1)).c_str());
  }
  resultObject->putAndInsertOFStringArray(
      DCM_FrameOfReferenceUID, (seriesId + "." + to_string(level + 1)).c_str());

  if (tiled) {
    resultObject->putAndInsertOFStringArray(DCM_DimensionOrganizationType,
                                            "TILED_FULL");

  } else {
    resultObject->putAndInsertOFStringArray(DCM_DimensionOrganizationType,
                                            "TILED_SPARSE");
    generateFramePositionMetadata(resultObject, numberOfFrames, rowSize, row,
                                  column, imgInfo.rows, imgInfo.cols);
  }
  if (firstLevelWidthMm > 0 && firstLevelHeightMm > 0) {
    resultObject->putAndInsertFloat32(DCM_ImagedVolumeWidth, firstLevelWidthMm);
    resultObject->putAndInsertFloat32(DCM_ImagedVolumeHeight,
                                      firstLevelHeightMm);
  }

  resultObject->putAndInsertOFStringArray(DCM_ImageType,
                                          "DERIVED\\PRIMARY\\VOLUME\\NONE");
  resultObject->putAndInsertOFStringArray(DCM_ImageOrientationSlide,
                                          "0\\-1\\0\\-1\\0\\0");
  generateSharedFunctionalGroupsSequence(resultObject,
                                         firstLevelHeightMm / imageHeight);

  generateDimensionIndexSequence(resultObject);
  additionalTags->populateDataset(resultObject);
  DcmFileFormat dcmFileFormat(resultObject);
  cond = dcmFileFormat.saveFile(outputFile.c_str(), imgInfo.transSyn,
                                encodingType, grpLenEncoding, paddingEncoding,
                                OFstatic_cast(Uint32, filepad),
                                OFstatic_cast(Uint32, itempad), writeMode);
  if (cond.good()) {
    BOOST_LOG_TRIVIAL(info) << outputFile.c_str() << " is created";
  } else {
    BOOST_LOG_TRIVIAL(error)
        << "error" << outputFile.c_str() << ": " << cond.text();
  }
  delete outPlug;
  outPlug = nullptr;
  delete resultObject;
  return cond;
}
