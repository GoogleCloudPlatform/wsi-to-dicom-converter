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

#include "src/openslideUtil.h"

namespace wsiToDicomConverter {

OpenSlidePtr::OpenSlidePtr(const char *filename) {
  _open(filename);
}

OpenSlidePtr::OpenSlidePtr(const std::string &filename) {
  _open(filename.c_str());
}

OpenSlidePtr::~OpenSlidePtr() {
  if (osr != nullptr) {
    openslide_close(osr);
  }
}

void OpenSlidePtr::_open(const char *filename) {
  osr = openslide_open(filename);
  if (osr == nullptr) {
    // Openslide did not intialize
    throw 1;
  }
  if (openslide_get_error(osr)) {
    BOOST_LOG_TRIVIAL(error) << openslide_get_error(osr);
    openslide_close(osr);
    throw 1;
  }
}

}  // namespace wsiToDicomConverter
