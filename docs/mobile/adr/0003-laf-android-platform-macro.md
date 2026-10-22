# ADR 0003: `LAF_ANDROID` is its own platform (not `LAF_LINUX`); Aseprite builds as one shared library

- Status: Accepted
- Date: 2026-10-21
- Owner: mobile-architect

## Context

laf picks the platform in `laf/base/CMakeLists.txt:113-123`: `WIN32` →
`LAF_WINDOWS`, `APPLE` → `LAF_MACOS`, everything else → `LAF_LINUX`. With
the NDK toolchain CMake sets `UNIX` and `ANDROID`, so today Android would be
treated as Linux.

`LAF_LINUX` in practice means **X11 desktop**: 33 uses outside
`laf/os/x11` and `laf/os/skia`, e.g. the X11 system include in
`src/app/app.cpp:82`, X11 stylus detection at `src/app/app.cpp:309-311`, the
GTK/X11 file dialog (`laf/dlgs/CMakeLists.txt:12`), fontconfig
(`laf/text/skia_font_mgr.cpp:120`), X11 sources and `find_package(X11)` in
`laf/os/CMakeLists.txt:52-62,84-87,134+`, XCB clipboard
(`laf/clip/CMakeLists.txt:81`), and Skia's `SK_BUILD_FOR_UNIX` +
`SK_R32_SHIFT=16` + fontconfig in `laf/cmake/FindSkia.cmake:220-240`.

`ENABLE_ASEPRITE_EXE` builds an executable (`src/CMakeLists.txt:184-229`);
Android needs a `.so` loaded by the activity.

## Decision

1. In laf, `if(ANDROID)` comes **before** the generic Unix branch and
   defines `LAF_ANDROID` (and not `LAF_LINUX`). CONTEXT.md rule 1 already
   uses `#if LAF_ANDROID`.
2. POSIX-generic code paths that today say `LAF_LINUX` but are not X11
   specific (e.g. `laf/base/fs.cpp:211`, `laf/base/platform.cpp:17,68`,
   `laf/base/process.cpp:29`) are changed to `LAF_LINUX || LAF_ANDROID` one
   by one, as compile errors or behaviour require, never by blanket
   search-and-replace.
3. Platform-specific pieces that have no Android implementation yet use the
   existing "none" variants: `clip_none.cpp`, `os/none`, an empty file dialog
   (Aseprite's own file selector is used anyway, see
   `src/app/pref/preferences.cpp:55-65`).
4. Aseprite's Android target is a single shared library `libaseprite.so`
   built from `src/main/main.cpp` + `app-lib`, plus laf's
   `laf/os/android/main.cpp` providing `android_main`. All C++ code uses
   `c++_static` (one .so, so no ODR problem across libraries).
5. `ENABLE_UPDATER`, `ENABLE_NEWS`, `ENABLE_DRM`, `ENABLE_STEAM`,
   `ENABLE_SENTRY`, `ENABLE_DESKTOP_INTEGRATION`, `ENABLE_WEBSOCKET`,
   `ENABLE_TESTS`, `ENABLE_CCACHE`(optional) are OFF; `LAF_WITH_EXAMPLES` is
   ON only for the bring-up APK variant (ADR 0005, M1), `LAF_WITH_TESTS` OFF.

## Alternatives

- **Define both `LAF_LINUX` and `LAF_ANDROID`.** Fewer edits in `base/`,
  but every X11/GTK/fontconfig branch would need `&& !LAF_ANDROID`, which is
  more edits and easy to miss. Rejected.
- **Build an executable and launch it from Java with `ProcessBuilder`.** No
  window/surface access from a child process. Rejected.

## Consequences

- Every new `#if LAF_LINUX` added upstream needs a look when we rebase the
  laf fork (reviewer checklist item).
- Desktop builds are unaffected because the new branch is only taken when
  `ANDROID` is set by the NDK toolchain file.
