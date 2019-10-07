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

set(BOOST_STATIC OFF)
if (STATIC_BUILD OR APPLE)
  set(BOOST_STATIC 1)
else()
  include(FindBoost)

  set (Boost_USE_STATIC_LIBS OFF)
  set (Boost_USE_MULTITHREADED ON)
  add_definitions(-DBOOST_LOG_DYN_LINK)
  list(APPEND BOOST_LIBS filesystem thread system date_time regex program_options log_setup)
  find_package(Boost COMPONENTS "${BOOST_LIBS}")
  if (NOT Boost_FOUND)
    message(FATAL_ERROR "Fatal error: Boost (version >= 1.69) required.")
  endif()
  include_directories(${Boost_INCLUDE_DIRS})
  link_libraries(${Boost_LIBRARIES})
endif()

if (BOOST_STATIC)
 set(BOOST_NAME boost_1_69_0)
 set(BOOST_SOURCES_DIR ${CMAKE_BINARY_DIR}/${BOOST_NAME})
  if (CMAKE_COMPILER_IS_GNUCXX)
    add_definitions(-isystem ${BOOST_SOURCES_DIR})
  endif()

  include_directories(
    BEFORE ${BOOST_SOURCES_DIR}
    )

  add_definitions(
    -DBOOST_ALL_NO_LIB
    -DBOOST_ALL_NOLIB 
    -DBOOST_DATE_TIME_NO_LIB 
    -DBOOST_LOG_NO_LIB
    -DBOOST_LOG_NOLIB
    -DBOOST_LOG_DYN_LINK
    -DBOOST_THREAD_BUILD_LIB
    -DBOOST_PROGRAM_OPTIONS_NO_LIB
    -DBOOST_REGEX_NO_LIB
    -DBOOST_SYSTEM_NO_LIB
    -DBOOST_LOCALE_NO_LIB
    )

  set(BOOST_SOURCES
    ${BOOST_SOURCES_DIR}/libs/system/src/error_code.cpp
    )

   list(APPEND BOOST_SOURCES
      ${BOOST_SOURCES_DIR}/libs/atomic/src/lockpool.cpp
      ${BOOST_SOURCES_DIR}/libs/thread/src/pthread/once.cpp
      ${BOOST_SOURCES_DIR}/libs/thread/src/pthread/thread.cpp
      ${BOOST_SOURCES_DIR}/libs/chrono/src/chrono.cpp
      )

    list(APPEND BOOST_SOURCES
      ${BOOST_SOURCES_DIR}/libs/program_options/src/value_semantic.cpp
      ${BOOST_SOURCES_DIR}/libs/program_options/src/config_file.cpp
      ${BOOST_SOURCES_DIR}/libs/program_options/src/convert.cpp
      ${BOOST_SOURCES_DIR}/libs/program_options/src/options_description.cpp
      ${BOOST_SOURCES_DIR}/libs/program_options/src/positional_options.cpp
      ${BOOST_SOURCES_DIR}/libs/program_options/src/parsers.cpp
      ${BOOST_SOURCES_DIR}/libs/program_options/src/variables_map.cpp
      ${BOOST_SOURCES_DIR}/libs/program_options/src/cmdline.cpp
      ${BOOST_SOURCES_DIR}/libs/program_options/src/value_semantic.cpp
      ${BOOST_SOURCES_DIR}/libs/program_options/src/utf8_codecvt_facet.cpp
      ${BOOST_SOURCES_DIR}/libs/log/src/trivial.cpp
      ${BOOST_SOURCES_DIR}/libs/log/src/attribute_name.cpp
      ${BOOST_SOURCES_DIR}/libs/log/src/attribute_set.cpp
      ${BOOST_SOURCES_DIR}/libs/log/src/attribute_value_set.cpp
      ${BOOST_SOURCES_DIR}/libs/log/src/code_conversion.cpp
      ${BOOST_SOURCES_DIR}/libs/log/src/date_time_format_parser.cpp
      ${BOOST_SOURCES_DIR}/libs/log/src/default_attribute_names.cpp
      ${BOOST_SOURCES_DIR}/libs/log/src/default_sink.cpp
      ${BOOST_SOURCES_DIR}/libs/log/src/dump.cpp
      ${BOOST_SOURCES_DIR}/libs/log/src/event.cpp
      ${BOOST_SOURCES_DIR}/libs/log/src/exceptions.cpp
      ${BOOST_SOURCES_DIR}/libs/log/src/format_parser.cpp
      ${BOOST_SOURCES_DIR}/libs/log/src/global_logger_storage.cpp
      ${BOOST_SOURCES_DIR}/libs/log/src/named_scope.cpp
      ${BOOST_SOURCES_DIR}/libs/log/src/named_scope_format_parser.cpp
      ${BOOST_SOURCES_DIR}/libs/log/src/once_block.cpp
      ${BOOST_SOURCES_DIR}/libs/log/src/permissions.cpp
      ${BOOST_SOURCES_DIR}/libs/log/src/process_id.cpp
      ${BOOST_SOURCES_DIR}/libs/log/src/process_name.cpp
      ${BOOST_SOURCES_DIR}/libs/log/src/record_ostream.cpp
      ${BOOST_SOURCES_DIR}/libs/log/src/severity_level.cpp
      ${BOOST_SOURCES_DIR}/libs/log/src/spirit_encoding.cpp
      ${BOOST_SOURCES_DIR}/libs/log/src/syslog_backend.cpp
      ${BOOST_SOURCES_DIR}/libs/log/src/text_file_backend.cpp
      ${BOOST_SOURCES_DIR}/libs/log/src/text_multifile_backend.cpp
      ${BOOST_SOURCES_DIR}/libs/log/src/text_ostream_backend.cpp
      ${BOOST_SOURCES_DIR}/libs/log/src/thread_id.cpp
      ${BOOST_SOURCES_DIR}/libs/log/src/thread_specific.cpp
      ${BOOST_SOURCES_DIR}/libs/log/src/threadsafe_queue.cpp
      ${BOOST_SOURCES_DIR}/libs/log/src/timer.cpp
      ${BOOST_SOURCES_DIR}/libs/log/src/timestamp.cpp
      ${BOOST_SOURCES_DIR}/libs/log/src/trivial.cpp
      ${BOOST_SOURCES_DIR}/libs/log/src/core.cpp
      ${BOOST_SOURCES_DIR}/libs/log/src/unhandled_exception_count.cpp
      ${BOOST_SOURCES_DIR}/libs/filesystem/src/codecvt_error_category.cpp
      ${BOOST_SOURCES_DIR}/libs/filesystem/src/operations.cpp
      ${BOOST_SOURCES_DIR}/libs/filesystem/src/path.cpp
      ${BOOST_SOURCES_DIR}/libs/filesystem/src/path_traits.cpp
      ${BOOST_SOURCES_DIR}/libs/filesystem/src/utf8_codecvt_facet.cpp)
endif()
