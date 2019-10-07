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

cmake_minimum_required(VERSION 2.8)
project(wsi2dcmCli)
add_executable(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp )
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/src/ DICOMIZER_SOURCES)
include_directories(wsi2dcmCli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/)
include_directories(wsi2dcmCli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/)

set(STATIC_BUILD OFF CACHE BOOL ON)
set(TESTS_BUILD OFF CACHE BOOL ON)

include(CMake/Boost.cmake)
include(CMake/OpenJpeg.cmake)
include(CMake/LibJpeg.cmake)
include(CMake/Dcmtk.cmake)
include(CMake/OpenSlide.cmake)
include(CMake/JsonCpp.cmake)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)
add_library(wsi2dcm SHARED
    ${DCMTK_SOURCES}
    ${BOOST_SOURCES}
    ${OPENSLIDE_SOURCES}
    ${OPENJPEG_SRCS}
    ${LIBJPEG_SOURCES}
    ${DICOMIZER_SOURCES}
    ${JSONCPP_SOURCES}
 )
if(APPLE)
    set(CMAKE_MACOSX_RPATH 1)
endif()
if(STATIC_BUILD)
    list(APPEND STATIC_LIBS glib-2.0 png jpeg gthread-2.0 gio-2.0 gobject-2.0 tiff z m lzma xml2 cairo sqlite3 gdk_pixbuf-2.0 -static-libgcc -static-libstdc++)
endif()
set_target_properties(wsi2dcm PROPERTIES
                        C_STANDARD 11
                        PUBLIC_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/src/wsi2dcm.h"
                     )
set_target_properties(wsi2dcmCli PROPERTIES
                        OUTPUT_NAME wsi2dcm
                      )

target_link_libraries(wsi2dcm pthread ${STATIC_LIBS} z m lzma xml2)
target_link_libraries(wsi2dcmCli wsi2dcm)

if (TESTS_BUILD)
    find_package(GTest REQUIRED)
    include_directories(${GTEST_INCLUDE_DIRS})

    AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/tests/ TESTS_SOURCES)
    add_executable(gTests ${TESTS_SOURCES})

    target_link_libraries(gTests ${GTEST_LIBRARIES} wsi2dcm)
endif()
