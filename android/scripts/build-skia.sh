#!/usr/bin/env bash
# Build aseprite/skia (branch aseprite-m124) for Android arm64-v8a.
# See docs/mobile/adr/0005-skia-for-android.md and docs/mobile/BUILDING.md.
#
# Env (defaults come from android/scripts/env.sh):
#   ANDROID_NDK_HOME  NDK r26b (26.1.10909125)
#   SKIA_DIR          checkout dir, default $HOME/deps/skia-m124-android
#   SKIA_JOBS         parallel jobs for ninja (default: all cores)
#
# Output: $SKIA_DIR/out/Release-arm64/{libskia.a,libskshaper.a,libskunicode*.a}
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo="$(cd "$here/../.." && pwd)"
# shellcheck source=env.sh
source "$here/env.sh"

SKIA_URL="https://github.com/aseprite/skia.git"
SKIA_COMMIT="08a5439a6be726021c1c1905d23ce298a3edc5e4"   # tag m124-08a5439a6b
OUT="out/Release-arm64"

# Keep in sync with laf/misc/skia-tag.txt.
want_tag="$(cat "$repo/laf/misc/skia-tag.txt" 2>/dev/null || true)"
if [[ -z "$want_tag" ]]; then
  echo "warning: $repo/laf/misc/skia-tag.txt not found (laf submodule not initialized?); skipping skia-tag check" >&2
elif [[ "$want_tag" != "m124-${SKIA_COMMIT:0:10}" ]]; then
  echo "laf/misc/skia-tag.txt says $want_tag but this script pins ${SKIA_COMMIT:0:10}" >&2
  exit 1
fi

[[ -d "$ANDROID_NDK_HOME/toolchains/llvm" ]] || {
  echo "NDK not found at ANDROID_NDK_HOME=$ANDROID_NDK_HOME (see docs/mobile/BUILDING.md)" >&2
  exit 1
}

# 1. Checkout (outside the repository).
if [[ ! -d "$SKIA_DIR/.git" ]]; then
  mkdir -p "$(dirname "$SKIA_DIR")"
  git clone --no-checkout "$SKIA_URL" "$SKIA_DIR"
fi
cd "$SKIA_DIR"
git cat-file -e "$SKIA_COMMIT^{commit}" 2>/dev/null || git fetch origin "$SKIA_COMMIT"
git checkout -q "$SKIA_COMMIT"

# 2. Third-party deps and gn. Skia's python scripts want python3.
# git-sync-deps downloads ~7 GB and sometimes fails on a flaky thread; retry
# once. emsdk (1.1 GB, wasm only) is skipped. A stamp file marks success.
stamp="third_party/externals/.synced-$SKIA_COMMIT"
if [[ ! -f "$stamp" ]]; then
  export GIT_SYNC_DEPS_SKIP_EMSDK=1
  python3 tools/git-sync-deps || python3 tools/git-sync-deps
  touch "$stamp"
fi
[[ -x bin/gn ]] || python3 bin/fetch-gn
# ninja comes from the SDK CMake (env.sh puts it on PATH).
NINJA=ninja
command -v "$NINJA" >/dev/null || { echo "ninja not found; install cmake;3.31.6 via sdkmanager (docs/mobile/BUILDING.md)" >&2; exit 1; }

# 3. Configure. Flags follow the aseprite/skia README (Linux/macOS recipe)
#    plus the Android bits from ADR 0005. No SK_R32_SHIFT: Android stays RGBA.
ARGS=(
  "ndk=\"$ANDROID_NDK_HOME\""
  'ndk_api=29'
  'target_os="android"'
  'target_cpu="arm64"'
  'is_debug=false'
  'is_official_build=true'
  'skia_use_system_expat=false'
  'skia_use_system_icu=false'
  'skia_use_system_libjpeg_turbo=false'
  'skia_use_system_libpng=false'
  'skia_use_system_libwebp=false'
  'skia_use_system_zlib=false'
  'skia_use_system_freetype2=false'
  'skia_use_system_harfbuzz=false'
  'skia_use_freetype=true'
  'skia_use_harfbuzz=true'
  'skia_pdf_subset_harfbuzz=true'
  'skia_use_gl=true'
  'skia_use_vulkan=false'
  'skia_enable_fontmgr_android=true'
  'extra_cflags_cc=["-frtti"]'
)
bin/gn gen "$OUT" --args="${ARGS[*]}"

# 4. Build.
"$NINJA" -C "$OUT" ${SKIA_JOBS:+-j "$SKIA_JOBS"} skia modules

echo
echo "Skia built in $SKIA_DIR/$OUT:"
ls -la "$OUT"/lib*.a
