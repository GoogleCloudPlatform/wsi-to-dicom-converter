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
#include <dcmtk/dcmdata/dcitem.h>
#include <dcmtk/dcmdata/dctag.h>
#include <dcmtk/dcmdata/dcvrat.h>
#include <json/json.h>
#include <boost/log/trivial.hpp>
#include <memory>
#include <string>
static const char VALUE[] = "Value";

DcmTags::DcmTags() {}

inline Json::Value readJsonTag(Json::Value obj, std::string tag) {
  if (obj[tag][VALUE] != nullptr) {
    return obj[tag][VALUE][0];
  } else {
    return Json::Value();
  }
}
inline std::string splitTagName(std::string tagName) {
  return tagName.insert(4, ",");
}
inline std::string tagValueAsString(Json::Value value) {
  if (value.type() == Json::stringValue) {
    return value.asString();
  }
  if (value.type() == Json::intValue) {
    return std::to_string(value.asInt());
  }
  if (value.type() == Json::realValue) {
    return std::to_string(value.asDouble());
  }
  return "";
}

DcmItem* parseJsonTag(Json::Value jsonNode, DcmItem* dcmItem) {
  const Json::Value::Members tags = jsonNode.getMemberNames();
  for (size_t subNodeIndex = 0; subNodeIndex < tags.size(); subNodeIndex++) {
    DcmTag tag;
    std::string tagName = tags[subNodeIndex];
    tagName = splitTagName(tagName);
    DcmTag::findTagFromName(tagName.c_str(), tag);
    Json::Value value = jsonNode[tags[subNodeIndex]][VALUE];
    Json::Value firstValue = jsonNode[tags[subNodeIndex]][VALUE][0];
    DcmVR vr = DcmVR(jsonNode[tags[subNodeIndex]]["vr"].asCString());
    auto element = std::make_unique<DcmItem>();
    std::string stringValue;
    switch (vr.getEVR()) {
      case EVR_IS:
      case EVR_DS:
      case EVR_AS:
      case EVR_DA:
      case EVR_DT:
      case EVR_TM:
      case EVR_AE:
      case EVR_CS:
      case EVR_SH:
      case EVR_LO:
      case EVR_ST:
      case EVR_LT:
      case EVR_UT:
      case EVR_PN:
      case EVR_UI:
      case EVR_UC:
      case EVR_UR:
        stringValue = tagValueAsString(firstValue);
        for (int valueIndex = 1; valueIndex < value.size(); valueIndex++) {
          stringValue += "\\" + tagValueAsString(value[valueIndex]);
        }
        dcmItem->putAndInsertString(tag, stringValue.c_str());

        break;
      case EVR_SL:
        dcmItem->putAndInsertSint32(tag, firstValue.asInt());
        break;

      case EVR_SS:
        dcmItem->putAndInsertSint16(tag, firstValue.asInt());
        break;

      case EVR_UL:
        dcmItem->putAndInsertUint32(tag, firstValue.asInt());
        break;

      case EVR_US:
        dcmItem->putAndInsertUint16(tag, firstValue.asInt());
        break;

      case EVR_FL:
        dcmItem->putAndInsertFloat32(tag, firstValue.asFloat());
        break;

      case EVR_FD:
        dcmItem->putAndInsertFloat64(tag, firstValue.asDouble());
        break;
      case EVR_AT: {
        DcmTag tagValue;
        stringValue = firstValue.asString();
        stringValue = splitTagName(stringValue);
        DcmTag::findTagFromName(stringValue.c_str(), tagValue);
        auto attributeTag = std::make_unique<DcmAttributeTag>(tag);
        attributeTag->putTagVal(tagValue);
        dcmItem->insert(attributeTag.release());
        break;
      }
      case EVR_SQ:
        dcmItem->insertSequenceItem(
            tag, parseJsonTag(firstValue, element.release()));
        break;
      default:
        BOOST_LOG_TRIVIAL(warning) << "unknown tag " << vr.getVRName();
    }
  }
  return dcmItem;
}

void DcmTags::readJsonFile(std::string fileName) {
  std::ifstream fileStream(fileName);
  readInputStream(&fileStream);
}

void DcmTags::readInputStream(std::istream* inputStream) {
  Json::CharReaderBuilder charReader;
  Json::Value jsonRoot;
  std::string errors;
  if (parseFromStream(charReader, *inputStream, &jsonRoot, &errors)) {
    try {
      parseJsonTag(jsonRoot, &dataset_);
    } catch (const std::exception& e) {
      BOOST_LOG_TRIVIAL(warning) << "can't read DCM tags from JSON" << std::endl
                                 << e.what();
    }
  } else {
    BOOST_LOG_TRIVIAL(warning) << "can't parse JSON";
    BOOST_LOG_TRIVIAL(warning) << errors;
  }
}

void DcmTags::populateDataset(DcmDataset* dataset) {
  size_t elementIndex = 0;
  DcmElement* element;
  element = dataset_.getElement(elementIndex);
  while (element != nullptr) {
    dataset->insert(OFstatic_cast(DcmElement*, element->clone()));
    element = dataset_.getElement(++elementIndex);
  }
}
