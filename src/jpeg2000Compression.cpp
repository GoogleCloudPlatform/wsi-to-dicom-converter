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

#include "src/jpeg2000Compression.h"
#include <openjpeg.h>
#include <boost/gil/image.hpp>
#include <boost/log/trivial.hpp>
#include<algorithm>
#include <string>
#include <vector>

Jpeg2000Compression::~Jpeg2000Compression() {}

DCM_Compression Jpeg2000Compression::method() const {
  return JPEG2000;
}

std::string Jpeg2000Compression::toString() const {
  return std::string("lossless JPEG2000 compressed");
}


static void openjpeg_error(const char* msg, void* client_data) {
  BOOST_LOG_TRIVIAL(error) << "JPEG 2000 Error: " << msg;
}

static void openjpeg_warning(const char* msg, void* client_data) {
  BOOST_LOG_TRIVIAL(warning) << "JPEG 2000 Warning: " << msg;
}

static void openjpeg_info(const char* msg, void* client_data) {
  BOOST_LOG_TRIVIAL(info) << "JPEG 2000 Info: " << msg;
}

std::unique_ptr<uint8_t[]> Jpeg2000Compression::writeToMemory(
    unsigned int width, unsigned int height,
    uint8_t* buffer, size_t* size) {
  opj_cparameters_t parameters;
  opj_set_default_encoder_parameters(&parameters);
  parameters.cp_disto_alloc = 1;
  parameters.tcp_numlayers = 1;
  parameters.tcp_rates[0] = 0;
  parameters.cp_comment = const_cast<char*>("");
  // Image sizes below 2^(#resolutions-1) causes
  // error: Number of resolutions is too high in comparison to the smallest
  // image dimension.  See: https://groups.google.com/g/openjpeg/c/Lpw6Ydhf7bA
  unsigned int min_dim = std::min<unsigned int>(width, height);
  int max_numresolution = static_cast<int>(std::log2(
                                            static_cast<double>(min_dim))) + 1;
  max_numresolution = std::min<int>(parameters.numresolution,
                                    max_numresolution);
  if (max_numresolution != parameters.numresolution) {
    BOOST_LOG_TRIVIAL(warning) << "JPEG 2000: Image size is smaller than 2^("
                                  "numresolution - 1); Changing numresolution "
                                  "from: " << parameters.numresolution <<
                                  " to: " << max_numresolution <<
                                  " to meet encoder requirments.";
    parameters.numresolution = max_numresolution;
  }

  COLOR_SPACE colorspace;
  opj_image_cmptparm_t componentsParameters[3];

  for (size_t i = 0; i < 3; i++) {
    memset(&componentsParameters[i], 0, sizeof(opj_image_cmptparm_t));
    componentsParameters[i].dx = 1;
    componentsParameters[i].dy = 1;
    componentsParameters[i].x0 = 0;
    componentsParameters[i].y0 = 0;
    componentsParameters[i].w = width;
    componentsParameters[i].h = height;
    componentsParameters[i].prec = 8;
    componentsParameters[i].sgnd = 0;
  }

  opj_image_t* opjImage = opj_image_create(3, &componentsParameters[0],
                                           colorspace);
  opjImage->x0 = 0;
  opjImage->y0 = 0;
  opjImage->x1 = width;
  opjImage->y1 = height;

  int32_t* red = opjImage->comps[0].data;
  int32_t* green = opjImage->comps[1].data;
  int32_t* blue = opjImage->comps[2].data;

  const uint8_t* pixel = reinterpret_cast<const uint8_t*>(buffer);
  const unsigned int pixels_count = height*width*3;
  for (unsigned int y = 0; y < pixels_count; y+=3) {
    *red = pixel[y];
    *green = pixel[y+1];
    *blue = pixel[y+2];
    red++;
    green++;
    blue++;
  }

  opj_codec_t* cinfo = opj_create_compress(OPJ_CODEC_J2K);

  // Uncomment to log info, warnings, & errors
  //
  // opj_set_info_handler(cinfo, openjpeg_info, NULL);
  // opj_set_warning_handler(cinfo, openjpeg_warning, NULL);
  // opj_set_error_handler(cinfo, openjpeg_error, NULL);

  opj_setup_encoder(cinfo, &parameters, opjImage);

  opj_stream_t* cio;
  cio = opj_stream_default_create(0);
  opj_stream_set_user_data(cio, this, {});
  opj_stream_set_write_function(
      cio, [](void* buffer, OPJ_SIZE_T size, void* userData) {
        Jpeg2000Compression* jpeg2000Compression =
            reinterpret_cast<Jpeg2000Compression*>(userData);
        jpeg2000Compression->buffer_ = reinterpret_cast<uint8_t*>(buffer);
        jpeg2000Compression->size_ = size;
        return size;
      });

  bool result = opj_start_compress(cinfo, opjImage, cio);
  if (!result) {
    BOOST_LOG_TRIVIAL(error) << "JPEG 2000 Error starting compression";
  }
  opj_encode(cinfo, cio);
  opj_end_compress(cinfo, cio);
  std::unique_ptr<uint8_t[]> compressed = std::make_unique<uint8_t[]>(size_);
  memcpy(compressed.get(), buffer_, size_);

  opj_image_destroy(opjImage);
  opj_stream_destroy(cio);
  opj_destroy_codec(cinfo);
  *size = size_;
  return compressed;
}

std::unique_ptr<uint8_t[]> Jpeg2000Compression::compress(
    const boost::gil::rgb8_view_t& view, size_t* size) {
  std::unique_ptr<uint8_t[]> storage = getRawData(view, size);

  return this->writeToMemory(view.width(), view.height(), storage.get(), size);
}
