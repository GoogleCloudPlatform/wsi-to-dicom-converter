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

#ifndef TESTS_TESTUTILS_H_
#define TESTS_TESTUTILS_H_
#include <dcmtk/dcmdata/dcdatset.h>
#include <dcmtk/dcmdata/dcdeftag.h>
#include <dcmtk/dcmdata/dcsequen.h>
#include <dcmtk/dcmdata/dcuid.h>
#include <cstdint>
#include <string>
#include "src/enums.h"

// Performs search of sub elemenet in DcmItem by DcmTagKey
// returns nullprt if tag is not present in sub elemenets
DcmElement* findElement(DcmItem* dataSet, const DcmTagKey& tag);

const char testPath[] = "../tests/";
const char tiffFileName[] = "../tests/CMU-1-Small-Region.svs";
#endif  // TESTS_TESTUTILS_H_
