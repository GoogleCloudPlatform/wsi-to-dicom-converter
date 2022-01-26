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
#include <tiffio.h>

#include <memory>
#include <string>
#include <vector>

#include "src/tiffDirectory.h"
#include "src/tiffTile.h"

namespace wsiToDicomConverter {

class TiffFile{
 public:
  explicit TiffFile(absl::string_view path, const int32_t dirIndex = 0);
  explicit TiffFile(const TiffFile &tf, const int32_t dirIndex);
  TiffFile(const TiffFile &tf);  // not implemented no copy constructor

  virtual ~TiffFile();
  bool isLoaded() const;
  bool isInitalized() const;
  bool hasExtractablePyramidImages() const;

  int32_t getDirectoryIndexMatchingImageDimensions(uint32_t width,
                                                   uint32_t height,
                                  bool isExtractablePyramidImage = true) const;
  const TiffDirectory *directory(int64_t dirIndex) const;
  const TiffDirectory *fileDirectory() const;
  uint32_t directoryCount() const;
  std::string path() const;
  std::unique_ptr<TiffTile> tile(uint32_t tileIndex);
  int32_t directoryLevel() const;
  void close();

 private:
  TIFF * tiffFile_;
  const std::string tiffFilePath_;
  bool initalized_;
  std::vector<std::unique_ptr<TiffDirectory>> tiffDir_;
  const int32_t currentDirectoryIndex_;
  tsize_t tileReadBufSize_;
};


}  // namespace wsiToDicomConverter

#endif  // SRC_TIFFFILE_H_
