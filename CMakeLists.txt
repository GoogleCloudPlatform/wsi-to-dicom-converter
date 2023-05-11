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

cmake_minimum_required(VERSION 2.8.12)
project(wsi2dcmCli)
add_executable(${PROJECT_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp )
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/src/ DICOMIZER_SOURCES)
include_directories(wsi2dcmCli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/)
include_directories(wsi2dcmCli PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/)
include_directories(wsi2dcmCli PUBLIC /usr/local/include/opencv4/)
include_directories(wsi2dcmCli PUBLIC ${TURBOJPEG_INCLUDE_DIRS})
include_directories(wsi2dcmCli PUBLIC /abseil/install/include/)
include_directories(wsi2dcmCli PUBLIC /usr/local/include/openslide)
link_directories(${TURBOJPEG_LIBDIR})

ADD_DEFINITIONS(-DBOOST_LOG_DYN_LINK)
find_package(Boost 1.82.0 COMPONENTS log program_options thread chrono REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})
link_directories(/usr/local/lib)

set(STATIC_BUILD OFF CACHE BOOL ON)
set(TESTS_BUILD OFF CACHE BOOL ON)

set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package(Threads REQUIRED)
FIND_PACKAGE(OpenCV REQUIRED)
FIND_PACKAGE(JPEG REQUIRED)
FIND_PACKAGE(DCMTK REQUIRED)
SET(DCMTK_DIR /usr/local/include/dcmtk)
INCLUDE_DIRECTORIES(${DCMTK_INCLUDE_DIRS})
find_package(OpenJPEG REQUIRED)
include_directories(${OPENJPEG_INCLUDE_DIRS})

add_library(wsi2dcm SHARED    
    ${OPENJPEG_SRCS}
    ${DICOMIZER_SOURCES}
    ${JSONCPP_SOURCES}
    ${OpenCV_SOURCES}
    ${TURBOJPEG_SOURCES}
    /abseil/install/lib
 )

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

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
target_link_libraries(wsi2dcm pthread ${STATIC_LIBS} z m lzma xml2 ${OpenCV_LIBS} ${TURBOJPEG_LIBRARIES} ofstd ${DCMTK_LIBRARIES}  ${Boost_LIBRARIES} openslide jsoncpp ${OPENJPEG_LIBRARIES})
target_link_libraries(wsi2dcmCli wsi2dcm)

if (TESTS_BUILD)
    find_package(GTest REQUIRED)
    include_directories(${GTEST_INCLUDE_DIRS})

    AUX_SOURCE_DIRECTORY(${CMAKE_CURRENT_SOURCE_DIR}/tests/ TESTS_SOURCES)
    add_executable(gTests ${TESTS_SOURCES})

    target_link_libraries(gTests ${GTEST_LIBRARIES} wsi2dcm)
endif()