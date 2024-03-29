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

#ifndef SRC_DCMTKIMGDATAINFO_H_
#define SRC_DCMTKIMGDATAINFO_H_

#include <dcmtk/dcmdata/libi2d/i2d.h>
#include <dcmtk/dcmdata/libi2d/i2doutpl.h>
#include <dcmtk/dcmdata/libi2d/i2dplsc.h>

#include <string>

// Structure for image metadata
struct DcmtkImgDataInfo {
  Uint16 rows;
  Uint16 cols;
  Uint16 samplesPerPixel;
  OFString photoMetrInt;
  Uint16 bitsAlloc;
  Uint16 bitsStored;
  Uint16 highBit;
  Uint16 pixelRepr;
  Uint16 planConf;
  Uint16 pixAspectH;
  Uint16 pixAspectV;
  E_TransferSyntax transSyn;
  std::string compressionRatio;
  std::string derivationDescription;

  OFBool operator==(const DcmtkImgDataInfo &other) {
    return (rows == other.rows) && (cols == other.cols) &&
           (samplesPerPixel == other.samplesPerPixel) &&
           (photoMetrInt == other.photoMetrInt) &&
           (bitsAlloc == other.bitsAlloc) &&
           (bitsStored == other.bitsStored) &&
           (highBit == other.highBit) && (pixelRepr == other.pixelRepr) &&
           (planConf == other.planConf) && (pixAspectH == other.pixAspectH) &&
           (pixAspectV == other.pixAspectV) && (transSyn == other.transSyn) &&
           (compressionRatio == other.compressionRatio) &&
           (derivationDescription == other.derivationDescription);
  }

  OFBool operator!=(const DcmtkImgDataInfo &other) {
    return !(*this == other);
  }
};

#endif  // SRC_DCMTKIMGDATAINFO_H_
