// Copyright 2022 Google LLC
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
#include "src/dcmFilePyramidSource.h"
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcpixel.h>
#include <dcmtk/dcmdata/dcpixseq.h>
#include <dcmtk/dcmdata/dcrledrg.h>
#include <dcmtk/dcmjpeg/djdecode.h>
#include <dcmtk/dcmimage/diregist.h>
#include <dcmtk/dcmimgle/diutils.h>
#include <dcmtk/dcmimgle/dcmimage.h>
#include <boost/log/trivial.hpp>
#include <boost/thread.hpp>
#include <opencv2/opencv.hpp>
#include <string>
#include <memory>
#include <utility>
#include "src/jpegUtil.h"

namespace wsiToDicomConverter {

AbstractDicomFileFrame::AbstractDicomFileFrame(
                               int64_t locationX,
                               int64_t locationY,
                               DcmFilePyramidSource* pyramidSource) :
                            BaseFileFrame(locationX, locationY, pyramidSource) {
}

void AbstractDicomFileFrame::debugLog() const {
  BOOST_LOG_TRIVIAL(info) << "AbstractDICOM File Frame: ";
}

// Returns frame component of DCM_DerivationDescription
// describes in text how frame imaging data was saved in frame.
std::string AbstractDicomFileFrame::derivationDescription() const {
  return "Generated from DICOM";
}

DICOMDatasetReader::DICOMDatasetReader(const std::string &filename) {
  dcmFile_.loadFile(filename.c_str());
  dataset_ = dcmFile_.getDataset();
}

DcmDataset *DICOMDatasetReader::dataset() {
  return dataset_;
}

boost::mutex* DICOMDatasetReader::datasetMutex() {
  return &datasetMutex_;
}

DICOMImageFrame::DICOMImageFrame(int64_t frameNumber,
                                 int64_t locationX,
                                 int64_t locationY,
                                 uint64_t dicomMemSize,
                                 DcmFilePyramidSource *pyramidSource) :
                  AbstractDicomFileFrame(locationX, locationY, pyramidSource),
                  frameNumber_(frameNumber) {
  size_ = dicomMemSize;
}

int64_t DICOMImageFrame::rawABGRFrameBytes(uint8_t *rawMemory,
                                          int64_t memorySize) {
  // DICOM frame data in native format is jpeg encoded.
  // uncompress and return # of bytes read.
  // return 0 if error occures.
  const uint64_t width = frameWidth();
  const uint64_t height = frameHeight();
  const uint64_t flags = CIF_UsePartialAccessToPixelData;
  const uint64_t frameCount = 1;
  const uint64_t bufferSize = size_;
  std::unique_ptr<uint8_t[]> buffer = std::make_unique<uint8_t[]>(bufferSize);
  int read_index = pyramidSource_->getNextDicomDatasetReaderIndex();
  DICOMDatasetReader *datasetReader =
                                pyramidSource_->dicomDatasetReader(read_index);
  {
    boost::lock_guard<boost::mutex> guard(*datasetReader->datasetMutex());
    DicomImage img(datasetReader->dataset(),
                  pyramidSource_->transferSyntax(), flags,
                  static_cast<uint64_t>(frameNumber_), frameCount);
    if (!img.getOutputData(buffer.get(), bufferSize)) {
      return 0;
    }
  }
  cv::Mat bgr(height, width, CV_8UC3, buffer.get());
  cv::Mat bgra(height, width, CV_8UC4, rawMemory);
  cv::cvtColor(bgr, bgra, cv::COLOR_BGR2BGRA);
  return width * height * 4;
}

JpegDicomFileFrame::JpegDicomFileFrame(int64_t locationX,
                                       int64_t locationY,
                                       uint8_t *dicomMem,
                                       uint64_t dicomMemSize,
                                       DcmFilePyramidSource *pyramidSource) :
                  AbstractDicomFileFrame(locationX, locationY, pyramidSource),
                  dicomFrameMemory_(dicomMem) {
  size_ = dicomMemSize;
}

J_COLOR_SPACE JpegDicomFileFrame::jpegDecodeColorSpace() const {
  return photoMetrInt() == "RGB" ? JCS_RGB : JCS_YCbCr;
}

int64_t JpegDicomFileFrame::rawABGRFrameBytes(uint8_t *rawMemory,
                                          int64_t memorySize) {
  const uint64_t width = frameWidth();
  const uint64_t height = frameHeight();
  if (jpegUtil::decodeJpeg(width, height, jpegDecodeColorSpace(),
                       dicomFrameMemory_,
                       size_, rawMemory, memorySize)) {
    return width * height * 4;
  }
  return 0;
}

Jp2KDicomFileFrame::Jp2KDicomFileFrame(int64_t locationX,
                                       int64_t locationY,
                                       uint8_t *dicomMem,
                                       uint64_t dicomMemSize,
                                       DcmFilePyramidSource *pyramidSource) :
                   AbstractDicomFileFrame(locationX, locationY, pyramidSource),
                   dicomFrameMemory_(dicomMem) {
  size_ = dicomMemSize;
}

int64_t Jp2KDicomFileFrame::rawABGRFrameBytes(uint8_t *rawMemory,
                                              int64_t memorySize) {
  cv::Mat rawData(1, size_, CV_8UC1,
                  reinterpret_cast<void*>(dicomFrameMemory_));
  cv::Mat decodedImage = cv::imdecode(rawData, cv::IMREAD_COLOR);
  if ( decodedImage.data == NULL ) {
    return 0;
  }
  const uint64_t width = frameWidth();
  const uint64_t height = frameHeight();
  cv::Mat bgra(height, width, CV_8UC4, rawMemory);
  cv::cvtColor(decodedImage, bgra, cv::COLOR_RGB2BGRA);
  return width * height * 4;
}

DcmFilePyramidSource::DcmFilePyramidSource(absl::string_view filePath) :
                      BaseFilePyramidSource<AbstractDicomFileFrame>(filePath) {
  errorMsg_ = "";
  xfer_ = EXS_Unknown;
  dcmtkCodecRegistered_ = false;
  dimensionalOrganization_ = "";
  studyInstanceUID_ = "";
  seriesInstanceUID_ = "";
  seriesDescription_ = "";
  std::string filename = static_cast<std::string>(filePath);
  dcmFile_.loadFile(filename.c_str());

  frameReaderIndex_ = 0;
  maxFrameReaderIndex_ = 30;
  dicomDatasetSpeedReader_.reserve(maxFrameReaderIndex_);
  for (int idx = 0; idx < maxFrameReaderIndex_; ++idx) {
    std::unique_ptr<DICOMDatasetReader> reader =
                                std::make_unique<DICOMDatasetReader>(filename);
    dicomDatasetSpeedReader_.push_back(std::move(reader));
  }
  dataset_ = dcmFile_.getDataset();
  frameWidth_ = getTagValueUI16(DCM_Columns);
  if (frameWidth_ <= 0) {
    setErrorMsg("DICOM missing FrameWidth.");
    return;
  }
  frameHeight_ = getTagValueUI16(DCM_Rows);
  if (frameHeight_ <= 0) {
    setErrorMsg("DICOM missing FrameHeight.");
    return;
  }
  imageWidth_ = getTagValueUI32(DCM_TotalPixelMatrixColumns);
  if (imageWidth_ <= 0) {
    setErrorMsg("DICOM missing TotalPixelMatrixColumns.");
    return;
  }
  imageHeight_ = getTagValueUI32(DCM_TotalPixelMatrixRows);
  if (imageHeight_ <= 0) {
    setErrorMsg("DICOM missing TotalPixelMatrixRows.");
    return;
  }
  int64_t frameCount  = getTagValueI64(DCM_NumberOfFrames);
  if (frameCount <= 0) {
    setErrorMsg("DICOM missing NumberOfFrames.");
    return;
  }
  photometric_ = getTagValueString(DCM_PhotometricInterpretation);
  if (photometric_ == "") {
    setErrorMsg("DICOM missing PhotometricInterpretation.");
    return;
  }
  samplesPerPixel_ = getTagValueUI16(DCM_SamplesPerPixel);
  if (samplesPerPixel_ <= 0) {
    setErrorMsg("DICOM missing SamplesPerPixel.");
    return;
  }
  planarConfiguration_ = getTagValueUI16(DCM_PlanarConfiguration);
  bitsAllocated_ = getTagValueUI16(DCM_BitsAllocated);
  if (bitsAllocated_ <= 0) {
    setErrorMsg("DICOM missing BitsAllocated.");
    return;
  }
  bitsStored_ = getTagValueUI16(DCM_BitsStored);
  if (bitsStored_ <= 0) {
    setErrorMsg("DICOM missing BitsStored.");
    return;
  }
  highBit_ = getTagValueUI16(DCM_HighBit);
  if (highBit_ <= 0) {
    setErrorMsg("DICOM missing HighBit.");
    return;
  }
  pixelRepresentation_ = getTagValueUI16(DCM_PixelRepresentation);
  firstLevelWidthMm_ = getTagValueFloat32(DCM_ImagedVolumeWidth);
  if (firstLevelWidthMm_ <= 0.0) {
    setErrorMsg("DICOM missing ImagedVolumeWidth.");
    return;
  }
  firstLevelHeightMm_ = getTagValueFloat32(DCM_ImagedVolumeHeight);
  if (firstLevelHeightMm_ <= 0.0) {
    setErrorMsg("DICOM missing ImagedVolumeHeight.");
    return;
  }
  if (tiledFull()) {
    uint64_t frames_per_row = imageWidth_ / frameWidth_;
    if (imageWidth_ % frameWidth_ > 0) {
      frames_per_row += 1;
    }
    uint64_t frames_per_column = imageHeight_ / frameHeight_;
    if (imageHeight_ % frameHeight_ > 0) {
      frames_per_column += 1;
    }
    if (frameCount != frames_per_row * frames_per_column) {
      setErrorMsg("Invalid number of frames in DICOM,");
      return;
    }
  }
  dimensionalOrganization_ = getTagValueStringArray(
                                                DCM_DimensionOrganizationType);
  studyInstanceUID_ = getTagValueStringArray(DCM_StudyInstanceUID);
  seriesInstanceUID_ = getTagValueStringArray(DCM_SeriesInstanceUID);
  seriesDescription_ = getTagValueStringArray(DCM_SeriesDescription);
  framesData_.reserve(frameCount);

  // Get pixel data sequence
  DcmStack stack;
  if (!dataset_->search(DCM_PixelData, stack, ESM_fromHere, OFFalse).good()) {
    setErrorMsg("DICOM missing PixelData");
    return;
  }
  DcmPixelData *pixelData = reinterpret_cast<DcmPixelData *>(stack.top());
  if (pixelData == nullptr || !pixelData->verify().good() ||
      !pixelData->checkValue().good()) {
    setErrorMsg("DICOM PixelData is invalid.");
    return;
  }
  const DcmRepresentationParameter *repParam = nullptr;
  pixelData->getOriginalRepresentationKey(xfer_, repParam);
  if (xfer_ == EXS_Unknown) {
    setErrorMsg("Unknown DICOM transfer syntax");
    return;
  }
  bool DecodeLossyJPEG  = false;
  bool DecodeJPEG2K = false;
  DcmPixelSequence *pixelSeq = nullptr;
  if (DcmXfer(xfer_).isEncapsulated()) {
    if (!pixelData->getEncapsulatedRepresentation(xfer_,
                                                repParam,
                                                pixelSeq).good() ||
      (pixelSeq == nullptr)) {
      setErrorMsg("Error getting pixel data.");
      return;
    }
    DecodeLossyJPEG = (EXS_JPEGProcess1 == xfer_ &&
      3 == samplesPerPixel_ &&
      0 == planarConfiguration_ &&
      0 == pixelRepresentation_ &&
      8 == bitsAllocated_ &&
      24 == bitsStored_ &&
      7 == highBit_ &&
      (photometric_ == "RGB" ||
       photometric_ == "YBR_FULL" ||
       photometric_ == "YBR_FULL_422"));
    DecodeJPEG2K = (EXS_JPEG2000LosslessOnly == xfer_ ||
      EXS_JPEG2000 == xfer_ ||
      EXS_JPEG2000MulticomponentLosslessOnly == xfer_ ||
      EXS_JPEG2000Multicomponent == xfer_);
    if (!DecodeLossyJPEG && !DecodeJPEG2K) {
      DJDecoderRegistration::registerCodecs();
      DcmRLEDecoderRegistration::registerCodecs();
      dcmtkCodecRegistered_ = true;
    }
  }
  int outputDicomImageSize = 0;
  if (!DecodeLossyJPEG && !DecodeJPEG2K) {
    const uint64_t flags = CIF_UsePartialAccessToPixelData;
    DicomImage img(dataset_, xfer_, flags, static_cast<uint64_t>(0),
                   static_cast<uint64_t>(1));
    outputDicomImageSize = img.getOutputDataSize();
    if (outputDicomImageSize == 0) {
      setErrorMsg("Error getting output image size.");
      return;
    }
  }
  uint64_t locationX = 0;
  uint64_t locationY = 0;
  for (size_t idx = 0; idx < frameCount; ++idx) {
    if (locationX > imageWidth_) {
      locationX = 0;
      locationY += frameHeight_;
    }
    if (DecodeLossyJPEG || DecodeJPEG2K) {
      DcmPixelItem *pixelItem;
      if (!pixelSeq->getItem(pixelItem, idx+1).good()) {
        setErrorMsg("Error getting DICOM Frame.");
        return;
      }
      uint64_t dicomFrameMemorySize = pixelItem->getLength();
      uint8_t *dicomFrameMemory;
      if (!pixelItem->getUint8Array(dicomFrameMemory).good()) {
        setErrorMsg("Error getting DICOM Frame Memory.");
        return;
      }
      if (DecodeLossyJPEG) {
        framesData_.push_back(std::make_unique<JpegDicomFileFrame>(locationX,
                                                          locationY,
                                                          dicomFrameMemory,
                                                          dicomFrameMemorySize,
                                                          this));
      } else {
        framesData_.push_back(std::make_unique<Jp2KDicomFileFrame>(locationX,
                                                          locationY,
                                                          dicomFrameMemory,
                                                          dicomFrameMemorySize,
                                                          this));
      }
    } else {
      framesData_.push_back(std::make_unique<DICOMImageFrame>(idx,
                                                          locationX,
                                                          locationY,
                                                          outputDicomImageSize,
                                                          this));
    }
    locationX += frameWidth_;
  }
  BOOST_LOG_TRIVIAL(info) << "Done Queueing Frames";
}

void DcmFilePyramidSource::setErrorMsg(absl::string_view msg) {
  errorMsg_ = static_cast<std::string>(msg);
  BOOST_LOG_TRIVIAL(error) << errorMsg_.c_str();
}

bool DcmFilePyramidSource::isValid() const {
  return errorMsg_ != "";
}

std::string DcmFilePyramidSource::errorMsg() const {
  return errorMsg_;
}

DICOMDatasetReader * DcmFilePyramidSource::dicomDatasetReader(int index) {
  return dicomDatasetSpeedReader_[index].get();
}

int DcmFilePyramidSource::getNextDicomDatasetReaderIndex() {
  boost::lock_guard<boost::mutex> guard(*datasetMutex());
  frameReaderIndex_ += 1;
  if (frameReaderIndex_ >= maxFrameReaderIndex_) {
    frameReaderIndex_ = 0;
  }
  return frameReaderIndex_;
}

DcmFilePyramidSource::~DcmFilePyramidSource() {
  if (dcmtkCodecRegistered_) {
    DJDecoderRegistration::cleanup();
    DcmRLEDecoderRegistration::cleanup();
  }
}

DcmDataset *DcmFilePyramidSource::dataset() {
  return dataset_;
}

boost::mutex* DcmFilePyramidSource::datasetMutex() {
  return &datasetMutex_;
}

bool DcmFilePyramidSource::tiledFull() const {
  return dimensionalOrganization_.find("TILED_FULL") != std::string::npos;
}

bool DcmFilePyramidSource::tiledSparse() const {
  return dimensionalOrganization_.find("TILED_SPARSE") != std::string::npos;
}

double DcmFilePyramidSource::getTagValueFloat32(const DcmTagKey &dcmTag) {
  Float32 value;
  if (dcmFile_.getDataset()->findAndGetFloat32(dcmTag, value, 0,
                                 OFFalse /*searchIntoSub*/).good()) {
    return value;
  }
  return 0.0;
}

int64_t DcmFilePyramidSource::getTagValueUI16(const DcmTagKey &dcmTag) {
  uint16_t value;
  if (dcmFile_.getDataset()->findAndGetUint16(dcmTag, value, 0,
                                OFFalse /*searchIntoSub*/).good()) {
    return value;
  }
  return 0;
}

int64_t DcmFilePyramidSource::getTagValueUI32(const DcmTagKey &dcmTag) {
  uint32_t value;
  if (dcmFile_.getDataset()->findAndGetUint32(dcmTag, value, 0,
                                OFFalse /*searchIntoSub*/).good()) {
    return value;
  }
  return 0;
}

int64_t DcmFilePyramidSource::getTagValueI64(const DcmTagKey &dcmTag) {
  int64_t value;
  if (dcmFile_.getDataset()->findAndGetLongInt(dcmTag, value, 0,
                                OFFalse /*searchIntoSub*/).good()) {
    return value;
  }
  return 0;
}

std::string DcmFilePyramidSource::getTagValueString(const DcmTagKey &dcmTag) {
  OFString value;
  if (dcmFile_.getDataset()->findAndGetOFString(dcmTag, value, 0,
                                  OFFalse /*searchIntoSub*/).good()) {
    return value.c_str();
  }
  return "";
}

std::string DcmFilePyramidSource::getTagValueStringArray(
                                                     const DcmTagKey &dcmTag) {
  OFString value;
  if (dcmFile_.getDataset()->findAndGetOFStringArray(dcmTag, value,
                                       OFFalse /*searchIntoSub*/).good()) {
    return value.c_str();
  }
  return "";
}

E_TransferSyntax DcmFilePyramidSource::transferSyntax() const {
  return xfer_;
}

DcmXfer DcmFilePyramidSource::transferSyntaxDcmXfer() const {
  return DcmXfer(xfer_);
}

absl::string_view DcmFilePyramidSource::studyInstanceUID() const {
  return studyInstanceUID_.c_str();
}

absl::string_view DcmFilePyramidSource::seriesInstanceUID() const {
  return seriesInstanceUID_.c_str();
}

absl::string_view DcmFilePyramidSource::seriesDescription() const {
  return seriesDescription_.c_str();
}

void DcmFilePyramidSource::debugLog() const {
  std::string tile = "";
  if (tiledFull()) {
      tile += "tile_full";
  }
  if (tiledSparse()) {
      tile += "tile_sparse";
  }
  if (tile == "") {
    tile = "unknown";
  }
  BOOST_LOG_TRIVIAL(info) << "Image Dim: " << imageWidth() << ", " <<
                             imageHeight() <<  "\n" << "Dim mm: " <<
                             imageHeightMM() << ", " << imageWidthMM() <<
                             "\n" << "Downsample: " << downsample() << "\n" <<
                             "Photometric: " <<
                      static_cast<std::string>(photometricInterpretation()) <<
                             "\n" << "Frame Count: " << fileFrameCount() <<
                             "\n" << "Tile: " << tile << "\n" <<
                             "Frame Dim: " << frameWidth() << ", " <<
                             frameHeight() <<  "\n"  << "Transfer Syntax: " <<
                             transferSyntax();
  }

}  // namespace wsiToDicomConverter
