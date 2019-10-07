# Copyright 2019 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

if (STATIC_BUILD OR APPLE)
  set(JSONCPP_SOURCES_DIR ${CMAKE_BINARY_DIR}/jsoncpp-0.10.7/src/lib_json)

  include(CheckIncludeFileCXX)
  include(CheckTypeSize)
  include(CheckStructHasMember)
  include(CheckCXXSymbolExists)

  check_include_file_cxx(clocale HAVE_CLOCALE)
  check_cxx_symbol_exists(localeconv clocale HAVE_LOCALECONV)

  if(CMAKE_VERSION VERSION_LESS 3.0.0)
      check_include_file(locale.h HAVE_LOCALE_H)
      set(CMAKE_EXTRA_INCLUDE_FILES locale.h)
      check_type_size("struct lconv" LCONV_SIZE)
      unset(CMAKE_EXTRA_INCLUDE_FILES)
      check_struct_has_member("struct lconv" decimal_point locale.h HAVE_DECIMAL_POINT)
  else()
      set(CMAKE_EXTRA_INCLUDE_FILES clocale)
      check_type_size(lconv LCONV_SIZE LANGUAGE CXX)
      unset(CMAKE_EXTRA_INCLUDE_FILES)
      check_struct_has_member(lconv decimal_point clocale HAVE_DECIMAL_POINT LANGUAGE CXX)
  endif()

  if(NOT (HAVE_CLOCALE AND HAVE_LCONV_SIZE AND HAVE_DECIMAL_POINT AND HAVE_LOCALECONV))
      message(WARNING "Locale functionality is not supported")
      add_definitions(-DJSONCPP_NO_LOCALE_SUPPORT)
  endif()

  set(JSONCPP_INCLUDE_DIR ${JSONCPP_SOURCES_DIR}/../../include)

  set( PUBLIC_HEADERS
      )

  include_directories( ${JSONCPP_INCLUDE_DIR})
  set(JSONCPP_SOURCES
            ${JSONCPP_SOURCES_DIR}/json_tool.h
            ${JSONCPP_SOURCES_DIR}/json_reader.cpp
            ${JSONCPP_SOURCES_DIR}/json_valueiterator.inl
            ${JSONCPP_SOURCES_DIR}/json_value.cpp
            ${JSONCPP_SOURCES_DIR}/json_writer.cpp
            ${JSONCPP_SOURCES_DIR}/version.h.in
            ${JSONCPP_INCLUDE_DIR}/json/config.h
            ${JSONCPP_INCLUDE_DIR}/json/forwards.h
            ${JSONCPP_INCLUDE_DIR}/json/features.h
            ${JSONCPP_INCLUDE_DIR}/json/value.h
            ${JSONCPP_INCLUDE_DIR}/json/reader.h
            ${JSONCPP_INCLUDE_DIR}/json/writer.h
            ${JSONCPP_INCLUDE_DIR}/json/assertions.h
            ${JSONCPP_INCLUDE_DIR}/json/version.h
            )
else()
    find_library(JsonCpp_LIBRARY NAMES jsoncpp)
    find_path(JsonCpp_INCLUDE_DIR NAMES json/json.h PATH_SUFFIXES jsoncpp)

    include_directories(${JsonCpp_INCLUDE_DIR})
    link_libraries(${JsonCpp_LIBRARY})
endif()
