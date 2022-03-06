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

#include "src/dcmFileDraft.h"
#include <absl/strings/string_view.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcdict.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcostrmf.h>
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
#include <string>
#include <utility>
#include <vector>
#include "src/dcmtkUtils.h"

namespace wsiToDicomConverter {
DcmFileDraft::DcmFileDraft(
    std::vector<std::unique_ptr<Frame> > framesData,
    absl::string_view outputFileMask, int64_t imageWidth, int64_t imageHeight,
    int64_t level, int64_t row, int64_t column, absl::string_view studyId,
    absl::string_view seriesId, absl::string_view imageName,
    DCM_Compression compression, bool tiled, DcmTags* additionalTags,
    double firstLevelWidthMm, double firstLevelHeightMm, int64_t downsample,
    const std::vector<std::unique_ptr<AbstractDcmFile>> * prior_frame_batches,
    absl::string_view sourceImageDescription) {
  framesData_ = std::move(framesData);
  outputFileMask_ = std::move(static_cast<std::string>(outputFileMask));
  sourceImageDescription_ = std::move(static_cast<std::string>(
                                                      sourceImageDescription));
  imageWidth_ = imageWidth;
  imageHeight_ = imageHeight;
  level_ = level;
  prior_batch_frames_ = 0;
  if (prior_frame_batches == NULL) {
    batchNumber_ = 0;
  } else {
    batchNumber_ =  prior_frame_batches->size();
    for (size_t idx = 0; idx < batchNumber_; ++idx) {
      prior_batch_frames_ += prior_frame_batches->at(idx)->fileFrameCount();
    }
  }

  row_ = row;
  column_ = column;
  if (framesData_.size() == 0) {
    frameWidth_ = 0;
    frameHeight_ = 0;
  } else {
    frameWidth_ = framesData_.at(0)->frameWidth();
    frameHeight_ = framesData_.at(0)->frameHeight();
  }
  studyId_ = std::move(static_cast<std::string>(studyId));
  seriesId_ = std::move(static_cast<std::string>(seriesId));
  imageName_ = std::move(static_cast<std::string>(imageName));
  compression_ = compression;
  tiled_ = tiled;
  additionalTags_ = additionalTags;
  firstLevelWidthMm_ = firstLevelWidthMm;
  firstLevelHeightMm_ = firstLevelHeightMm;
  downsample_ = downsample;
}

DcmFileDraft::~DcmFileDraft() {}

int64_t DcmFileDraft::downsample() const {
  return downsample_;
}

int64_t DcmFileDraft::frameWidth() const {
  return frameWidth_;
}

int64_t DcmFileDraft::frameHeight() const {
  return frameHeight_;
}

int64_t DcmFileDraft::imageWidth() const {
  return imageWidth_;
}

int64_t DcmFileDraft::imageHeight() const {
  return imageHeight_;
}

int64_t DcmFileDraft::fileFrameCount() const {
  return framesData_.size();
}

Frame* DcmFileDraft::frame(int64_t idx) const {
  return framesData_.at(idx).get();
}

void DcmFileDraft::write(DcmOutputStream* outStream) {
  std::unique_ptr<DcmPixelData> pixelData =
      std::make_unique<DcmPixelData>(DCM_PixelData);
  DcmOffsetList offsetList;
  std::unique_ptr<DcmPixelSequence> compressedPixelSequence =
      std::make_unique<DcmPixelSequence>(DCM_PixelSequenceTag);
  std::unique_ptr<DcmPixelItem> offsetTable =
      std::make_unique<DcmPixelItem>(DcmTag(DCM_Item, EVR_OB));
  compressedPixelSequence->insert(offsetTable.release());
  DcmtkImgDataInfo imgInfo;
  std::vector<uint8_t> frames;

  // What channel components in represent.
  // Values = RGB or YBR_FULL_422.
  // value determined by compression JPEG2000 & RAW = RGB
  // Jpeg compressed = YBR_FULL_422
  // TiffFrame jpeg compressed = RGB or YBR_FULL_422 value
  // dependent on encoding of jpeg.
  std::string framePhotoMetrIntrp = "";

  // Text description of rough image processing which generated frames.
  std::string derivationDescription = "";

  // Actual size of imaging for frames writen in dicom file.
  // Summed across frames.  Used to calculate imaging compression ratio.
  int64_t imagingSizeBytes = 0;

  const int64_t  frameDataSize = framesData_.size();
  // get general state from first frame.
  if (frameDataSize > 0) {
    const int firstFrameNumber = 0;
    while (!framesData_[firstFrameNumber]->isDone()) {
      boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
    }
    Frame *frame = framesData_[firstFrameNumber].get();
    framePhotoMetrIntrp = frame->photoMetrInt();
    derivationDescription = sourceImageDescription_ +
                            frame->derivationDescription();
    // maximum length of derivationDescription vr type is 1024 chars
    // subtracting 1 to allow for null.
    derivationDescription = std::move(derivationDescription.substr(0, 1023));
  }

  // if image is encoded as raw compute total size of all frames and reserve
  // memory to speed addition of data to raw write buffer.
  uint64_t totalFrameByteSize = 0;
  for (size_t frameNumber = 0; frameNumber < frameDataSize; ++frameNumber) {
    while (!framesData_[frameNumber]->isDone()) {
      boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
    }
    Frame *frame = framesData_[frameNumber].get();
    if (frame->hasDcmPixelItem()) {
      break;  // Jpeg or JPeg2000 encoded data.
    } else {
      totalFrameByteSize += frame->dicomFrameBytesSize();
    }
  }
  if (totalFrameByteSize > 0) {
    frames.reserve(totalFrameByteSize);
  }

  for (size_t frameNumber = 0; frameNumber < frameDataSize; ++frameNumber) {
    while (!framesData_[frameNumber]->isDone()) {
      boost::this_thread::sleep_for(boost::chrono::milliseconds(100));
    }
    Frame *frame = framesData_[frameNumber].get();
    if (frame->hasDcmPixelItem()) {  // Jpeg or JPeg2000 encoded data.
      // if currentSize is odd this will be fixed by dcmtk during
      // DcmOtherByteOtherWord::write()
      // passing ownership of frame pointer to dcmtk.
      compressedPixelSequence->insert(frame->dcmPixelItem());
      // 8 bytes extra for header
      Uint32 currentSize = frame->dicomFrameBytesSize() + 8;
      // odd frame size requires padding, i.e. last fragment uses odd
      // length pixel item
      if (currentSize & 1) {
        currentSize++;
      }
      offsetList.push_back(currentSize);
    } else {
      frames.insert(frames.end(), frame->dicomFrameBytes(),
                    frame->dicomFrameBytes() + frame->dicomFrameBytesSize());
      frame->clearDicomMem();  // memory copied clear.
    }
    imagingSizeBytes += frame->dicomFrameBytesSize();
  }
  if (imagingSizeBytes > 0) {
    // compute uncompressed size realtive to frames written in file. Possible
    // to split frames across multiple files.
    const double uncompressed = static_cast<double>(3 * frameWidth_ *
                                                frameHeight_ * frameDataSize);
    const double storedImageSize = static_cast<double>(imagingSizeBytes);
    const double compression_ratio = uncompressed / storedImageSize;
    imgInfo.compressionRatio = std::to_string(compression_ratio);
    imgInfo.derivationDescription = derivationDescription;
  } else {
    imgInfo.compressionRatio = "";
    imgInfo.derivationDescription = "";
  }

  switch (compression_) {
    case JPEG:
      imgInfo.transSyn = EXS_JPEGProcess1;

      imgInfo.photoMetrInt = (framePhotoMetrIntrp.empty()) ?
                                  "YBR_FULL_422" : framePhotoMetrIntrp.c_str();
      pixelData->putOriginalRepresentation(imgInfo.transSyn, nullptr,
                                           compressedPixelSequence.release());
      break;
    case JPEG2000:
      imgInfo.transSyn = EXS_JPEG2000LosslessOnly;
      imgInfo.photoMetrInt = (framePhotoMetrIntrp.empty()) ?
                                           "RGB" : framePhotoMetrIntrp.c_str();
      pixelData->putOriginalRepresentation(imgInfo.transSyn, nullptr,
                                           compressedPixelSequence.release());
      break;
    default:
      imgInfo.transSyn = EXS_LittleEndianExplicit;
      imgInfo.photoMetrInt = (framePhotoMetrIntrp.empty()) ?
                                           "RGB" : framePhotoMetrIntrp.c_str();
      pixelData->putUint8Array(&frames[0], frames.size());
  }
  imgInfo.samplesPerPixel = 3;
  imgInfo.planConf = 0;
  imgInfo.rows = frameHeight_;
  imgInfo.cols = frameWidth_;
  imgInfo.bitsAlloc = 8;
  imgInfo.bitsStored = 8;
  imgInfo.highBit = 7;
  imgInfo.pixelRepr = 0;
  const int64_t batchSize = fileFrameCount();
  const int64_t numberOfFrames = batchSize + prior_batch_frames_;
  uint32_t rowSize = 1 + ((imageWidth_ - 1) / frameWidth_);
  uint32_t totalNumberOfFrames =
      rowSize * (1 + ((imageHeight_ - 1) / frameHeight_));
  DcmtkUtils::startConversion(
      imageHeight_, imageWidth_, rowSize, studyId_, seriesId_, imageName_,
      std::move(pixelData), imgInfo, batchSize, row_, column_, level_,
      batchNumber_, numberOfFrames - batchSize, totalNumberOfFrames, tiled_,
      additionalTags_, firstLevelWidthMm_, firstLevelHeightMm_, outStream);
}

void DcmFileDraft::saveFile() {
  const int64_t batchSize = fileFrameCount();
  const int64_t numberOfFrames = batchSize + prior_batch_frames_;
  OFString fileName =
      OFString((outputFileMask_ + "/level-" + std::to_string(level_) +
                "-frames-" + std::to_string(numberOfFrames - batchSize) +
                "-" + std::to_string(numberOfFrames) + ".dcm")
                   .c_str());
  std::unique_ptr<DcmOutputFileStream> fileStream =
      std::make_unique<DcmOutputFileStream>(fileName);
  write(fileStream.get());
}

}  // namespace wsiToDicomConverter
