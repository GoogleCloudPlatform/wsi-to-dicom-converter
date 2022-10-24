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
  std::string downsamplingAlgorithm;
  std::string firstlevelCompression;
  std::string jpegSubsampling;
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
  bool floorCorrectDownsampling;
  bool preferProgressiveDownsampling;
  bool SVSImportPreferScannerTileingForLargestLevel;
  bool SVSImportPreferScannerTileingForAllLevels;
  bool readDICOM;
  int compressionQuality;
  bool readUntiledImage;
  double untiledImageHeightMM;
  std::vector<int> downsamples;
  bool sparse;
  bool includeSingleFrameDownsample;
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
        "firstLevelCompression",
        programOptions::value<std::string>(&firstlevelCompression)
            ->default_value("default"),
        "compression, supported compressions: jpeg, jpeg2000, raw")
        (
        "seriesDescription",
        programOptions::value<std::string>(&seriesDescription)->
                                                      default_value(""),
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
        ("jpegCompressionQuality",
        programOptions::value<int>(
        &compressionQuality)->default_value(80),
        "Compression quality range(0 - 100")
        ("opencvDownsampling",
        programOptions::value<std::string>(
        &downsamplingAlgorithm)->default_value("NONE"),
        "OpenCV downsampling algorithm, supported: LANCZOS4, CUBIC, AREA, "
        "LINEAR, LINEAR_EXACT, NEAREST, NEAREST_EXACT. Default value "
        "'NONE' uses non-opencv boost::gli nearestneighbor downsampling.")
        ("SVSImportPreferScannerTileingForLargestLevel",
        programOptions::bool_switch(
        &SVSImportPreferScannerTileingForLargestLevel)->default_value(false),
        "Optimization for DICOM generation from jpeg encoded SVS, generates "
        " highest magification DICOM using svs encoded jpeg images "
        "directly without decompression to avoid recompression artifacts."
        " First slice compression and tile dimensions command line parameters "
        " apply only DICOM generation from non-jpeg "
        " encoded svs imaging and non-svs imaging. Does not support "
        "re-tiling of jpeg encoded svs.  Not compatiable with "
        "row/column-dropping or cropping commandline settings options.")
        ("SVSImportPreferScannerTileingForAllLevels",
        programOptions::bool_switch(
        &SVSImportPreferScannerTileingForAllLevels)->default_value(false),
        "Optimization for jpeg encoded SVS. Use jpeg images encoded in SVS at "
        "all levels preferentially. Same limitations as "
        "SVSImportPreferScannerTileingForLargestLevel. Compression settings "
        "apply to generated levels only.")
        ("readDICOM",
        programOptions::bool_switch(&readDICOM)->default_value(false),
        "Generate DICOM Pyramid from Dicom file (level 0). "
        "Output starts at Level 1.")
        ("readImage",
        programOptions::bool_switch(&readUntiledImage)->default_value(false),
        "Generate DICOM Pyramid from untiled image.")
        ("untiledImageHeightMM",
        programOptions::value<double>(&untiledImageHeightMM)->default_value(
          0.0), "Height in mm of untiled image (assumed square pixel"
                " aspect ratio).")
       ("singleFrameDownsample",
        programOptions::bool_switch(&includeSingleFrameDownsample)->
        default_value(false), "Force downsampling to include at least one "
        "single frame downsample.")
        ("jpegSubsampling",
        programOptions::value<std::string>(&jpegSubsampling)->
        default_value("420"), "JPEG subsampling for Y component, supported: "
        "444(best-quality), 440, 442, 420(most-compressed).");
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
  if (dropFirstRowAndColumn &&
     (SVSImportPreferScannerTileingForLargestLevel ||
      SVSImportPreferScannerTileingForAllLevels)) {
     std::cerr << "Options: dropFirstRowAndColumn is"
              << " not compatible with Options: " <<
              "SVSImportPreferScannerTileingForLargestLevel and " <<
              "SVSImportPreferScannerTileingForAllLevels." << std::endl;
      return ERROR_IN_COMMAND_LINE;
  }
  if (readDICOM & !preferProgressiveDownsampling) {
    std::cerr << "Generating WSI Pyramids from DICOM requires enabling "
                 "progressive downsampling." << std::endl;
    return ERROR_IN_COMMAND_LINE;
  }
  if (readUntiledImage & !preferProgressiveDownsampling) {
    std::cerr << "Generating WSI Pyramids from un-tiled images requires "
                 "enabling progressive downsampling." << std::endl;
    return ERROR_IN_COMMAND_LINE;
  }
  if (readUntiledImage && readDICOM) {
    std::cerr << "Invalid configuration cannot use both "
                 "readUntiledImage and readDICOM" << std::endl;
    return ERROR_IN_COMMAND_LINE;
  }
  wsiToDicomConverter::WsiRequest request;
  request.genPyramidFromUntiledImage = readUntiledImage;
  request.untiledImageHeightMM = untiledImageHeightMM;
  request.genPyramidFromDicom = readDICOM;
  request.inputFile = inputFile;
  request.outputFileMask = outputFolder;
  request.frameSizeX = std::max(tileWidth, 1);
  request.frameSizeY = std::max(tileHeight, 1);
  request.firstlevelCompression = (firstlevelCompression == "default") ?
                               dcmCompressionFromString(compression) :
                               dcmCompressionFromString(firstlevelCompression);
  request.compression = dcmCompressionFromString(compression);
  request.quality = std::max(std::min(100, compressionQuality), 0);
  request.startOnLevel = request.genPyramidFromDicom ? std::max(start, 1) :
                                                       std::max(start, 0);
  request.stopOnLevel =  std::max(stop, -1);
  request.imageName = seriesDescription;
  request.studyId = studyId;
  request.seriesId = seriesId;
  request.jsonFile = jsonFile;
  request.retileLevels = std::max(levels, 0);
  request.includeSingleFrameDownsample = includeSingleFrameDownsample;
  for (size_t idx = 0; idx < downsamples.size(); ++idx) {
    downsamples[idx] = std::max(downsamples[idx], 0);
  }
  request.downsamples = std::move(downsamples);
  request.tiled = !sparse;
  request.batchLimit = std::max(batch, 0);
  request.threads = std::max(threads, -1);
  request.dropFirstRowAndColumn = dropFirstRowAndColumn;
  request.stopDownsamplingAtSingleFrame = stopDownsamplingAtSingleFrame;
  request.floorCorrectDownsampling = floorCorrectDownsampling;
  if (request.genPyramidFromDicom || request.genPyramidFromUntiledImage) {
    request.preferProgressiveDownsampling = true;
  } else {
    request.preferProgressiveDownsampling = preferProgressiveDownsampling;
  }
  request.SVSImportPreferScannerTileingForLargestLevel =
          SVSImportPreferScannerTileingForLargestLevel;
  request.SVSImportPreferScannerTileingForAllLevels =
          SVSImportPreferScannerTileingForAllLevels;
  request.useOpenCVDownsampling = true;
  if (downsamplingAlgorithm == "LANCZOS4") {
    request.openCVInterpolationMethod = cv::INTER_LANCZOS4;
  } else if (downsamplingAlgorithm == "CUBIC") {
    request.openCVInterpolationMethod = cv::INTER_CUBIC;
  } else if (downsamplingAlgorithm == "AREA") {
    request.openCVInterpolationMethod = cv::INTER_AREA;
  } else if (downsamplingAlgorithm == "LINEAR") {
    request.openCVInterpolationMethod = cv::INTER_LINEAR;
  } else if (downsamplingAlgorithm == "LINEAR_EXACT") {
    request.openCVInterpolationMethod = cv::INTER_LINEAR_EXACT;
  } else if (downsamplingAlgorithm == "NEAREST") {
    request.openCVInterpolationMethod = cv::INTER_NEAREST;
  } else if (downsamplingAlgorithm == "NEAREST_EXACT") {
    request.openCVInterpolationMethod = cv::INTER_NEAREST_EXACT;
  } else if (downsamplingAlgorithm == "NONE") {
    request.openCVInterpolationMethod = cv::INTER_LANCZOS4;
    request.useOpenCVDownsampling = false;
  } else {
    std::cerr << "Unrecognized OpenCVDownsamplingAlgorithm: " <<
                 downsamplingAlgorithm;
    return 1;
  }
  request.debug = debug;

  if (jpegSubsampling == "444") {
    request.jpegSubsampling = subsample_444;
  } else if (jpegSubsampling == "440") {
    request.jpegSubsampling = subsample_440;
  } else if (jpegSubsampling == "422") {
    request.jpegSubsampling = subsample_422;
  } else if (jpegSubsampling == "420") {
    request.jpegSubsampling = subsample_420;
  } else {
    std::cerr << "Unrecognized jpegSubsampling: " <<
                 jpegSubsampling;
    return 1;
  }
  wsiToDicomConverter::WsiToDcm converter(&request);
  return converter.wsi2dcm();
}
