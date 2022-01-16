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

#include <boost/thread/mutex.hpp>
#include <tiffio.h>

#include <memory>
#include <string>
#include <vector>

#include "src/tiffDirectory.h"
#include "src/tiffTile.h"

namespace wsiToDicomConverter {

class TiffFile{
 public:
  explicit TiffFile(const std::string &path);
  virtual ~TiffFile();
  bool isLoaded();
  bool hasExtractablePyramidImages();

  int32_t getDirectoryIndexMatchingImageDimensions(uint32_t width,
                                                   uint32_t height,
                                        bool isExtractablePyramidImage = true);
  TiffDirectory *directory(int64_t dirIndex);
  uint32_t directoryCount();
  std::unique_ptr<TiffTile> tile(int32_t dirIndex, uint32_t tileIndex);

 private:
  TIFF * tiffFile_;
  std::string tiffFilePath_;
  std::vector<std::unique_ptr<TiffDirectory>> tiffDir_;
  boost::mutex tiffReadMutex_;

  uint32_t currentDirectoryIndex_;
  tsize_t tileReadBufSize_;
  tdata_t tileReadBuffer_;
};


}  // namespace wsiToDicomConverter

#endif  // SRC_TIFFFILE_H_
