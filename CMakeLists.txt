cmake_minimum_required(VERSION 3.3)
project(mucom88 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories("src")
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
  add_definitions("-D__stdcall=")
endif()

if(CMAKE_C_COMPILER_ID MATCHES "^(GNU|Clang)$")
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang)$")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
endif()

add_library(mucom88-fmgen STATIC
  "src/fmgen/fmgen.cpp"
  "src/fmgen/fmtimer.cpp"
  "src/fmgen/opm.cpp"
  "src/fmgen/opna.cpp"
  "src/fmgen/psg.cpp")

add_library(mucom88-z80 STATIC
  "src/Z80/Z80.cpp")

add_executable(mucom88
  "src/adpcm.cpp"
  "src/cmucom.cpp"
  "src/main.cpp"
  "src/md5.c"
  "src/membuf.cpp"
  "src/mucomerror.cpp"
  "src/mucomvm.cpp"
  "src/plugin.cpp"
  "src/realchip.cpp"
  "src/soundbuf.cpp"
  "src/soundrt.cpp")

target_link_libraries(mucom88 PRIVATE mucom88-fmgen mucom88-z80)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  target_link_libraries(mucom88 PRIVATE dl)
endif()

find_library(UV_LIBRARY "uv")
if(NOT UV_LIBRARY)
  message(FATAL_ERROR "cannot find libuv")
endif()
target_link_libraries(mucom88 PRIVATE "${UV_LIBRARY}")

find_library(SAMPLERATE_LIBRARY "samplerate")
if(NOT SAMPLERATE_LIBRARY)
  message(FATAL_ERROR "cannot find libsamplerate")
endif()
target_link_libraries(mucom88 PRIVATE "${SAMPLERATE_LIBRARY}")

set(RtAudio_SOURCE_DIR "${PROJECT_SOURCE_DIR}/src/rtaudio")
include("RtAudio.cmake")
target_link_libraries(mucom88 PRIVATE RtAudio)
