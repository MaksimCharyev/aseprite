#!/usr/bin/env bash
# Copyright (C) 2026  Maksim Charyev
#
# Build the code generator `gen` for the HOST (ADR 0008 section 1).
#
# When cross-compiling for Android, every generation rule in
# src/app/CMakeLists.txt runs `gen` on the build machine, so it cannot be the
# aarch64 binary. This script builds it in <repo>/build/host-tools (git-ignored)
# with the host compiler; configure-android.sh then passes
# -DGEN_EXE=<repo>/build/host-tools/bin/gen (hook in src/CMakeLists.txt).
#
# Idempotent and incremental, can be run from any directory.
#
# Env (defaults come from android/scripts/env.sh):
#   HOST_CC / HOST_CXX  host compilers, default gcc / g++
#   HOST_TOOLS_DIR      build tree, default <repo>/build/host-tools
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(cd "$here/../.." && pwd)"
# shellcheck source=env.sh
source "$here/env.sh"

build="${HOST_TOOLS_DIR:-$repo/build/host-tools}"
cc="${HOST_CC:-gcc}"
cxx="${HOST_CXX:-g++}"

for tool in cmake ninja "$cc" "$cxx"; do
  command -v "$tool" >/dev/null || {
    echo "build-host-gen: '$tool' not found (see docs/mobile/BUILDING.md)" >&2
    exit 1
  }
done
[[ -f "$repo/laf/CMakeLists.txt" && -f "$repo/laf/clip/CMakeLists.txt" ]] || {
  echo "Submodules missing: git submodule update --init --recursive" >&2
  exit 1
}

# Do not let a stray Android toolchain setting leak into the host build.
unset CMAKE_TOOLCHAIN_FILE ANDROID_ABI ANDROID_PLATFORM

cmake -G Ninja -S "$repo" -B "$build" \
  -DCMAKE_C_COMPILER="$cc" \
  -DCMAKE_CXX_COMPILER="$cxx" \
  -DCMAKE_BUILD_TYPE=Release \
  -DLAF_BACKEND=none \
  -DENABLE_UPDATER=OFF \
  -DENABLE_NEWS=OFF \
  -DENABLE_DRM=OFF \
  -DENABLE_STEAM=OFF \
  -DENABLE_SENTRY=OFF \
  -DENABLE_DESKTOP_INTEGRATION=OFF \
  -DENABLE_WEBSOCKET=OFF \
  -DENABLE_TESTS=OFF \
  -DENABLE_CCACHE=OFF \
  -DLAF_WITH_EXAMPLES=OFF \
  -DLAF_WITH_TESTS=OFF

cmake --build "$build" --target gen

gen="$build/bin/gen"
[[ -x "$gen" ]] || { echo "build-host-gen: $gen was not produced" >&2; exit 1; }
echo "host gen: $gen"
