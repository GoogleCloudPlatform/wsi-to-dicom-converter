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

#ifndef SRC_DCMFILEPYRAMIDSOURCE_H_
#define SRC_DCMFILEPYRAMIDSOURCE_H_

#include <absl/strings/string_view.h>
#include <boost/thread/mutex.hpp>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcpixseq.h>
#include <jpeglib.h>
#include <string>
#include <memory>
#include <vector>
#include "src/abstractDcmFile.h"
#include "src/frame.h"

namespace wsiToDicomConverter {

class DcmFilePyramidSource;

class AbstractDicomFileFrame : public Frame {
 public:
  AbstractDicomFileFrame(int64_t locationX,
                 int64_t locationY,
                 DcmFilePyramidSource *pyramidSource);
  virtual ~AbstractDicomFileFrame();
  virtual void sliceFrame();
  virtual std::string photoMetrInt() const;
  virtual bool hasRawABGRFrameBytes() const;
  virtual void incSourceFrameReadCounter();
  virtual void setDicomFrameBytes(std::unique_ptr<uint8_t[]> dcmdata,
                                                         uint64_t size);
  virtual void debugLog() const;

  // Returns frame component of DCM_DerivationDescription
  // describes in text how frame imaging data was saved in frame.
  virtual std::string derivationDescription() const;

 protected:
  DcmFilePyramidSource *pyramidSource_;
};

class JpegDicomFileFrame : public AbstractDicomFileFrame {
 public:
  JpegDicomFileFrame(int64_t locationX,
                int64_t locationY,
                uint8_t *dicomMem,
                uint64_t dicomMemSize,
                DcmFilePyramidSource *pyramidSource);
  virtual ~JpegDicomFileFrame();
  virtual J_COLOR_SPACE jpegDecodeColorSpace() const;
  virtual int64_t rawABGRFrameBytes(uint8_t *raw_memory, int64_t memorysize);

 private:
  const uint8_t *dicomFrameMemory_;
};

class Jp2KDicomFileFrame : public AbstractDicomFileFrame {
 public:
  Jp2KDicomFileFrame(int64_t locationX,
                int64_t locationY,
                uint8_t *dicomMem,
                uint64_t dicomMemSize,
                DcmFilePyramidSource *pyramidSource);
  virtual ~Jp2KDicomFileFrame();
  virtual int64_t rawABGRFrameBytes(uint8_t *raw_memory, int64_t memorysize);

 private:
  uint8_t *dicomFrameMemory_;
};

class DICOMImageFrame : public AbstractDicomFileFrame {
 public:
  DICOMImageFrame(int64_t frameNumber,
                  int64_t locationX,
                  int64_t locationY,
                  uint64_t dicomMemSize,
                  DcmFilePyramidSource *pyramidSource);
  virtual ~DICOMImageFrame();
  virtual int64_t rawABGRFrameBytes(uint8_t *raw_memory, int64_t memorysize);

 private:
  const uint64_t frameNumber_;
};

// Represents single DICOM file with metadata
class DcmFilePyramidSource : public AbstractDcmFile {
 public:
  explicit DcmFilePyramidSource(absl::string_view filePath);
  virtual ~DcmFilePyramidSource();
  virtual int64_t frameWidth() const;
  virtual int64_t frameHeight() const;
  virtual int64_t imageWidth() const;
  virtual int64_t imageHeight() const;
  virtual int64_t fileFrameCount() const;
  virtual int64_t downsample() const;
  virtual AbstractDicomFileFrame* frame(int64_t idx) const;
  virtual double imageHeightMM() const;
  virtual double imageWidthMM() const;
  virtual std::string photometricInterpretation() const;
  virtual std::string studyInstanceUID() const;
  virtual std::string seriesInstanceUID() const;
  virtual std::string seriesDescription() const;

  // DICOM transfer syntax objs
  virtual E_TransferSyntax transferSyntax() const;
  virtual DcmXfer transferSyntaxDcmXfer() const;

  virtual bool tiledFull() const;
  virtual bool tiledSparse() const;
  void debugLog() const;
  const char *filename() const;
  DcmDataset *dataset();
  boost::mutex *datasetMutex();

 private:
  DcmFileFormat dcmFile_;
  E_TransferSyntax xfer_;
  int64_t frameWidth_;
  int64_t frameHeight_;
  int64_t imageWidth_;
  int64_t imageHeight_;
  int64_t frameCount_;
  int64_t samplesPerPixel_;
  int64_t planarConfiguration_;
  std::string photometric_;
  double firstLevelWidthMm_;
  double firstLevelHeightMm_;
  std::string dimensionalOrganization_;
  std::vector<std::unique_ptr<AbstractDicomFileFrame>> framesData_;
  std::string filename_;
  boost::mutex datasetMutex_;
  DcmDataset *dataset_;
  bool dcmtkCodecRegistered_;
  int64_t bitsAllocated_;
  int64_t bitsStored_;
  int64_t highBit_;
  int64_t pixelRepresentation_;
  std::string studyInstanceUID_;
  std::string seriesInstanceUID_;
  std::string seriesDescription_;

  double getTagValueFloat32(const DcmTagKey &dcmTag);
  int64_t getTagValueUI16(const DcmTagKey &dcmTag);
  int64_t getTagValueUI32(const DcmTagKey &dcmTag);
  std::string getTagValueString(const DcmTagKey &dcmTag);
  std::string getTagValueStringArray(const DcmTagKey &dcmTag);
  int64_t getTagValueI64(const DcmTagKey &dcmTag);
};

}  // namespace wsiToDicomConverter
#endif  // SRC_DCMFILEPYRAMIDSOURCE_H_
