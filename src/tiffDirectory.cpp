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
  tileCount_ = TIFFNumberOfTiles(tiff);  // tiles_x * tiles_y * tiles_z
  isTiled_ = TIFFIsTiled(tiff);
}

TiffDirectory::~TiffDirectory() {}

tdir_t TiffDirectory::directoryIndex() { return directoryIndex_; }

bool TiffDirectory::hasICCProfile() { return hasIccProfile_; }  // 34675

int64_t TiffDirectory::subfileType() { return subfileType_; }    // 254

int64_t TiffDirectory::imageWidth()  { return imageWidth_; }     // 256

int64_t TiffDirectory::imageHeight() { return imageHeight_; }    // 257

int64_t TiffDirectory::imageDepth()  { return imageDepth_; }     // 32997

int64_t TiffDirectory::bitsPerSample() { return bitsPerSample_; }  // 258

int64_t TiffDirectory::compression() { return compression_; }  // 259

int64_t TiffDirectory::photometric() { return photoMetric_; }  // 262

std::string TiffDirectory::imageDescription() {  // 270
  return imageDescription_;
}

int64_t TiffDirectory::orientation() { return orientation_; }   // 274

int64_t TiffDirectory::samplesPerPixel() { return samplePerPixel_; }  // 277

int64_t TiffDirectory::RowsPerStrip() { return rowsPerStrip_; }  // 278

int64_t TiffDirectory::planarConfiguration() { return planarConfig_; }  // 284

int64_t TiffDirectory::tileWidth() { return tileWidth_; }  // 322

int64_t TiffDirectory::tileHeight()  { return tileHeight_; }  // 323

int64_t TiffDirectory::tileDepth()  { return tileDepth_; }   // 32998

double TiffDirectory::xResolution()  { return xResolution_; }  // 282

double TiffDirectory::yResolution()  { return yResolution_; }   // 283

int64_t TiffDirectory::tilesPerRow() {
  if ((imageWidth_ == -1) || (tileWidth_ <= 0)) {
    return -1;
  }
  double tilesPerRow = static_cast<double>(imageWidth_) /
                       static_cast<double>(tileWidth_);
  return static_cast<int64_t>(std::ceil(tilesPerRow));
}


int64_t TiffDirectory::tilesPerColumn()  {
  if ((imageHeight_ == -1) || (tileHeight_ <= 0)) {
    return -1;
  }
  double tilesPerColumn = static_cast<double>(imageHeight_) /
                          static_cast<double>(tileHeight_);
  return static_cast<int64_t>(std::ceil(tilesPerColumn));
}

int64_t TiffDirectory::tileCount() {
  return tileCount_;
}

bool TiffDirectory::isTiled() {
  return isTiled_ && tileCount_ > 0;
}

bool TiffDirectory::isPyramidImage() {
  return isTiled_ && subfileType_ == 0;
}

bool TiffDirectory::isThumbnailImage() {
  return !isTiled_  && subfileType_ == 0;
}

bool TiffDirectory::isMacroImage() {
  return !isTiled_ && subfileType_ == 0x9;
}

bool TiffDirectory::isLabelImage() {
  return !isTiled_ && subfileType_ == 0x1;
}

bool TiffDirectory::isJpegCompressed() {
  return (compression_  == COMPRESSION_JPEG);
}

bool TiffDirectory::isPhotoMetricRGB() {
  return (photoMetric_  == PHOTOMETRIC_RGB);
}

bool TiffDirectory::isPhotoMetricYCBCR() {
  return (photoMetric_  == PHOTOMETRIC_YCBCR);
}

void TiffDirectory::log() {
  BOOST_LOG_TRIVIAL(info) << "Tiff File Directory\n" <<
    "----------------------\n" <<
    " isJpegCompressed: " << isJpegCompressed() << "\n" <<
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
    "----------------------\n";
}

bool TiffDirectory::isExtractablePyramidImage() {
  return (isJpegCompressed() && isPyramidImage() &&
          (isPhotoMetricYCBCR() || isPhotoMetricRGB()) &&
          (tileCount() > 0) && (tileWidth() > 0) && (tileHeight() > 0) &&
          (imageWidth() > 0) && (imageHeight() > 0) &&
          ((imageDepth() == 1) ||
           (tilesPerRow() * tilesPerColumn() == tileCount())));
}

bool TiffDirectory::doImageDimensionsMatch(int64_t width, int64_t height) {
  return (imageWidth_ == width && imageHeight_ == height);
}

bool TiffDirectory::isSet (int64_t val) { return val != -1; }

bool TiffDirectory::isSet (double val) { return val != -1.0; }

bool TiffDirectory::isSet (std::string val) { return val != ""; }

void TiffDirectory::_getTiffField_ui32(TIFF *tiff, ttag_t tag, int64_t *val) {
  *val = -1;
  uint32_t normalint = 0;
  int result = TIFFGetField(tiff, tag, &normalint);
  if (result != 1) {
    *val = -1;
  } else {
      *val = static_cast<int64_t>(normalint);
  }
}

void TiffDirectory::_getTiffField_ui16(TIFF *tiff, ttag_t tag, int64_t *val) {
  *val = -1;
  uint16_t normalint = 0;
  int result = TIFFGetField(tiff, tag, &normalint);
  if (result != 1) {
    *val = -1;
  } else {
      *val = static_cast<int64_t>(normalint);
  }
}

void TiffDirectory::_getTiffField_f(TIFF *tiff, ttag_t tag, double *val) {
  float flt;
  int result = TIFFGetField(tiff, tag, &flt);
  if (result != 1) {
    *val = -1.0;
  } else {
      *val = static_cast<double>(flt);
  }
}

void TiffDirectory::_getTiffField_str(TIFF *tiff, ttag_t tag,
                                      std::string *val) {
  char *str;
  int result = TIFFGetField(tiff, tag, &str);
  if (result == 1) {
    *val = str;
  } else {
    *val = "";
  }
}

bool TiffDirectory::_hasICCProfile(TIFF *tiff) {
  uint32_t temp;
  void *ptr;
  return (1 == TIFFGetField(tiff, TIFFTAG_ICCPROFILE, &temp, &ptr));
}

}  // namespace wsiToDicomConverter
