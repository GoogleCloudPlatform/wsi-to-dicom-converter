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

if (STATIC_BUILD)
  set(LIBJPEG_SOURCES_DIR ${CMAKE_BINARY_DIR}/jpeg-9c)
  include_directories(
    ${LIBJPEG_SOURCES_DIR}
    )

  list(APPEND LIBJPEG_SOURCES 
    ${LIBJPEG_SOURCES_DIR}/jaricom.c
    ${LIBJPEG_SOURCES_DIR}/jcapimin.c
    ${LIBJPEG_SOURCES_DIR}/jcapistd.c
    ${LIBJPEG_SOURCES_DIR}/jcarith.c
    ${LIBJPEG_SOURCES_DIR}/jccoefct.c
    ${LIBJPEG_SOURCES_DIR}/jccolor.c
    ${LIBJPEG_SOURCES_DIR}/jcdctmgr.c
    ${LIBJPEG_SOURCES_DIR}/jchuff.c
    ${LIBJPEG_SOURCES_DIR}/jcinit.c
    ${LIBJPEG_SOURCES_DIR}/jcmarker.c
    ${LIBJPEG_SOURCES_DIR}/jcmaster.c
    ${LIBJPEG_SOURCES_DIR}/jcomapi.c
    ${LIBJPEG_SOURCES_DIR}/jcparam.c
    ${LIBJPEG_SOURCES_DIR}/jcprepct.c
    ${LIBJPEG_SOURCES_DIR}/jcsample.c
    ${LIBJPEG_SOURCES_DIR}/jctrans.c
    ${LIBJPEG_SOURCES_DIR}/jdapimin.c
    ${LIBJPEG_SOURCES_DIR}/jdapistd.c
    ${LIBJPEG_SOURCES_DIR}/jdarith.c
    ${LIBJPEG_SOURCES_DIR}/jdatadst.c
    ${LIBJPEG_SOURCES_DIR}/jdatasrc.c
    ${LIBJPEG_SOURCES_DIR}/jdcoefct.c
    ${LIBJPEG_SOURCES_DIR}/jdcolor.c
    ${LIBJPEG_SOURCES_DIR}/jddctmgr.c
    ${LIBJPEG_SOURCES_DIR}/jdhuff.c
    ${LIBJPEG_SOURCES_DIR}/jdinput.c
    ${LIBJPEG_SOURCES_DIR}/jcmainct.c
    ${LIBJPEG_SOURCES_DIR}/jdmainct.c
    ${LIBJPEG_SOURCES_DIR}/jdmarker.c
    ${LIBJPEG_SOURCES_DIR}/jdmaster.c
    ${LIBJPEG_SOURCES_DIR}/jdmerge.c
    ${LIBJPEG_SOURCES_DIR}/jdpostct.c
    ${LIBJPEG_SOURCES_DIR}/jdsample.c
    ${LIBJPEG_SOURCES_DIR}/jdtrans.c
    ${LIBJPEG_SOURCES_DIR}/jerror.c
    ${LIBJPEG_SOURCES_DIR}/jfdctflt.c
    ${LIBJPEG_SOURCES_DIR}/jfdctfst.c
    ${LIBJPEG_SOURCES_DIR}/jfdctint.c
    ${LIBJPEG_SOURCES_DIR}/jidctflt.c
    ${LIBJPEG_SOURCES_DIR}/jidctfst.c
    ${LIBJPEG_SOURCES_DIR}/jidctint.c
    ${LIBJPEG_SOURCES_DIR}/jmemmgr.c
    ${LIBJPEG_SOURCES_DIR}/jmemnobs.c
    ${LIBJPEG_SOURCES_DIR}/jquant1.c
    ${LIBJPEG_SOURCES_DIR}/jquant2.c
    ${LIBJPEG_SOURCES_DIR}/jutils.c
    )

  configure_file(
    ${LIBJPEG_SOURCES_DIR}/jconfig.txt
    ${LIBJPEG_SOURCES_DIR}/jconfig.h COPYONLY
    )

else()
  include(FindJPEG)
  if (NOT ${JPEG_FOUND})
    message(FATAL_ERROR "Unable to find libjpeg")
  endif()
  include_directories(${JPEG_INCLUDE_DIR})
  link_libraries(${JPEG_LIBRARIES})
endif()
