cmake_minimum_required(VERSION 3.27)

project(casacore_mini VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CASACORE_MINI_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(CASACORE_MINI_ENABLE_CLANG_TIDY "Enable clang-tidy linting" OFF)
option(CASACORE_MINI_ENABLE_COVERAGE "Enable coverage instrumentation" OFF)
option(CASACORE_MINI_ENABLE_DOXYGEN "Enable Doxygen documentation target" ON)

find_package(Git QUIET)
set(CASACORE_MINI_GIT_REVISION "unknown")
set(CASACORE_MINI_GIT_DESCRIBE "unknown")

if(Git_FOUND AND EXISTS "${CMAKE_SOURCE_DIR}/.git")
  execute_process(
    COMMAND ${GIT_EXECUTABLE} -C "${CMAKE_SOURCE_DIR}" rev-parse --short=12 HEAD
    OUTPUT_VARIABLE CASACORE_MINI_GIT_REVISION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)

  execute_process(
    COMMAND ${GIT_EXECUTABLE} -C "${CMAKE_SOURCE_DIR}" describe --tags --dirty --always --match v[0-9]*
    OUTPUT_VARIABLE CASACORE_MINI_GIT_DESCRIBE
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
endif()

if(NOT CASACORE_MINI_GIT_REVISION)
  set(CASACORE_MINI_GIT_REVISION "unknown")
endif()

if(NOT CASACORE_MINI_GIT_DESCRIBE)
  set(CASACORE_MINI_GIT_DESCRIBE "unknown")
endif()

if(CASACORE_MINI_GIT_DESCRIBE STREQUAL "unknown")
  set(CASACORE_MINI_BUILD_VERSION "${PROJECT_VERSION}-dev")
elseif(CASACORE_MINI_GIT_DESCRIBE MATCHES "^v([0-9]+\\.[0-9]+\\.[0-9]+)$")
  if(CMAKE_MATCH_1 STREQUAL PROJECT_VERSION)
    set(CASACORE_MINI_BUILD_VERSION "${PROJECT_VERSION}")
  else()
    set(CASACORE_MINI_BUILD_VERSION "${PROJECT_VERSION}+${CASACORE_MINI_GIT_DESCRIBE}")
  endif()
else()
  set(CASACORE_MINI_BUILD_VERSION "${PROJECT_VERSION}+${CASACORE_MINI_GIT_DESCRIBE}")
endif()

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/version_generated.hpp.in"
  "${CMAKE_CURRENT_BINARY_DIR}/generated/include/casacore_mini/version_generated.hpp"
  @ONLY)

add_library(casacore_mini_project_options INTERFACE)
target_compile_features(casacore_mini_project_options INTERFACE cxx_std_20)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  target_compile_options(
    casacore_mini_project_options
    INTERFACE
      -Wall
      -Wextra
      -Wpedantic
      -Wconversion
      -Wsign-conversion
      -Wshadow
      -Wformat=2
      -Wundef
      -Wnon-virtual-dtor)

  if(CASACORE_MINI_WARNINGS_AS_ERRORS)
    target_compile_options(casacore_mini_project_options INTERFACE -Werror)
  endif()
elseif(MSVC)
  target_compile_options(casacore_mini_project_options INTERFACE /W4)
  if(CASACORE_MINI_WARNINGS_AS_ERRORS)
    target_compile_options(casacore_mini_project_options INTERFACE /WX)
  endif()
endif()

if(CASACORE_MINI_ENABLE_COVERAGE)
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    target_compile_options(casacore_mini_project_options INTERFACE --coverage)
    target_link_options(casacore_mini_project_options INTERFACE --coverage)
  else()
    message(WARNING "Coverage requested but compiler is not Clang/GNU; skipping coverage flags")
  endif()
endif()

if(CASACORE_MINI_ENABLE_CLANG_TIDY)
  find_program(CLANG_TIDY_EXE NAMES clang-tidy REQUIRED)
  set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}")
endif()

add_library(
  casacore_mini_core
  src/version.cpp
  src/record.cpp
  src/record_io.cpp
  src/aipsio_reader.cpp
  src/table_dat.cpp
  src/table_schema.cpp
  src/measure_coord_metadata.cpp
  src/keyword_record.cpp)
target_include_directories(
  casacore_mini_core
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated/include>)
target_link_libraries(casacore_mini_core PUBLIC casacore_mini_project_options)

enable_testing()
add_executable(version_test tests/version_test.cpp)
target_link_libraries(version_test PRIVATE casacore_mini_core casacore_mini_project_options)
add_test(NAME version_test COMMAND version_test)

add_executable(record_io_test tests/record_io_test.cpp)
target_link_libraries(record_io_test PRIVATE casacore_mini_core casacore_mini_project_options)
add_test(NAME record_io_test COMMAND record_io_test)

add_executable(aipsio_reader_test tests/aipsio_reader_test.cpp)
target_link_libraries(aipsio_reader_test PRIVATE casacore_mini_core casacore_mini_project_options)
add_test(NAME aipsio_reader_test COMMAND aipsio_reader_test)

add_executable(table_dat_test tests/table_dat_test.cpp)
target_link_libraries(table_dat_test PRIVATE casacore_mini_core casacore_mini_project_options)
target_compile_definitions(
  table_dat_test
  PRIVATE CASACORE_MINI_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_test(NAME table_dat_test COMMAND table_dat_test)

add_executable(table_schema_test tests/table_schema_test.cpp)
target_link_libraries(table_schema_test PRIVATE casacore_mini_core casacore_mini_project_options)
target_compile_definitions(
  table_schema_test
  PRIVATE CASACORE_MINI_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_test(NAME table_schema_test COMMAND table_schema_test)

add_executable(measure_coord_metadata_test tests/measure_coord_metadata_test.cpp)
target_link_libraries(
  measure_coord_metadata_test
  PRIVATE casacore_mini_core casacore_mini_project_options)
target_compile_definitions(
  measure_coord_metadata_test
  PRIVATE CASACORE_MINI_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_test(NAME measure_coord_metadata_test COMMAND measure_coord_metadata_test)

add_executable(keyword_record_test tests/keyword_record_test.cpp)
target_link_libraries(
  keyword_record_test
  PRIVATE casacore_mini_core casacore_mini_project_options)
target_compile_definitions(
  keyword_record_test
  PRIVATE CASACORE_MINI_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
add_test(NAME keyword_record_test COMMAND keyword_record_test)

add_executable(table_schema_cli src/table_schema_cli.cpp)
target_link_libraries(table_schema_cli PRIVATE casacore_mini_core casacore_mini_project_options)

if(CASACORE_MINI_ENABLE_DOXYGEN)
  find_package(Doxygen QUIET)
  if(DOXYGEN_FOUND)
    set(DOXYGEN_OUTPUT_DIR "${CMAKE_BINARY_DIR}/docs/doxygen")
    configure_file("${CMAKE_SOURCE_DIR}/docs/Doxyfile.in" "${CMAKE_BINARY_DIR}/Doxyfile" @ONLY)

    add_custom_target(
      doc
      COMMAND "${CMAKE_COMMAND}" -E make_directory "${DOXYGEN_OUTPUT_DIR}"
      COMMAND "${DOXYGEN_EXECUTABLE}" "${CMAKE_BINARY_DIR}/Doxyfile"
      WORKING_DIRECTORY "${CMAKE_BINARY_DIR}"
      COMMENT "Generating API documentation with Doxygen"
      VERBATIM)
  else()
    message(STATUS "Doxygen not found; doc target is disabled")
  endif()
endif()
