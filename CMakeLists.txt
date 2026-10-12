cmake_minimum_required(VERSION 3.18)
project(DualSenseEngine VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(DS_BUILD_SHARED "Build a shared library (always shared on Android)" ON)
option(DS_BUILD_TESTS  "Build host unit tests (ignored on Android)" ON)

set(DS_SOURCES src/dualsense_engine.cpp)
if(ANDROID)
  list(APPEND DS_SOURCES bindings/android/dualsense_jni.cpp)
endif()

if(ANDROID OR DS_BUILD_SHARED)
  add_library(DualSenseEngine SHARED ${DS_SOURCES})
else()
  add_library(DualSenseEngine STATIC ${DS_SOURCES})
endif()

target_include_directories(DualSenseEngine PUBLIC include)
target_compile_options(DualSenseEngine PRIVATE -Wall -Wextra)

if(ANDROID)
  # Required for apps on Android 15+ devices with 16 KB memory pages.
  target_link_options(DualSenseEngine PRIVATE -Wl,-z,max-page-size=16384)
endif()

if(NOT ANDROID AND DS_BUILD_TESTS)
  enable_testing()
  add_executable(ds_test tests/test_engine.cpp src/dualsense_engine.cpp)
  target_include_directories(ds_test PRIVATE include)
  add_test(NAME ds_test COMMAND ds_test)
endif()
