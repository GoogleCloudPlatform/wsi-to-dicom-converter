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

#include <string>

namespace wsiToDicomConverter {

class TiffDirectory {
 public:
  explicit TiffDirectory(TIFF *tiff);
  virtual ~TiffDirectory();

  tdir_t directoryIndex();
                                 // comment # is tiff tag number
  bool hasICCProfile();         // 34675
  int64_t subfileType();         // 254
  int64_t imageWidth();          // 256
  int64_t imageHeight();         // 257
  int64_t imageDepth();          // 32997
  int64_t bitsPerSample();       // 258
  int64_t compression();         // 259
  int64_t photometric();         // 262
  std::string imageDescription();  // 270
  int64_t orientation();         // 274
  int64_t samplesPerPixel();     // 277
  int64_t RowsPerStrip();        // 278
  int64_t planarConfiguration();   // 284
  int64_t tileWidth();           // 322
  int64_t tileHeight();          // 323
  int64_t tileDepth();           // 32998
  double xResolution();          // 282
  double yResolution();          // 283

  int64_t tilesPerRow();
  int64_t tilesPerColumn();
  int64_t tileCount();

  bool isTiled();
  bool isPyramidImage();
  bool isThumbnailImage();
  bool isMacroImage();
  bool isLabelImage();
  bool isJpegCompressed();
  bool isPhotoMetricRGB();
  bool isPhotoMetricYCBCR();
  bool isExtractablePyramidImage();
  bool doImageDimensionsMatch(int64_t width, int64_t height);

  bool isSet(int64_t val);
  bool isSet(double val);
  bool isSet(std::string val);
  void log();

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

  void _getTiffField_ui32(TIFF *tiff, ttag_t tag, int64_t *val);
  void _getTiffField_ui16(TIFF *tiff, ttag_t tag, int64_t *val);
  void _getTiffField_str(TIFF *tiff, ttag_t tag, std::string *val);
  void _getTiffField_f(TIFF *tiff, ttag_t tag, double *val);
  bool _hasICCProfile(TIFF *tiff);
};

}  // namespace wsiToDicomConverter

#endif  // SRC_TIFFDIRECTORY_H_
