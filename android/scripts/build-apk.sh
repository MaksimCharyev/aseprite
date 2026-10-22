#!/usr/bin/env bash
# Copyright (C) 2026  Maksim Charyev
#
# Build the debug APK of one native variant and keep a copy per variant in
# <repo>/build/apk/aseprite-<variant>.apk (git-ignored), so several variants
# can be installed one after another without rebuilding.
#
# Usage: android/scripts/build-apk.sh [probe|hello_laf|aseprite]   (default: probe)
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(cd "$here/../.." && pwd)"
# shellcheck source=env.sh
source "$here/env.sh"

variant="${1:-probe}"
"$repo/android/gradlew" -p "$repo/android" :app:assembleDebug "-PnativeTarget=$variant"
mkdir -p "$repo/build/apk"
cp "$repo/android/app/build/outputs/apk/debug/app-debug.apk" "$repo/build/apk/aseprite-$variant.apk"
ls -l "$repo/build/apk/aseprite-$variant.apk"
