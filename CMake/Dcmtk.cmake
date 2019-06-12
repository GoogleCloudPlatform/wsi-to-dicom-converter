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

SET(DCMTK_SOURCES_DIR ${CMAKE_BINARY_DIR}/dcmtk-3.6.2)
# https://dicom.offis.de/download/dcmtk/dcmtk362/dcmtk-3.6.2.zip")
if(NOT EXISTS ${DCMTK_SOURCES_DIR})
     message(FATAL_ERROR "Unable to find dcmtk source code, please copy it to ${DCMTK_SOURCES_DIR}")
endif()
macro(DCMTK_UNSET)
endmacro()

macro(DCMTK_UNSET_CACHE)
endmacro()

set(DCMTK_BINARY_DIR ${DCMTK_SOURCES_DIR}/)
set(DCMTK_CMAKE_INCLUDE ${DCMTK_SOURCES_DIR}/)
set(DCMTK_WITH_THREADS ON)

configure_file(
  ${CMAKE_SOURCE_DIR}/patches/arith.h
  ${DCMTK_SOURCES_DIR}/config/include/dcmtk/config/arith.h
  COPYONLY)

SET(DCMTK_SOURCE_DIR ${DCMTK_SOURCES_DIR})
SET(DCMTK_ENABLE_BUILTIN_DICTIONARY ON)
SET(DCMTK_ENABLE_CXX11 ON)
include(${DCMTK_SOURCES_DIR}/CMake/CheckFunctionWithHeaderExists.cmake)
include(${DCMTK_SOURCES_DIR}/CMake/GenerateDCMTKConfigure.cmake)



set(DCMTK_PACKAGE_VERSION_SUFFIX "")
set(DCMTK_PACKAGE_VERSION_NUMBER ${DCMTK_VERSION_NUMBER})

CONFIGURE_FILE(
${DCMTK_SOURCES_DIR}/CMake/osconfig.h.in
${DCMTK_SOURCES_DIR}/config/include/dcmtk/config/osconfig.h)


add_definitions(-DDCMTK_INSIDE_LOG4CPLUS=1)

AUX_SOURCE_DIRECTORY(${DCMTK_SOURCES_DIR}/dcmdata/libsrc DCMTK_SOURCES)
AUX_SOURCE_DIRECTORY(${DCMTK_SOURCES_DIR}/dcmdata/libi2d DCMTK_SOURCES)
AUX_SOURCE_DIRECTORY(${DCMTK_SOURCES_DIR}/ofstd/libsrc DCMTK_SOURCES)
AUX_SOURCE_DIRECTORY(${DCMTK_SOURCES_DIR}/oflog/libsrc DCMTK_SOURCES)


if (${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    list(REMOVE_ITEM DCMTK_SOURCES
      ${DCMTK_SOURCES_DIR}/oflog/libsrc/unixsock.cc
      ${DCMTK_SOURCES_DIR}/oflog/libsrc/clfsap.cc
      )
else()
    list(REMOVE_ITEM DCMTK_SOURCES
      ${DCMTK_SOURCES_DIR}/oflog/libsrc/clfsap.cc
      ${DCMTK_SOURCES_DIR}/oflog/libsrc/windebap.cc
      ${DCMTK_SOURCES_DIR}/oflog/libsrc/winsock.cc
      )
endif()


list(REMOVE_ITEM DCMTK_SOURCES
${DCMTK_SOURCES_DIR}/dcmdata/libsrc/mkdictbi.cc
${DCMTK_SOURCES_DIR}/dcmdata/libsrc/mkdeftag.cc
)

add_definitions(
-DLOG4CPLUS_DISABLE_FATAL=1
-DDCMTK_VERSION_NUMBER=${DCMTK_VERSION_NUMBER}
)

add_definitions(
  -DDCMTK_LOG4CPLUS_DISABLE_FATAL=1
  -DDCMTK_LOG4CPLUS_DISABLE_ERROR=1
  -DDCMTK_LOG4CPLUS_DISABLE_WARN=1
  -DDCMTK_LOG4CPLUS_DISABLE_INFO=1
  -DDCMTK_LOG4CPLUS_DISABLE_DEBUG=1
  )

include_directories(
${DCMTK_SOURCES_DIR}
${DCMTK_SOURCES_DIR}/config/include
${DCMTK_SOURCES_DIR}/ofstd/include
${DCMTK_SOURCES_DIR}/oflog/include
${DCMTK_SOURCES_DIR}/dcmdata/include
)

set(DCMTK_DICTIONARIES
  DICTIONARY_DICOM ${DCMTK_SOURCES_DIR}/dcmdata/data/dicom.dic
  DICTIONARY_PRIVATE ${DCMTK_SOURCES_DIR}/dcmdata/data/private.dic
  DICTIONARY_DICONDE ${DCMTK_SOURCES_DIR}/dcmdata/data/diconde.dic
  )

include_directories(${DCMTK_INCLUDE_DIRS})

add_definitions(-DHAVE_CONFIG_H=1)
add_definitions(-DDCMTK_USE_EMBEDDED_DICTIONARIES=1)
