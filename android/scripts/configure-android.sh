#!/usr/bin/env bash
# Copyright (C) 2026  Maksim Charyev
#
# Configure the root Aseprite CMake project for Android arm64-v8a (M0.4).
# Idempotent, can be run from any directory.
#
# Env (defaults come from android/scripts/env.sh):
#   ANDROID_NDK_HOME  NDK r26b (26.1.10909125)
#   SKIA_DIR          Skia checkout, default $HOME/deps/skia-m124-android
#   ANDROID_BUILD_DIR build tree, default <repo>/build/android (git-ignored)
#   ANDROID_PLATFORM  default 29 (ADR 0004)
#   CMAKE_BUILD_TYPE  default RelWithDebInfo (the root CMakeLists default)
#
# Extra arguments are passed to cmake, e.g. -DCMAKE_BUILD_TYPE=Debug.
# Then: cmake --build build/android --target doc-lib render-lib fixmath-lib filters-lib
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(cd "$here/../.." && pwd)"
# shellcheck source=env.sh
source "$here/env.sh"

build="${ANDROID_BUILD_DIR:-$repo/build/android}"
toolchain="$ANDROID_NDK_HOME/build/cmake/android.toolchain.cmake"

[[ -f "$toolchain" ]] || {
  echo "NDK toolchain file not found: $toolchain (see docs/mobile/BUILDING.md)" >&2
  exit 1
}
[[ -f "$SKIA_DIR/out/Release-arm64/libskia.a" ]] || {
  echo "Skia not built: run android/scripts/build-skia.sh" >&2
  exit 1
}
[[ -f "$repo/laf/CMakeLists.txt" && -f "$repo/laf/clip/CMakeLists.txt" ]] || {
  echo "Submodules missing: git submodule update --init --recursive" >&2
  exit 1
}

cmake -G Ninja -S "$repo" -B "$build" \
  -DCMAKE_TOOLCHAIN_FILE="$toolchain" \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_PLATFORM="${ANDROID_PLATFORM:-29}" \
  -DANDROID_STL=c++_static \
  -DLAF_BACKEND=skia \
  -DSKIA_DIR="$SKIA_DIR" \
  -DSKIA_LIBRARY_DIR="$SKIA_DIR/out/Release-arm64" \
  -DENABLE_UPDATER=OFF \
  -DENABLE_NEWS=OFF \
  -DENABLE_DRM=OFF \
  -DENABLE_STEAM=OFF \
  -DENABLE_SENTRY=OFF \
  -DENABLE_DESKTOP_INTEGRATION=OFF \
  -DENABLE_WEBSOCKET=OFF \
  -DENABLE_CCACHE=OFF \
  -DENABLE_TESTS=OFF \
  -DLAF_WITH_EXAMPLES=OFF \
  "$@"
