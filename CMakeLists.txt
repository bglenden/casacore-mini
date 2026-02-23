cmake_minimum_required(VERSION 3.27)

project(casacore_mini VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CASACORE_MINI_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(CASACORE_MINI_ENABLE_CLANG_TIDY "Enable clang-tidy linting" OFF)
option(CASACORE_MINI_ENABLE_COVERAGE "Enable coverage instrumentation" OFF)

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

add_library(casacore_mini_core src/version.cpp)
target_include_directories(
  casacore_mini_core
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(casacore_mini_core PUBLIC casacore_mini_project_options)

enable_testing()
add_executable(version_test tests/version_test.cpp)
target_link_libraries(version_test PRIVATE casacore_mini_core casacore_mini_project_options)
add_test(NAME version_test COMMAND version_test)
