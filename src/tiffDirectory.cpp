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
#include <boost/log/trivial.hpp>

#include <cmath>
#include <memory>
#include <string>

#include "src/tiffDirectory.h"

namespace wsiToDicomConverter {

TiffDirectory::TiffDirectory(TIFF *tiff) {      // comment # is tiff tag number
  directoryIndex_ = TIFFCurrentDirectory(tiff);
  _getTiffField_ui32(tiff, TIFFTAG_SUBFILETYPE, &subfileType_);  // 254
  _getTiffField_ui32(tiff, TIFFTAG_IMAGEWIDTH, &imageWidth_);    // 256
  _getTiffField_ui32(tiff, TIFFTAG_IMAGELENGTH, &imageHeight_);  // 257
  _getTiffField_ui16(tiff, TIFFTAG_BITSPERSAMPLE, &bitsPerSample_);  // 258
  _getTiffField_ui16(tiff, TIFFTAG_COMPRESSION, &compression_);  // 259
  _getTiffField_ui16(tiff, TIFFTAG_PHOTOMETRIC, &photoMetric_);  // 262
  _getTiffField_str(tiff, TIFFTAG_IMAGEDESCRIPTION, &imageDescription_);  // 270
  _getTiffField_ui16(tiff, TIFFTAG_ORIENTATION, &orientation_);  // 274
  _getTiffField_ui16(tiff, TIFFTAG_SAMPLESPERPIXEL, &samplePerPixel_);  // 277
  _getTiffField_ui32(tiff, TIFFTAG_ROWSPERSTRIP, &rowsPerStrip_);  // 278
  _getTiffField_f(tiff, TIFFTAG_XRESOLUTION, &xResolution_);  // 282
  _getTiffField_f(tiff, TIFFTAG_YRESOLUTION, &yResolution_);  // 283
  _getTiffField_ui16(tiff, TIFFTAG_PLANARCONFIG, &planarConfig_);  // 284
  _getTiffField_ui32(tiff, TIFFTAG_TILEWIDTH, &tileWidth_);  // 322
  _getTiffField_ui32(tiff, TIFFTAG_TILELENGTH, &tileHeight_);  // 323
  _getTiffField_ui32(tiff, TIFFTAG_IMAGEDEPTH, &imageDepth_);  // 32997
  hasIccProfile_ = _hasICCProfile(tiff);  // 34675
  _getTiffField_ui32(tiff, TIFFTAG_TILEDEPTH, &tileDepth_);  // 32998
  isTiled_ = TIFFIsTiled(tiff);
  if (!isTiled_) {
    tileCount_ = 0;
  } else {
    tileCount_ = TIFFNumberOfTiles(tiff);  // tiles_x * tiles_y * tiles_z
  }
  _getTiffField_jpegTables(tiff, &jpegTableDataSize_, &jpegTableData_);
  _getTiffField_ui32(tiff, TIFFTAG_JPEGQUALITY, &jpegQuality_);
  _getTiffField_ui32(tiff, TIFFTAG_JPEGCOLORMODE, &jpegColorMode_);
  _getTiffField_ui32(tiff, TIFFTAG_JPEGTABLESMODE, &jpegTableMode_);
}

TiffDirectory::TiffDirectory(const TiffDirectory &dir) {
  directoryIndex_ = dir.directoryIndex_;
  subfileType_ = dir.subfileType_;
  imageWidth_ = dir.imageWidth_;
  imageHeight_ = dir.imageHeight_;
  bitsPerSample_ = dir.bitsPerSample_;
  compression_ = dir.compression_;
  photoMetric_ = dir.photoMetric_;
  imageDescription_ = dir.imageDescription_;
  orientation_ = dir.orientation_;
  samplePerPixel_ = dir.samplePerPixel_;
  rowsPerStrip_ = dir.rowsPerStrip_;
  planarConfig_ = dir.planarConfig_;
  tileWidth_ = dir.tileWidth_;
  tileHeight_ = dir.tileHeight_;
  imageDepth_ = dir.imageDepth_;
  tileDepth_ = dir.tileDepth_;
  hasIccProfile_ = dir.hasIccProfile_;
  xResolution_ = dir.xResolution_;
  yResolution_ = dir.yResolution_;
  tileCount_ = dir.tileCount_;
  isTiled_ = dir.isTiled_;
  jpegTableDataSize_ = dir.jpegTableDataSize_;
  if (jpegTableDataSize_ <= 0 || dir.jpegTableData_ == nullptr) {
    jpegTableData_ = nullptr;
  } else {
    jpegTableData_ = std::make_unique<uint8_t[]>(jpegTableDataSize_);
    memcpy(jpegTableData_.get(), dir.jpegTableData_.get(), jpegTableDataSize_);
  }
  jpegQuality_ = dir.jpegQuality_;
  jpegColorMode_ = dir.jpegColorMode_;
  jpegTableMode_ = dir.jpegTableMode_;
  photoMetricStr_ = isPhotoMetricRGB() ? "RGB" : "YBR_FULL_422";
}

TiffDirectory::~TiffDirectory() {}

tdir_t TiffDirectory::directoryIndex() const { return directoryIndex_; }

bool TiffDirectory::hasICCProfile() const { return hasIccProfile_; }  // 34675

int64_t TiffDirectory::subfileType() const { return subfileType_; }    // 254

int64_t TiffDirectory::imageWidth() const { return imageWidth_; }     // 256

int64_t TiffDirectory::imageHeight() const { return imageHeight_; }    // 257

int64_t TiffDirectory::imageDepth() const { return imageDepth_; }     // 32997

int64_t TiffDirectory::bitsPerSample() const { return bitsPerSample_; }  // 258

int64_t TiffDirectory::compression() const { return compression_; }  // 259

int64_t TiffDirectory::photometric() const { return photoMetric_; }  // 262

int64_t TiffDirectory::jpegQuality() const { return jpegQuality_; }

int64_t TiffDirectory::jpegColorMode() const { return jpegColorMode_; }

int64_t TiffDirectory::jpegTableMode() const { return jpegTableMode_; }

int64_t TiffDirectory::jpegTableDataSize() const { return jpegTableDataSize_; }

uint8_t* TiffDirectory::jpegTableData() const { return jpegTableData_.get(); }

bool TiffDirectory::hasJpegTableData() const {
  return ((jpegTableData_ != nullptr) && (jpegTableDataSize_ > 0));
}

std::string TiffDirectory::imageDescription() const {  // 270
  return imageDescription_;
}

int64_t TiffDirectory::orientation() const { return orientation_; }   // 274

int64_t TiffDirectory::samplesPerPixel() const {
  return samplePerPixel_;  // 277
}

int64_t TiffDirectory::RowsPerStrip() const { return rowsPerStrip_; }  // 278

int64_t TiffDirectory::planarConfiguration() const {
  return planarConfig_;  // 284
}

int64_t TiffDirectory::tileWidth() const { return tileWidth_; }  // 322

int64_t TiffDirectory::tileHeight() const { return tileHeight_; }  // 323

int64_t TiffDirectory::tileDepth() const { return tileDepth_; }  // 32998

double TiffDirectory::xResolution() const { return xResolution_; }  // 282

double TiffDirectory::yResolution() const { return yResolution_; }   // 283

absl::string_view TiffDirectory::photoMetrIntStr() const {
  return photoMetricStr_.c_str();
}

int64_t TiffDirectory::tilesPerRow() const {
  if ((imageWidth_ == -1) || (tileWidth_ <= 0)) {
    return -1;
  }
  double tilesPerRow = static_cast<double>(imageWidth_) /
                       static_cast<double>(tileWidth_);
  return static_cast<int64_t>(std::ceil(tilesPerRow));
}


int64_t TiffDirectory::tilesPerColumn() const {
  if ((imageHeight_ == -1) || (tileHeight_ <= 0)) {
    return -1;
  }
  double tilesPerColumn = static_cast<double>(imageHeight_) /
                          static_cast<double>(tileHeight_);
  return static_cast<int64_t>(std::ceil(tilesPerColumn));
}

int64_t TiffDirectory::tileCount() const {
  return tileCount_;
}

bool TiffDirectory::isTiled() const {
  return isTiled_ && tileCount_ > 0;
}

bool TiffDirectory::isPyramidImage() const {
  return isTiled_ && subfileType_ == 0;
}

bool TiffDirectory::isThumbnailImage() const {
  return !isTiled_  && subfileType_ == 0;
}

bool TiffDirectory::isMacroImage() const {
  return !isTiled_ && subfileType_ == 0x9;
}

bool TiffDirectory::isLabelImage() const {
  return !isTiled_ && subfileType_ == 0x1;
}

bool TiffDirectory::isJpegCompressed() const {
  return (compression_  == COMPRESSION_JPEG);
}

bool TiffDirectory::isJpeg2kCompressed() const {
  // Aperio 33003: YCbCr format, possibly with a chroma subsampling of 4:2:2.
  const int compression_aperio_YCbCr = 33003;
  // Aperio 33005: RGB format
  const int compression_aperio_RGB = 33005;
  return (compression_  == COMPRESSION_JP2000 ||
          compression_  == compression_aperio_YCbCr ||
          compression_  == compression_aperio_RGB);
}

bool TiffDirectory::isPhotoMetricRGB() const {
  return (photoMetric_  == PHOTOMETRIC_RGB);
}

bool TiffDirectory::isPhotoMetricYCBCR() const {
  return (photoMetric_  == PHOTOMETRIC_YCBCR);
}

void TiffDirectory::log() const {
  BOOST_LOG_TRIVIAL(info) << "Tiff File Directory\n" <<
    "----------------------\n" <<
    " isJpegCompressed: " << isJpegCompressed() << "\n" <<
    " isJpeg2kCompressed: " << isJpeg2kCompressed() << "\n" <<
    " isPyramidImage: " << isPyramidImage() << "\n" <<
    " isPhotoMetricYCBCR: " << isPhotoMetricYCBCR() << "\n" <<
    " isPhotoMetricRGB: " << isPhotoMetricRGB() << "\n" <<
    " tileCount: " << tileCount() << "\n" <<
    " tileWidth: " << tileWidth() << "\n" <<
    " tileHeight: " << tileHeight() << "\n" <<
    " imageDepth: " << imageDepth() << "\n" <<
    " tilesPerRow: " << tilesPerRow() << "\n" <<
    " tilesPerColumn: " << tilesPerColumn() << "\n" <<
    " tileCount: " << tileCount() << "\n" <<
    " photoMetric_: " <<  photoMetric_ << "\n" <<
    "----------------------\n" <<
    " hasJpegTableData: " <<  hasJpegTableData() << "\n" <<
    " jpegTableDataSize: " <<  jpegTableDataSize() << "\n" <<
    " jpegTableMode: " <<  jpegTableMode() << "\n" <<
    " jpegColorMode: " <<  jpegColorMode() << "\n" <<
    " jpegQuality: " <<  jpegQuality() << "\n" <<
    "----------------------\n";
}

bool TiffDirectory::isExtractablePyramidImage() const {
  return ((isJpegCompressed() || isJpeg2kCompressed()) && isPyramidImage() &&
          (isPhotoMetricYCBCR() || isPhotoMetricRGB()) &&
          (tileCount() > 0) && (tileWidth() > 0) && (tileHeight() > 0) &&
          (imageWidth() > 0) && (imageHeight() > 0) &&
          ((imageDepth() == 1) ||
           (tilesPerRow() * tilesPerColumn() == tileCount())));
}

bool TiffDirectory::doImageDimensionsMatch(int64_t width,
                                           int64_t height) const {
  return (imageWidth_ == width && imageHeight_ == height);
}

bool TiffDirectory::isSet(int64_t val) const { return val != -1; }

bool TiffDirectory::isSet(double val) const { return val != -1.0; }

bool TiffDirectory::isSet(std::string val) const { return val != ""; }

void TiffDirectory::_getTiffField_ui32(TIFF *tiff, ttag_t tag,
                                                   int64_t *val) const {
  *val = -1;
  uint32_t normalint = 0;
  int result = TIFFGetField(tiff, tag, &normalint);
  if (result != 1) {
    *val = -1;
  } else {
      *val = static_cast<int64_t>(normalint);
  }
}

void TiffDirectory::_getTiffField_ui16(TIFF *tiff, ttag_t tag,
                                                   int64_t *val) const {
  *val = -1;
  uint16_t normalint = 0;
  int result = TIFFGetField(tiff, tag, &normalint);
  if (result != 1) {
    *val = -1;
  } else {
      *val = static_cast<int64_t>(normalint);
  }
}

void TiffDirectory::_getTiffField_f(TIFF *tiff, ttag_t tag,
                                                double *val) const {
  float flt;
  int result = TIFFGetField(tiff, tag, &flt);
  if (result != 1) {
    *val = -1.0;
  } else {
      *val = static_cast<double>(flt);
  }
}

void TiffDirectory::_getTiffField_str(TIFF *tiff, ttag_t tag,
                                      std::string *val) const {
  char *str;
  int result = TIFFGetField(tiff, tag, &str);
  if (result == 1) {
    *val = str;
  } else {
    *val = "";
  }
}

void TiffDirectory::_getTiffField_jpegTables(TIFF *tiff,
                                             int64_t* jpegTableDataSize,
                            std::unique_ptr<uint8_t[]> *jpegTableData) const {
  uint16_t size = 0;
  void *tableData;
  *jpegTableDataSize = -1;
  *jpegTableData = nullptr;
  int result = TIFFGetField(tiff, TIFFTAG_JPEGTABLES, &size, &tableData);
  if (result == 1) {
    *jpegTableDataSize = static_cast<int64_t>(size);
    if (*jpegTableDataSize > 0) {
      *jpegTableData = std::make_unique<uint8_t[]>(size);
      memcpy((*jpegTableData).get(), tableData, size);
    }
  }
}

bool TiffDirectory::_hasICCProfile(TIFF *tiff) const {
  uint32_t temp;
  void *ptr;
  return (1 == TIFFGetField(tiff, TIFFTAG_ICCPROFILE, &temp, &ptr));
}

}  // namespace wsiToDicomConverter
