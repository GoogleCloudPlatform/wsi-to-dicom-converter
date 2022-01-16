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

#ifndef SRC_TIFFDIRECTORY_H_
#define SRC_TIFFDIRECTORY_H_

#include <boost/thread/mutex.hpp>
#include <tiffio.h>

#include <memory>
#include <string>

namespace wsiToDicomConverter {

class TiffDirectory {
 public:
  explicit TiffDirectory(TIFF *tiff);
  virtual ~TiffDirectory();

  tdir_t directoryIndex() const;
                                 // comment # is tiff tag number
  bool hasICCProfile() const;         // 34675
  int64_t subfileType() const;         // 254
  int64_t imageWidth() const;          // 256
  int64_t imageHeight() const;         // 257
  int64_t imageDepth() const;          // 32997
  int64_t bitsPerSample() const;       // 258
  int64_t compression() const;         // 259
  int64_t photometric() const;         // 262
  std::string imageDescription() const;  // 270
  int64_t orientation() const;         // 274
  int64_t samplesPerPixel() const;     // 277
  int64_t RowsPerStrip() const;        // 278
  int64_t planarConfiguration() const;   // 284
  int64_t tileWidth() const;           // 322
  int64_t tileHeight() const;          // 323
  int64_t tileDepth() const;           // 32998
  double xResolution() const;          // 282
  double yResolution() const;          // 283

  int64_t jpegQuality() const;
  int64_t jpegColorMode() const;
  int64_t jpegTableMode() const;
  int64_t jpegTableDataSize() const;
  uint8_t* jpegTableData() const;
  bool hasJpegTableData() const;

  int64_t tilesPerRow() const;
  int64_t tilesPerColumn() const;
  int64_t tileCount() const;

  bool isTiled() const;
  bool isPyramidImage() const;
  bool isThumbnailImage() const;
  bool isMacroImage() const;
  bool isLabelImage() const;
  bool isJpegCompressed() const;
  bool isPhotoMetricRGB() const;
  bool isPhotoMetricYCBCR() const;
  bool isExtractablePyramidImage() const;
  bool doImageDimensionsMatch(int64_t width, int64_t height) const;

  bool isSet(int64_t val) const;
  bool isSet(double val) const;
  bool isSet(std::string val) const;
  void log() const;

 private:
  tdir_t directoryIndex_;
  int64_t subfileType_;              // 254
  int64_t imageWidth_, imageHeight_;   // 256, 257
  int64_t bitsPerSample_;            // 258 16bit
  int64_t compression_;              // 259 16bit
  int64_t photoMetric_;              // 262 16bit
  std::string imageDescription_;     // 270
  int64_t orientation_;              // 274 16bit
  int64_t samplePerPixel_;           // 277 16bit
  int64_t rowsPerStrip_;             // 278
  int64_t planarConfig_;             // 284  16bit
  int64_t tileWidth_, tileHeight_;   // 322, 323
  int64_t imageDepth_;               // 32997
  int64_t tileDepth_;                // 32998
  bool hasIccProfile_;               // 34675
  double  xResolution_, yResolution_;  // 282, 283
  int64_t tileCount_;
  bool isTiled_;
  int64_t jpegTableDataSize_;
  std::unique_ptr<uint8_t[]> jpegTableData_;
  int64_t jpegQuality_;
  int64_t jpegColorMode_;
  int64_t jpegTableMode_;

  void _getTiffField_ui32(TIFF *tiff, ttag_t tag, int64_t *val) const;
  void _getTiffField_ui16(TIFF *tiff, ttag_t tag, int64_t *val) const;
  void _getTiffField_str(TIFF *tiff, ttag_t tag, std::string *val) const;
  void _getTiffField_f(TIFF *tiff, ttag_t tag, double *val) const;
  bool _hasICCProfile(TIFF *tiff) const;
  void _getTiffField_jpegTables(TIFF *tiff, int64_t *jpegTableDataSize,
                              std::unique_ptr<uint8_t[]> *jpegTableData) const;
};

}  // namespace wsiToDicomConverter

#endif  // SRC_TIFFDIRECTORY_H_
