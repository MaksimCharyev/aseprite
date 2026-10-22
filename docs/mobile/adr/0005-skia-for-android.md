# ADR 0005: Build Skia `aseprite-m124` for Android ourselves; RGBA, GL ES via EGL; CPU raster only for bring-up

- Status: Accepted
- Date: 2026-10-21
- Owner: mobile-architect

## Context

- laf requires Skia from branch `aseprite-m124` (`laf/README.md`), pinned to
  `m124-08a5439a6b` (`laf/misc/skia-tag.txt`), commit
  `08a5439a6be726021c1c1905d23ce298a3edc5e4` of `aseprite/skia`.
- Prebuilt packages exist only for Windows/macOS/Linux
  (release `m124-08a5439a6b` of github.com/aseprite/skia has no Android
  asset), so we must build.
- Skia's GN supports Android natively: setting `ndk=` makes
  `target_os="android"` (`gn/BUILDCONFIG.gn:57-58`); with `skia_use_gl`
  (default true) on Android it compiles
  `src/gpu/ganesh/gl/egl/GrGLMakeNativeInterface_egl.cpp` and links `EGL`
  and `GLESv2` (`BUILD.gn:975-981,1571-1580`), so laf's
  `GrGLMakeNativeInterface()` call (`laf/os/skia/skia_gl.cpp:35`) works
  once an EGL context is current.
- Pixel format: Skia defaults to **RGBA** except on Windows
  (`include/core/SkTypes.h:36-44`) and adds `SK_R32_SHIFT=16` (BGRA) only
  for `is_linux || is_fuchsia` (`BUILD.gn:38-40`). Android is not
  `is_linux`, so an Android Skia is RGBA. laf's `FindSkia.cmake:229-233`
  currently forces `SK_R32_SHIFT=16` for every `UNIX AND NOT APPLE`
  target, which would make laf/Aseprite disagree with the Skia library
  about `SkPMColor` layout.
- Fonts: laf picks a font manager per platform
  (`laf/text/skia_font_mgr.cpp:114-124`); on Android the right one is
  `SkFontMgr_New_Android()` (`skia_enable_fontmgr_android`, needs expat +
  FreeType, both default on).

## Decision

1. A script `android/scripts/build-skia.sh` (owner: build engineer) clones
   `aseprite/skia` at the pinned commit into `$HOME/deps/skia-m124-android`
   (outside the repo), runs `python3 tools/git-sync-deps` (with
   `GIT_SYNC_DEPS_SKIP_EMSDK=1`; ninja comes from the SDK CMake), then:

   ```
   bin/gn gen out/Release-arm64 --args='
     ndk="<NDK r26b path>" ndk_api=29 target_cpu="arm64"
     is_debug=false is_official_build=true
     skia_use_system_expat=false skia_use_system_icu=false
     skia_use_system_libjpeg_turbo=false skia_use_system_libpng=false
     skia_use_system_libwebp=false skia_use_system_zlib=false
     skia_use_system_freetype2=false skia_use_system_harfbuzz=false
     skia_use_freetype=true skia_use_harfbuzz=true
     skia_pdf_subset_harfbuzz=true
     skia_use_gl=true skia_use_vulkan=false
     skia_enable_fontmgr_android=true
     extra_cflags_cc=["-frtti"]'
   ninja -C out/Release-arm64 skia modules
   ```
   (RTTI on, as the macOS/Linux recipes do for laf; verify the exact arg
   list against the `aseprite/skia` README at build time and record the
   final `args.gn` in NOTES.md.)
2. `laf/cmake/FindSkia.cmake` gets an `ANDROID` branch: `SKIA_ARCH=arm64`,
   `SK_BUILD_FOR_ANDROID` instead of `SK_BUILD_FOR_UNIX`, **no**
   `SK_R32_SHIFT=16`, link `GLESv2 EGL android log` instead of `GL`
   and fontconfig.
3. Rendering path (D2): EGL context + window surface on the
   `ANativeWindow` (`os/gl/gl_context_egl.h`, implementing
   `os::GLContext` from `laf/os/gl/gl_context.h`), Skia Ganesh GL backend
   through the existing `SkiaGL` (`laf/os/skia/skia_gl.cpp`).
4. Bring-up fallback (allowed by D2): CPU raster `onPaint()` that copies the
   Skia bitmap to `ANativeWindow_lock()` buffers, the same shape as
   `SkiaWindowX11::onPaint` (`laf/os/skia/skia_window_x11.cpp:58-100`).
   Window format `WINDOW_FORMAT_RGBA_8888` matches Skia's RGBA N32, so no
   swizzle.

## Alternatives

- **Skia upstream `chrome/m124` instead of the aseprite fork:** the fork
  carries laf-specific patches; diverging would cost time. Rejected.
- **Vulkan backend:** excluded by D2.
- **Keep BGRA (`SK_R32_SHIFT=16`) on Android by passing it as a GN
  define:** possible, but adds a swizzle for `ANativeWindow` raster and GL
  readbacks and deviates from what Skia tests on Android. Rejected unless
  laf turns out to hard-code BGRA somewhere (verify in M1: a red test
  rectangle must look red on the tablet).

## Consequences

- Skia build measured at 3m20s on 12 cores, ~8.7 GB on disk with
  deps (~7.6 GB without emsdk), plus several minutes of network sync; done once per
  host, outside the repo, never committed.
- Any place in laf/Aseprite that assumes BGRA on non-Windows must be found
  during M1 (symptom: red/blue swapped).
- A Skia version bump in upstream laf means rebuilding the Android Skia.
