# Aseprite
# Copyright (C) 2026  Maksim Charyev
#
# This program is distributed under the terms of
# the End-User License Agreement for Aseprite.

# Wrapper CMake project run by Gradle (ADR 0008 sections 2-3). The variant is
# selected with `-PnativeTarget=probe|hello_laf|aseprite`, which Gradle passes
# here as ASEPRITE_NATIVE_TARGET. Every variant ends up as libaseprite.so, the
# library AsepriteActivity loads.
#
#   probe      M0.6 input probe stub (main.cpp), does not need Skia nor laf
#   hello_laf  laf/examples/hello_laf.cpp on the laf Android backend
#   aseprite   the root Aseprite project (not linking yet, M1.8)
#
# Other cache variables passed by Gradle: SKIA_DIR, GEN_EXE.

cmake_minimum_required(VERSION 3.22)

# Gradle's default for a debug variant is Debug (-O0), too slow to judge on
# the tablet (ADR 0008 section 2). Must be set before project().
set(CMAKE_BUILD_TYPE RelWithDebInfo CACHE STRING "" FORCE)

project(aseprite-android LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(ASEPRITE_NATIVE_TARGET "probe" CACHE STRING
  "Native variant: probe, hello_laf or aseprite")
set_property(CACHE ASEPRITE_NATIVE_TARGET PROPERTY STRINGS
  probe hello_laf aseprite)

get_filename_component(ASEPRITE_ROOT_DIR
  "${CMAKE_CURRENT_LIST_DIR}/../../../../.." ABSOLUTE)

# Provided by Gradle (buildFeatures.prefab) from androidx.games:games-activity.
# It must be found before laf is added: laf-os uses the existing
# game-activity::game-activity_static target.
find_package(game-activity REQUIRED CONFIG)

if(ASEPRITE_NATIVE_TARGET STREQUAL "probe")

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

else()

  # Skia (see android/scripts/build-skia.sh).
  if(NOT SKIA_DIR)
    message(FATAL_ERROR "SKIA_DIR is not set (Gradle passes it from the "
      "SKIA_DIR env var or -PskiaDir=..., see android/scripts/env.sh)")
  endif()
  set(SKIA_LIBRARY_DIR "${SKIA_DIR}/out/Release-arm64" CACHE PATH "")

  # ADR 0003 section 5: desktop-only features off, no tests.
  set(ENABLE_UPDATER OFF CACHE BOOL "")
  set(ENABLE_NEWS OFF CACHE BOOL "")
  set(ENABLE_DRM OFF CACHE BOOL "")
  set(ENABLE_STEAM OFF CACHE BOOL "")
  set(ENABLE_SENTRY OFF CACHE BOOL "")
  set(ENABLE_DESKTOP_INTEGRATION OFF CACHE BOOL "")
  set(ENABLE_WEBSOCKET OFF CACHE BOOL "")
  set(ENABLE_CCACHE OFF CACHE BOOL "")
  set(ENABLE_TESTS OFF CACHE BOOL "")
  # laf-os links the Android glue with -u, test executables would not link.
  set(LAF_WITH_TESTS OFF CACHE BOOL "")
  set(LAF_BACKEND skia CACHE STRING "")

  if(ASEPRITE_NATIVE_TARGET STREQUAL "hello_laf")

    # Only laf, not the whole Aseprite tree: no GEN_EXE needed and a much
    # shorter configure/build.
    set(LAF_WITH_EXAMPLES ON CACHE BOOL "")
    add_subdirectory("${ASEPRITE_ROOT_DIR}/laf" laf)

    # laf_add_example() already builds hello_laf as a SHARED library on
    # Android; give it the name the activity loads.
    set_target_properties(hello_laf PROPERTIES OUTPUT_NAME aseprite)

  elseif(ASEPRITE_NATIVE_TARGET STREQUAL "aseprite")

    if(NOT GEN_EXE)
      message(FATAL_ERROR "GEN_EXE is not set: run android/scripts/build-host-gen.sh "
        "(Gradle does it for the aseprite variant)")
    endif()
    set(LAF_WITH_EXAMPLES OFF CACHE BOOL "")
    add_subdirectory("${ASEPRITE_ROOT_DIR}" root)

  else()
    message(FATAL_ERROR "Unknown ASEPRITE_NATIVE_TARGET=${ASEPRITE_NATIVE_TARGET}")
  endif()

endif()
