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
#include <boost/program_options.hpp>

#include <algorithm>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include "src/wsiToDcm.h"
namespace {
const size_t ERROR_IN_COMMAND_LINE = 1;
const size_t SUCCESS = 0;
const size_t ERROR_UNHANDLED_EXCEPTION = 2;

}  // namespace
int main(int argc, char *argv[]) {
  std::string inputFile;
  std::string jsonFile;
  std::string outputFolder;
  std::string compression;
  std::string seriesDescription;
  std::string seriesId;
  std::string studyId;
  int tileHeight;
  int tileWidth;
  int levels;
  int start;
  int stop;
  int batch;
  int threads;
  bool debug;
  bool dropFirstRowAndColumn;
  bool stopDownsamplingAtSingleFrame;
  bool useBilinearDownsampling;
  bool floorCorrectDownsampling;
  bool preferProgressiveDownsampling;
  bool cropFrameToGenerateUniformPixelSpacing;
  int compressionQuality;
  std::vector<int> downsamples;
  downsamples.resize(1, 0);
  bool sparse;
  try {
    namespace programOptions = boost::program_options;
    programOptions::options_description desc("Options", 90, 20);
    desc.add_options()("help", "Print help messages")(
        "input", programOptions::value<std::string>(&inputFile)->required(),
        "input file")("outFolder",
                      programOptions::value<std::string>(&outputFolder)
                          ->required()
                          ->default_value("./"),
                      "folder to store dcm files")(
        "tileHeight",
        programOptions::value<int>(&tileHeight)->required()->default_value(500),
        "tile height px")(
        "tileWidth",
        programOptions::value<int>(&tileWidth)->required()->default_value(500),
        "tile width px")("levels",
                         programOptions::value<int>(&levels)->default_value(0),
                         "number of levels, levels == 0 means number of levels "
                         "will be readed from wsi file")(
        "downsamples",
        programOptions::value<std::vector<int> >(&downsamples)->multitoken(),
        "downsample for each level, downsample is size factor for each level."
        " If used with progressiveDownsample, downsamples must be ordered from "
        "low downsampling to high downsampling (e.g., 1 2 4)")(
        "startOn", programOptions::value<int>(&start)->default_value(0),
        "level to start")("stopOn",
                          programOptions::value<int>(&stop)->default_value(-1),
                          "level to stop")(
        "sparse", programOptions::bool_switch(&sparse)->default_value(false),
        "use TILED_SPARSE frame organization, by default it's TILED_FULL")(
        "compression",
        programOptions::value<std::string>(&compression)->default_value("jpeg"),
        "compression, supported compressions: jpeg, jpeg2000, raw")(
        "seriesDescription",
        programOptions::value<std::string>(&seriesDescription)->required(),
        "series description in SeriesDescription tag")(
        "studyId",
        programOptions::value<std::string>(&studyId)->required()->default_value(
            ""),
        "StudyID")("seriesId",
                   programOptions::value<std::string>(&seriesId)
                       ->required()
                       ->default_value(""),
                   "SeriesID")("jsonFile",
                               programOptions::value<std::string>(&jsonFile),
                               "dicom json file with additional tags "
        "\nhttps://www.dicomstandard.org/dicomweb/dicom-json-format/")(
        "batch", programOptions::value<int>(&batch)->default_value(0),
        "maximum frames in one file")(
        "threads",
        programOptions::value<int>(&threads)->required()->default_value(-1),
        "number of threads")(
        "debug", programOptions::bool_switch(&debug)->default_value(false),
        "debug messages")(
        "dropFirstRowAndColumn",
        programOptions::bool_switch(
        &dropFirstRowAndColumn)->default_value(false),
        "drop first row and column of the source image in order to "
        "workaround bug\nhttps://github.com/openslide/openslide/issues/268")
        ("stopDownsamplingAtSingleFrame",
        programOptions::bool_switch(
        &stopDownsamplingAtSingleFrame)->default_value(false),
        "Stop image downsampling if image dimensions < "
        "frame dimensions.")
        ("bilinearDownsampling",
        programOptions::bool_switch(
        &useBilinearDownsampling)->default_value(false),
        "Use bilinear interpolation to downsample images instead of"
        " nearest neighbor interpolation to improve downsample image quality.")
        ("floorCorrectOpenslideLevelDownsamples",
        programOptions::bool_switch(
        &floorCorrectDownsampling)->default_value(false),
        "Floor openslide level downsampling to improve pixel-to-pixel "
        "correspondance for level images which are dimensionally not perfect "
        "multiples. Example (40x 45,771x35,037) downsampled (16x) -> "
        "(2.5x 2,860 x 2,189)  openslide reported downsampling: 16.004892."
        " Floor correction = 16")
        ("progressiveDownsample",
        programOptions::bool_switch(
        &preferProgressiveDownsampling)->default_value(false),
        "Preferentially generate downsampled images progressively from "
        "prior downsampled images. Faster, increased memory requirment, and "
        "avoids rounding bug in openslide api which can cause pixel level "
        "alignemnt issues when generating output from images captured at "
        "multiple downsampling levels. To use images must be generated from "
        "highest to lowest magnification.")
        ("compressionQuality",
        programOptions::value<int>(
        &compressionQuality)->default_value(80),
        "Compression quality range(0 - 100")
        ("uniformPixelSpacing",
        programOptions::bool_switch(
        &cropFrameToGenerateUniformPixelSpacing)->default_value(false),
        "Crop imaging to generate downsampled images with unifrom pixel "
        "spacing. Not compatiable with dropFirstRowAndColumn. Recommended, "
        "use uniformPixelSpacing in combination with both "
        "--stopDownsamplingAtSingleFrame --progressiveDownsample");
    programOptions::positional_options_description positionalOptions;
    positionalOptions.add("input", 1);
    positionalOptions.add("outFolder", 1);
    programOptions::variables_map vm;
    try {
      programOptions::store(programOptions::command_line_parser(argc, argv)
                                .options(desc)
                                .positional(positionalOptions)
                                .run(),
                            vm);

      if (vm.count("help")) {
        std::cout << "Wsi2dcm" << std::endl << desc << std::endl;
        return SUCCESS;
      }
      programOptions::notify(vm);
    } catch (programOptions::error &e) {
      std::cerr << "ERROR: " << e.what() << std::endl << std::endl;
      std::cerr << desc << std::endl;
      return ERROR_IN_COMMAND_LINE;
    }
  } catch (std::exception &exception) {
    std::cerr << "Unhandled Exception reached the top of main: "
              << exception.what() << ", application will now exit" << std::endl;
    return ERROR_UNHANDLED_EXCEPTION;
  }
  if (cropFrameToGenerateUniformPixelSpacing && dropFirstRowAndColumn) {
     std::cerr << "Options: uniformPixelSpacing and dropFirstRowAndColumn are"
              << " not compatible." << std::endl;
      return 1;
  }
  wsiToDicomConverter::WsiRequest request;
  request.inputFile = inputFile;
  request.outputFileMask = outputFolder;
  request.frameSizeX = std::max(tileWidth, 1);
  request.frameSizeY = std::max(tileHeight, 1);
  request.compression = dcmCompressionFromString(compression);
  request.quality = std::max(std::min(100, compressionQuality), 0);
  request.startOnLevel = std::max(start, 0);
  request.stopOnLevel =  std::max(stop, -1);
  request.imageName = seriesDescription;
  request.studyId = studyId;
  request.seriesId = seriesId;
  request.jsonFile = jsonFile;
  request.retileLevels = std::max(levels, 0);
  if (levels > 0 && levels+1 > downsamples.size()) {
    // fix buffer overun bug.  accessing meory outside of
    // downsample buffer when retileing.
    downsamples.resize(levels+1, 0);
  }
  for (size_t idx = 0; idx < downsamples.size(); ++idx) {
    downsamples[idx] = std::max(downsamples[idx], 0);
  }
  request.downsamples = std::move(downsamples);
  request.tiled = !sparse;
  request.batchLimit = std::max(batch, 0);
  request.threads = std::max(threads, -1);
  request.dropFirstRowAndColumn = dropFirstRowAndColumn;
  request.stopDownsamplingAtSingleFrame = stopDownsamplingAtSingleFrame;
  request.useBilinearDownsampling = useBilinearDownsampling;
  request.floorCorrectDownsampling = floorCorrectDownsampling;
  request.preferProgressiveDownsampling = preferProgressiveDownsampling;
  request.cropFrameToGenerateUniformPixelSpacing =
                                      cropFrameToGenerateUniformPixelSpacing;
  request.debug = debug;
  wsiToDicomConverter::WsiToDcm converter(&request);
  return converter.wsi2dcm();
}
