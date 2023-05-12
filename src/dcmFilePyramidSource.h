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
#include "src/baseFilePyramidSource.h"

namespace wsiToDicomConverter {

// DcmFilePyramidSource forward declared. Defined below in header file.
class DcmFilePyramidSource;

class AbstractDicomFileFrame : public BaseFileFrame<DcmFilePyramidSource> {
 public:
  AbstractDicomFileFrame(int64_t locationX,
                 int64_t locationY,
                 DcmFilePyramidSource *pyramidSource);
  virtual void debugLog() const;

  // Returns frame component of DCM_DerivationDescription
  // describes in text how frame imaging data was saved in frame.
  virtual std::string derivationDescription() const;
};

class JpegDicomFileFrame : public AbstractDicomFileFrame {
 public:
  JpegDicomFileFrame(int64_t locationX,
                int64_t locationY,
                uint8_t *dicomMem,
                uint64_t dicomMemSize,
                DcmFilePyramidSource *pyramidSource);
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
  virtual int64_t rawABGRFrameBytes(uint8_t *raw_memory, int64_t memorysize);

 private:
  const uint64_t frameNumber_;
};

class DICOMDatasetReader {
 public:
  explicit DICOMDatasetReader(const std::string &filename);
  DcmDataset *dataset();
  boost::mutex *datasetMutex();

 private:
  DcmFileFormat dcmFile_;
  DcmDataset *dataset_;
  boost::mutex datasetMutex_;
};

// Represents single DICOM file with metadata
class DcmFilePyramidSource :
                        public BaseFilePyramidSource<AbstractDicomFileFrame> {
 public:
  explicit DcmFilePyramidSource(absl::string_view filePath);
  virtual ~DcmFilePyramidSource();
  virtual absl::string_view studyInstanceUID() const;
  virtual absl::string_view seriesInstanceUID() const;
  virtual absl::string_view seriesDescription() const;

  // DICOM transfer syntax objs
  virtual E_TransferSyntax transferSyntax() const;
  virtual DcmXfer transferSyntaxDcmXfer() const;

  virtual bool tiledFull() const;
  virtual bool tiledSparse() const;
  virtual void debugLog() const;
  DcmDataset *dataset();
  boost::mutex *datasetMutex();

  int getNextDicomDatasetReaderIndex();
  DICOMDatasetReader *dicomDatasetReader(int readerIndex);

  bool isValid() const;
  std::string errorMsg() const;

 private:
  int frameReaderIndex_, maxFrameReaderIndex_;
  std::vector<std::unique_ptr<DICOMDatasetReader>> dicomDatasetSpeedReader_;

  DcmFileFormat dcmFile_;
  E_TransferSyntax xfer_;
  int64_t samplesPerPixel_;
  int64_t planarConfiguration_;
  std::string dimensionalOrganization_;
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
  std::string errorMsg_;

  double getTagValueFloat32(const DcmTagKey &dcmTag);
  int64_t getTagValueUI16(const DcmTagKey &dcmTag);
  int64_t getTagValueUI32(const DcmTagKey &dcmTag);
  std::string getTagValueString(const DcmTagKey &dcmTag);
  std::string getTagValueStringArray(const DcmTagKey &dcmTag);
  int64_t getTagValueI64(const DcmTagKey &dcmTag);
  void setErrorMsg(absl::string_view msg);
};

}  // namespace wsiToDicomConverter
#endif  // SRC_DCMFILEPYRAMIDSOURCE_H_
