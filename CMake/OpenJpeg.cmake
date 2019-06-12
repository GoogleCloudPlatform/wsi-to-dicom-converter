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
    include_regular_expression("^.*$")
    set(OPENJPEG_VERSION_MAJOR 2)
    set(OPENJPEG_VERSION_MINOR 3)
    set(OPENJPEG_VERSION_BUILD 0)
    set(OPENJPEG_VERSION
      "${OPENJPEG_VERSION_MAJOR}.${OPENJPEG_VERSION_MINOR}.${OPENJPEG_VERSION_BUILD}")
    set(PACKAGE_VERSION
      "${OPENJPEG_VERSION_MAJOR}.${OPENJPEG_VERSION_MINOR}.${OPENJPEG_VERSION_BUILD}")

    set(OPENJPEG_NAME openjpeg-2.3.0)
    set(OPENJPEG_SOURCES_DIR ${CMAKE_BINARY_DIR}/${OPENJPEG_NAME})
    #"https://github.com/uclouvain/openjpeg/archive/v2.3.0.tar.gz"

    include_directories(
      ${OPENJPEG_SOURCES_DIR}/src/lib/openjp3d
    )
    set(OPENJPEG_SOURCES_DIR ${OPENJPEG_SOURCES_DIR}/src/lib/openjp2)


    include (${CMAKE_ROOT}/Modules/CheckIncludeFile.cmake)

    macro(ensure_file_include INCLUDE_FILENAME VARIABLE_NAME MANDATORY_STATUS)

    CHECK_INCLUDE_FILE(${INCLUDE_FILENAME} ${VARIABLE_NAME})

    if (NOT ${${VARIABLE_NAME}})
      if (${MANDATORY_STATUS})
        message(FATAL_ERROR "The file ${INCLUDE_FILENAME} is mandatory but not found on your system")
      endif()
    endif()

    endmacro()

    ensure_file_include("stdint.h"   OPJ_HAVE_STDINT_H   YES)
    ensure_file_include("inttypes.h" OPJ_HAVE_INTTYPES_H YES)

    configure_file(
     ${OPENJPEG_SOURCES_DIR}/opj_config.h.cmake.in
     ${OPENJPEG_SOURCES_DIR}/opj_config.h
     @ONLY
     )
    message(${CMAKE_CURRENT_BINARY_DIR})
     configure_file(
     ${OPENJPEG_SOURCES_DIR}/opj_config_private.h.cmake.in
     ${OPENJPEG_SOURCES_DIR}/opj_config_private.h
     @ONLY
     )

    include_directories(
      ${CMAKE_BINARY_DIR}
    )
    include_directories(
      ${OPENJPEG_SOURCES_DIR}
    )

    set(OPENJPEG_SRCS
      ${OPENJPEG_SOURCES_DIR}/thread.c
      ${OPENJPEG_SOURCES_DIR}/thread.h
      ${OPENJPEG_SOURCES_DIR}/bio.c
      ${OPENJPEG_SOURCES_DIR}/bio.h
      ${OPENJPEG_SOURCES_DIR}/cio.c
      ${OPENJPEG_SOURCES_DIR}/cio.h
      ${OPENJPEG_SOURCES_DIR}/dwt.c
      ${OPENJPEG_SOURCES_DIR}/dwt.h
      ${OPENJPEG_SOURCES_DIR}/event.c
      ${OPENJPEG_SOURCES_DIR}/event.h
      ${OPENJPEG_SOURCES_DIR}/image.c
      ${OPENJPEG_SOURCES_DIR}/image.h
      ${OPENJPEG_SOURCES_DIR}/invert.c
      ${OPENJPEG_SOURCES_DIR}/invert.h
      ${OPENJPEG_SOURCES_DIR}/j2k.c
      ${OPENJPEG_SOURCES_DIR}/j2k.h
      ${OPENJPEG_SOURCES_DIR}/jp2.c
      ${OPENJPEG_SOURCES_DIR}/jp2.h
      ${OPENJPEG_SOURCES_DIR}/mct.c
      ${OPENJPEG_SOURCES_DIR}/mct.h
      ${OPENJPEG_SOURCES_DIR}/mqc.c
      ${OPENJPEG_SOURCES_DIR}/mqc.h
      ${OPENJPEG_SOURCES_DIR}/mqc_inl.h
      ${OPENJPEG_SOURCES_DIR}/openjpeg.c
      ${OPENJPEG_SOURCES_DIR}/openjpeg.h
      ${OPENJPEG_SOURCES_DIR}/opj_clock.c
      ${OPENJPEG_SOURCES_DIR}/opj_clock.h
      ${OPENJPEG_SOURCES_DIR}/pi.c
      ${OPENJPEG_SOURCES_DIR}/pi.h
      ${OPENJPEG_SOURCES_DIR}/t1.c
      ${OPENJPEG_SOURCES_DIR}/t1.h
      ${OPENJPEG_SOURCES_DIR}/t2.c
      ${OPENJPEG_SOURCES_DIR}/t2.h
      ${OPENJPEG_SOURCES_DIR}/tcd.c
      ${OPENJPEG_SOURCES_DIR}/tcd.h
      ${OPENJPEG_SOURCES_DIR}/tgt.c
      ${OPENJPEG_SOURCES_DIR}/tgt.h
      ${OPENJPEG_SOURCES_DIR}/function_list.c
      ${OPENJPEG_SOURCES_DIR}/function_list.h
      ${OPENJPEG_SOURCES_DIR}/opj_codec.h
      ${OPENJPEG_SOURCES_DIR}/opj_includes.h
      ${OPENJPEG_SOURCES_DIR}/opj_intmath.h
      ${OPENJPEG_SOURCES_DIR}/opj_malloc.c
      ${OPENJPEG_SOURCES_DIR}/opj_malloc.h
      ${OPENJPEG_SOURCES_DIR}/opj_stdint.h
      ${OPENJPEG_SOURCES_DIR}/sparse_array.c
      ${OPENJPEG_SOURCES_DIR}/sparse_array.h
    )
    if(BUILD_JPIP)
      add_definitions(-DUSE_JPIP)
      set(OPENJPEG_SRCS
        ${OPENJPEG_SRCS}
        ${OPENJPEG_SOURCES_DIR}/cidx_manager.c
        ${OPENJPEG_SOURCES_DIR}/cidx_manager.h
        ${OPENJPEG_SOURCES_DIR}/phix_manager.c
        ${OPENJPEG_SOURCES_DIR}/ppix_manager.c
        ${OPENJPEG_SOURCES_DIR}/thix_manager.c
        ${OPENJPEG_SOURCES_DIR}/tpix_manager.c
        ${OPENJPEG_SOURCES_DIR}/indexbox_manager.h
      )
    endif()

else()
    find_package(OpenJPEG REQUIRED)
    include_directories(${OPENJPEG_INCLUDE_DIRS})
    include_directories(${OPENJPEG_INCLUDE_DIRS}/openjpeg-2.3/)
    link_libraries(${OPENJPEG_LIBRARIES})
endif()
