# Aseprite
# Copyright (C) 2026  Maksim Charyev
#
# This program is distributed under the terms of
# the End-User License Agreement for Aseprite.

# Stand-alone "hello" native library for the M0.5 Gradle skeleton. It is NOT
# the Aseprite root CMake project yet (that is wired in at M1).

cmake_minimum_required(VERSION 3.22)
project(aseprite-android-hello LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Provided by Gradle (buildFeatures.prefab) from androidx.games:games-activity.
find_package(game-activity REQUIRED CONFIG)

# Same name as the final Aseprite library: AsepriteActivity loads "aseprite".
add_library(aseprite SHARED main.cpp)

target_compile_options(aseprite PRIVATE -Wall -Wextra)

# The GameActivity JNI entry point lives in a static library; keep it from
# being dropped by the linker.
set_target_properties(aseprite PROPERTIES
  LINK_FLAGS "-u Java_com_google_androidgamesdk_GameActivity_initializeNativeCode")

target_link_libraries(aseprite
  game-activity::game-activity_static
  EGL
  GLESv3
  android
  log)
