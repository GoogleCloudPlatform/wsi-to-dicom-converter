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

#include "src/dcmTags.h"
#include <dcmtk/dcmdata/dcuid.h>
#include <dcmtk/dcmdata/dcvrat.h>
#include <gtest/gtest.h>
#include <string>
#include "tests/testUtils.h"

TEST(readJson, singleTag) {
  DcmTags tags;
  std::string mediaStorage = "sopcalssUID";
  std::stringstream jsonStream;

  jsonStream << "{\"00020002\":{\"vr\":\"UI\",\"Value\":[\"" + mediaStorage +
                    "\"]}}";

  tags.readInputStream(&jsonStream);
  DcmDataset dataset;
  tags.populateDataset(&dataset);
  char* stringValue;
  findElement(&dataset, DCM_MediaStorageSOPClassUID)->getString(stringValue);
  ASSERT_EQ(mediaStorage, std::string(stringValue));
}

TEST(readJson, sequnceTag) {
  DcmTags tags;
  std::string dimension = "OrganizationUID";
  std::stringstream jsonStream;

  jsonStream << "{\"00209221\":{\"vr\":\"SQ\",\"Value\":[{\"00209164\":{\"vr\":"
                "\"UI\",\"Value\":[\"" +
                    dimension + "\"]}}]}}";

  tags.readInputStream(&jsonStream);
  DcmDataset dataset;
  tags.populateDataset(&dataset);
  DcmSequenceOfItems* dimensionOrganization =
      reinterpret_cast<DcmSequenceOfItems*>(
          findElement(&dataset, DCM_DimensionOrganizationSequence));

  ASSERT_NE(nullptr, dimensionOrganization);
  char* stringValue;
  findElement(dimensionOrganization->getItem(0), DCM_DimensionOrganizationUID)
      ->getString(stringValue);
  ASSERT_EQ(dimension, std::string(stringValue));
}

TEST(readJson, attributeTag) {
  DcmTags tags;
  std::stringstream jsonStream;
  jsonStream << "{\"00209165\":{\"vr\":\"AT\",\"Value\":[\"0048021E\"]}}";
  tags.readInputStream(&jsonStream);
  DcmDataset dataset;
  tags.populateDataset(&dataset);
  DcmAttributeTag* dimensionOrganization = reinterpret_cast<DcmAttributeTag*>(
      findElement(&dataset, DCM_DimensionIndexPointer));
  ASSERT_NE(nullptr, dimensionOrganization);
  DcmTagKey tagValue;
  dimensionOrganization->getTagVal(tagValue);
  ASSERT_EQ(DCM_ColumnPositionInTotalImagePixelMatrix, tagValue);
}

TEST(readJson, incorrectJson) {
  DcmTags tags;
  std::stringstream jsonStream;
  jsonStream << "}";
  tags.readInputStream(&jsonStream);
  DcmDataset dataset;
  tags.populateDataset(&dataset);
  ASSERT_EQ(nullptr, dataset.getElement(0));
}
