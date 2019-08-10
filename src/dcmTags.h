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

#ifndef SRC_DCMTAGS_H_
#define SRC_DCMTAGS_H_
#include <dcmtk/dcmdata/dcdatset.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcsequen.h>
#include <string>

// Parses Json file with DICOM tags and populates DcmDataset with it
class DcmTags {
 public:
  DcmTags();
  // Reads json data from file
  void readJsonFile(std::string fileName);
  // Reads json data inputstream
  void readInputStream(std::istream* inputStream);
  // Populates dataset with elements parsed from json
  void populateDataset(DcmDataset* dataset);

 private:
  DcmItem dataset_;
};

#endif  // SRC_DCMTAGS_H_
