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

#ifndef SRC_TIFFFILE_H_
#define SRC_TIFFFILE_H_

#include <absl/strings/string_view.h>
#include <openslide.h>
#include <tiffio.h>

#include <memory>
#include <string>
#include <vector>

#include "src/openslideUtil.h"
#include "src/tiffDirectory.h"
#include "src/tiffTile.h"

namespace wsiToDicomConverter {

class TiffFile{
 public:
  explicit TiffFile(absl::string_view path, const int32_t dirIndex = 0);
  explicit TiffFile(const TiffFile &tf, const int32_t dirIndex);
  TiffFile(const TiffFile &tf) = delete;  // not implemented no copy constructor

  virtual ~TiffFile();
  bool isLoaded() const;
  bool isInitalized() const;
  bool hasExtractablePyramidImages() const;

  // Tiff Files and SVS Files can contain mulitiple images.
  // In Tiff terminology, images are stored in directories.
  // Image 1 = (dir 0), Image 2 = (dir 1), ..., Image N = (dir N - 1)
  // The properties of each image can different.
  //
  // On intitalization, the TiffFile class builds a list of the metadata
  // associated with each of the image directories.  This is stored in
  // the TiffDirectory vector.
  //
  // the getDirectoryIndexMatchingImageDimensions method
  // find the first image in the iage directory list which
  // has the dimensions (width and height) and optionally (true default)
  // also an isExtractablePyramidImage.
  // This the criteria for this setting filters images for
  // image types which the TiffFile can extract and embedded directly in
  // DICOM as encapsulated lossy jpeg. In the future the images
  // formats returned by this filter may be expaneded if the
  // TiffFrame image encapsulation is broadened to support
  // other image compression formats supported by svs (e.g.,
  // Lossless JPEG 20000).
  int32_t getDirectoryIndexMatchingImageDimensions(uint32_t width,
                                                   uint32_t height,
                                  bool isExtractablePyramidImage = true) const;
  const TiffDirectory *directory(int64_t dirIndex) const;
  const TiffDirectory *fileDirectory() const;
  uint32_t directoryCount() const;
  std::string path() const;
  std::unique_ptr<TiffTile> tile(uint32_t tileIndex);
  int32_t directoryLevel() const;

  // Openslide interfaces. Used to decode JPEG2000
  openslide_t * getOpenslidePtr();
  int32_t getOpenslideLevel() const;

  void close();

 private:
  TIFF * tiffFile_;
  const std::string tiffFilePath_;
  bool initalized_;
  std::vector<std::unique_ptr<TiffDirectory>> tiffDir_;
  const int32_t currentDirectoryIndex_;
  tsize_t tileReadBufSize_;

  std::unique_ptr<OpenSlidePtr> osptr_;
  int32_t openslide_level_;
};


}  // namespace wsiToDicomConverter

#endif  // SRC_TIFFFILE_H_
