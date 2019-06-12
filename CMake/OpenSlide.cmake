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
    set(OPENSLIDE_NAME "openslide-3.4.1")
    set(OPENSLIDE_SOURCES_DIR ${CMAKE_BINARY_DIR}/${OPENSLIDE_NAME})
    # "https://github.com/openslide/openslide/releases/download/v3.4.1/openslide-3.4.1.tar.gz"
    find_path(GLIB_INCLUDE_DIR NAMES glib.h PATH_SUFFIXES glib-2.0)
    find_path(GLIBC_INCLUDE_DIR NAMES glibconfig.h PATH_SUFFIXES lib64/glib-2.0/include)
    if(NOT GLIBC_INCLUDE_DIR)
        set(GLIBC_INCLUDE_DIR "/usr/lib/x86_64-linux-gnu/glib-2.0/")
    endif()
    find_path(CAIRO_INCLUDE_DIR NAMES cairo.h PATH_SUFFIXES cairo)
    find_path(GDK_INCLUDE_DIR NAMES gdk-pixbuf.h PATH_SUFFIXES gdk-pixbuf-2.0/gdk-pixbuf)
    find_package(LibXml2 REQUIRED)
    include_directories(${GDK_INCLUDE_DIR}/..)
    include_directories(${GLIB_INCLUDE_DIR})
    include_directories(${GLIBC_INCLUDE_DIR})
    include_directories(${CAIRO_INCLUDE_DIR})
    include_directories(${ZLIB_INCLUDE_DIR})
    include_directories(${LIBXML2_INCLUDE_DIR})
    include_directories(${OPENSLIDE_SOURCES_DIR}/src)
    include_directories(${OPENSLIDE_SOURCES_DIR})
    execute_process(COMMAND "${OPENSLIDE_SOURCES_DIR}/configure" WORKING_DIRECTORY ${OPENSLIDE_SOURCES_DIR})
    list(APPEND OPENSLIDE_SOURCES
      ${OPENSLIDE_SOURCES_DIR}/src/openslide.c
      ${OPENSLIDE_SOURCES_DIR}/src/openslide-cache.c
      ${OPENSLIDE_SOURCES_DIR}/src/openslide-cairo.h
      ${OPENSLIDE_SOURCES_DIR}/src/openslide-decode-jp2k.c
      ${OPENSLIDE_SOURCES_DIR}/src/openslide-decode-jp2k.h
      ${OPENSLIDE_SOURCES_DIR}/src/openslide-decode-jpeg.c
      ${OPENSLIDE_SOURCES_DIR}/src/openslide-decode-gdkpixbuf.h
      ${OPENSLIDE_SOURCES_DIR}/src/openslide-decode-gdkpixbuf.c
      ${OPENSLIDE_SOURCES_DIR}/src/openslide-decode-jpeg.h
      ${OPENSLIDE_SOURCES_DIR}/src/openslide-decode-png.c
      ${OPENSLIDE_SOURCES_DIR}/src/openslide-decode-png.h
      ${OPENSLIDE_SOURCES_DIR}/src/openslide-decode-sqlite.c
      ${OPENSLIDE_SOURCES_DIR}/src/openslide-decode-sqlite.h
      ${OPENSLIDE_SOURCES_DIR}/src/openslide-decode-tiff.c
      ${OPENSLIDE_SOURCES_DIR}/src/openslide-decode-tiff.h
      ${OPENSLIDE_SOURCES_DIR}/src/openslide-decode-tifflike.c
      ${OPENSLIDE_SOURCES_DIR}/src/openslide-decode-tifflike.h
      ${OPENSLIDE_SOURCES_DIR}/src/openslide-decode-xml.c
      ${OPENSLIDE_SOURCES_DIR}/src/openslide-decode-xml.h
      ${OPENSLIDE_SOURCES_DIR}/src/openslide-dll.rc
      ${OPENSLIDE_SOURCES_DIR}/src/openslide-dll.rc.in
      ${OPENSLIDE_SOURCES_DIR}/src/openslide-error.c
      ${OPENSLIDE_SOURCES_DIR}/src/openslide-grid.c
      ${OPENSLIDE_SOURCES_DIR}/src/openslide-hash.c
      ${OPENSLIDE_SOURCES_DIR}/src/openslide-jdatasrc.c
      ${OPENSLIDE_SOURCES_DIR}/src/openslide-tables.c
      ${OPENSLIDE_SOURCES_DIR}/src/openslide-util.c
      ${OPENSLIDE_SOURCES_DIR}/src/openslide-vendor-aperio.c
      ${OPENSLIDE_SOURCES_DIR}/src/openslide-vendor-generic-tiff.c
      ${OPENSLIDE_SOURCES_DIR}/src/openslide-vendor-hamamatsu.c
      ${OPENSLIDE_SOURCES_DIR}/src/openslide-vendor-leica.c
      ${OPENSLIDE_SOURCES_DIR}/src/openslide-vendor-mirax.c
      ${OPENSLIDE_SOURCES_DIR}/src/openslide-vendor-philips.c
      ${OPENSLIDE_SOURCES_DIR}/src/openslide-vendor-sakura.c
      ${OPENSLIDE_SOURCES_DIR}/src/openslide-vendor-trestle.c
      ${OPENSLIDE_SOURCES_DIR}/src/openslide-vendor-ventana.c
    )
else()
    find_path(OPENSLIDE_INCLUDE_DIR NAMES openslide.h
      PATH_SUFFIXES openslide
      )
    mark_as_advanced(OPENSLIDE_INCLUDE_DIR)

    find_library(OPENSLIDE_LIBRARY NAMES openslide)
    mark_as_advanced(OPENSLIDE_LIBRARY)
    set(OPENSLIDE_LIBRARIES ${OPENSLIDE_LIBRARY})
    set(OPENSLIDE_INCLUDE_DIRS ${OPENSLIDE_INCLUDE_DIR})
    include_directories(${OPENSLIDE_INCLUDE_DIR})
    link_libraries(${OPENSLIDE_LIBRARY})
endif()
