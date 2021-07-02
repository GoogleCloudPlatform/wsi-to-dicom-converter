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

#include <iostream>
#include <string>
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
        "downsample for each level, downsample is size factor for each level")(
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
        "Stop generating image downsampels if image dimensions < "
        "frame dimensions.")
        ("bilinearDownsampling",
        programOptions::bool_switch(
        &useBilinearDownsampling)->default_value(false),
        "Use bilinear interpolation to downsample images instead of"
        " nearest neighbor interpolation.")
        ("floorCorrectOpenslideLevelDownsamples",
        programOptions::bool_switch(
        &floorCorrectDownsampling)->default_value(false),
        "Floor openslide level downsampling to improve pixel-to-pixel "
        "correspondance for level images which are dimensionally not perfect "
        "multiples. Example (40x 45,771x35,037) downsampled (16x) -> "
        "(2.5x 2,860 x 2,189)  openslide reported downsampling: 16.004892."
        " Floor correction = 16");
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
  wsiToDicomConverter::WsiRequest request;
  request.inputFile = inputFile;
  request.outputFileMask = outputFolder;
  request.frameSizeX = tileWidth;
  request.frameSizeY = tileHeight;
  request.compression = dcmCompressionFromString(compression);
  request.quality = 80;
  request.startOnLevel = start;
  request.stopOnLevel = stop;
  request.imageName = seriesDescription;
  request.studyId = studyId;
  request.seriesId = seriesId;
  request.jsonFile = jsonFile;
  request.retileLevels = levels;
  if (levels > 0 && levels+1 > downsamples.size()) {
    // fix buffer overun bug.  accessing meory outside of
    // downsample buffer when retileing.
    downsamples.resize(levels+1, 0);
  }
  request.downsamples = &downsamples[0];
  request.tiled = !sparse;
  request.batchLimit = batch;
  request.threads = threads;
  request.dropFirstRowAndColumn = dropFirstRowAndColumn;
  request.stopDownsamplingAtSingleFrame = stopDownsamplingAtSingleFrame;
  request.useBilinearDownsampling = useBilinearDownsampling;
  request.floorCorrectDownsampling = floorCorrectDownsampling;
  request.debug = debug;
  return wsiToDicomConverter::WsiToDcm::wsi2dcm(request);
}
