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

#include "rawCompression.h"
#include <boost/gil/image.hpp>
#include <vector>
using namespace std;
using namespace boost::gil;

struct PixelInserter {
  std::vector<uint8_t> *storage;
  PixelInserter(std::vector<uint8_t> *s) : storage(s) {}
  void operator()(boost::gil::rgb8_pixel_t p) const {
    using boost::gil::at_c;
    storage->push_back(at_c<0>(p));
    storage->push_back(at_c<1>(p));
    storage->push_back(at_c<2>(p));
  }
};

RawCompression::RawCompression() {}

void RawCompression::compress(uint8_t *&output,
                              const boost::gil::rgb8_view_t &view,
                              size_t &size) {
  getRawData(output, view, size);
}

void RawCompression::getRawData(uint8_t *&output,
                                const boost::gil::rgb8_view_t &view,
                                size_t &size) {
  vector<uint8_t> storage;
  storage.reserve(view.width() * view.height() *
                  boost::gil::num_channels<rgb8_image_t>());
  for_each_pixel(view, PixelInserter(&storage));
  output = new uint8_t[storage.size()];
  std::copy(storage.begin(), storage.end(), output);
  size = storage.size();
  storage.clear();
}
