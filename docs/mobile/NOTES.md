# Aseprite Mobile — research notes

Facts about the device, platform and toolchain that agents need. Every item
says whether it is **verified on the user's device**, **documented**
(official source), or **reported** (third party). Add new findings at the
end of the relevant section with a date and a source.

Last architect pass: 2026-10-21.

---

## 1. Repository and host state (verified on the build host, 2026-10-21)

- Submodules are **already initialized** on branch `mobile/setup`
  (`git submodule status` shows `laf` at `500768c`, plus all of
  `third_party/*`, `src/{flic,observable,psd,tga,undo}`).
- `laf/` remotes: `origin` = https://github.com/MaksimCharyev/laf.git,
  `upstream` = https://github.com/aseprite/laf.git (matches D3).
- Android toolchain installed 2026-10-21 (M0.1), user-local, see
  `docs/mobile/BUILDING.md`: JDK 17 `~/Android/jdk-17`, SDK
  `~/Android/Sdk` (platform 35, build-tools 35.0.0, NDK r26b, CMake 3.31.6
  with Ninja 1.12.1, platform-tools 37.0.1). Env via
  `android/scripts/env.sh` (not in any shell rc). No system CMake/Ninja.
- Skia pinned by laf: `laf/misc/skia-tag.txt` = `m124-08a5439a6b`
  (branch `aseprite-m124`, commit `08a5439a6be726021c1c1905d23ce298a3edc5e4`,
  2024-06-11).
- C++17 (`CMakeLists.txt:7`, `laf/CMakeLists.txt:15`); CMake >= 3.20
  (`CMakeLists.txt:5`).

## 2. Device: Android API level reported by HarmonyOS 4.x

Status: **not verified on the user's device.** Update: see §9, verified 2026-10-21 (API 31, HarmonyOS 4.3.0).

- Reported: HarmonyOS **4.2.0** on a MatePad 12X 2024 shows **Android 12**
  (API 31) — Krita Artists thread, 2026-04:
  https://krita-artists.org/t/double-tap-gesture-on-huawei-m-pencil-3/178496
- Reported: HarmonyOS 3.0 was listed with Android 12 / API 31 compatibility:
  https://www.huaweicentral.com/harmonyos-3-0-is-compatible-with-android-12/amp/
- No official Huawei statement found for HarmonyOS 4.x
  (https://en.wikipedia.org/wiki/HarmonyOS_version_history).
- Working assumption: API 31. `minSdk 29` (ADR 0004) leaves margin.

**User check** (USB debugging on, `adb devices` shows the tablet):

```
adb shell getprop ro.build.version.sdk
adb shell getprop ro.build.version.release
adb shell getprop ro.build.version.security_patch
adb shell getprop ro.product.model
adb shell getprop ro.product.cpu.abilist
adb shell getprop ro.huawei.build.display.id
adb shell getprop hw_sc.build.platform.version
adb shell getprop ro.opengles.version
adb shell getprop ro.hardware.egl
adb shell wm size
adb shell wm density
adb shell dumpsys SurfaceFlinger | grep -iE "GLES|refresh|fps" | head -20
adb shell dumpsys display | grep -iE "mRefreshRate|supportedModes|renderFrameRate" | head -20
adb shell dumpsys input | grep -iE "Device [0-9]+:|Sources|stylus|pen|touch" | head -60
```

(`ro.opengles.version` 196610 = GLES 3.2, 196608 = 3.0. Some Huawei props
may be empty; that is fine — paste everything.)

## 3. M-Pencil: side button and double-tap

Status: **verified on the device 2026-10-22** (§9 "M0.6 input probe on the device"): Android `keyCode=718`, `scanCode=190`, two DOWN/UP pairs per double-tap. The research below is kept for history.

- **User's pen (confirmed 2026-10-21): M-Pencil 3rd gen** — no side
  button; double-tap on the barrel is the only pen gesture.

- Pen generations: M-Pencil 2nd and 3rd gen have **no physical side
  button**; their special control is a **double-tap on the barrel**. The
  M-Pencil Pro adds a button/squeeze. The MatePad 11.5" PaperMatte is sold
  with/for the M-Pencil 3rd gen.
  https://www.parkablogs.com/content/huawei-matepad-11-5-s-2026-review ,
  https://www.notebookcheck.org/Huawei-MatePad-11-5-PaperMatte-Edition-se-lanza-con-una-nueva-pantalla-antirreflejos-y-compatibilidad-avanzada-con-M-Pencil-2.751446.0.html ,
  https://consumer.huawei.com/ph/support/content/en-us15819784
- Huawei says the double-tap "is open to third-party apps", naming GoPaint,
  TouchNotes, Jnotes; no developer API documented on that page:
  https://consumer.huawei.com/nz/support/content/en-us16085408/
- **Key finding (reported):** on HarmonyOS 4.2 (Android 12), M-Pencil 3
  double-tap reaches a plain Android app as a **`KeyEvent` with key code
  718** (not defined in AOSP; Android only defines up to F24 etc.). One
  double-tap produced two down/up pairs. Drawpile patched Qt to map it to
  F25 so users can bind it to "eraser":
  https://krita-artists.org/t/double-tap-gesture-on-huawei-m-pencil-3/178496 ,
  https://invent.kde.org/szaman/qtbase/-/merge_requests/17
- It may arrive only while the pen is hovering (Huawei Pen Kit docs for
  HarmonyOS NEXT say double-tap events need hover; reported, ArkTS only):
  https://www.besthub.dev/articles/harmonyos-pen-kit-integrating-stylus-squeeze-double-tap-color-picker-3ffe65193881
- **Huawei Pen Kit** (`@kit.Penkit`) is an ArkTS/HarmonyOS NEXT API; no
  Android/HMS SDK for pen gestures was found. Not usable for our APK.
- GameActivity forwards every `onKeyDown/onKeyUp` to native; the default
  native key filter only drops volume/camera/zoom keys, so 718 will reach
  laf (source: `GameActivity.java` `onKeyDown`, `default_key_filter` in
  `android_native_app_glue.c`,
  https://android.googlesource.com/platform/frameworks/opt/gamesdk/+/refs/heads/main/game-activity/).
- If a pen with a barrel button is used, Android reports it as
  `BUTTON_STYLUS_PRIMARY` (`AMOTION_EVENT_BUTTON_STYLUS_PRIMARY = 1<<5`) in
  `buttonState`, with `ACTION_BUTTON_PRESS/RELEASE` (11/12):
  https://developer.android.com/ndk/reference/group/input
- Huawei may consume the double-tap system-wide if the system setting
  "double-tap to switch" is bound to something global — check the setting
  under Settings > Accessibility/Smart features > Stylus (exact path
  varies) during the probe test.

Plan: ADR 0006 (virtual key `PenDoubleTap` → `ToggleEraser`), with an
on-screen eraser toggle as fallback. **The M0 input-probe APK verifies this
before any implementation.**

**User check** (after M0.6 probe APK is installed; also works today with any
app focused, but logs are clearer with the probe):

```
adb logcat -c
adb logcat -v time | grep -iE "AsepriteProbe|Unhandled key|keycode|KeyEvent"
```
Double-tap the pen barrel 3 times while hovering, then 3 times while not
near the screen; paste the output.

## 4. Palm rejection signals

Status: **verified on the device 2026-10-22** (§9 "M0.6 input probe on the device"): no `TOOL_TYPE_PALM`, no `FLAG_CANCELED`; palm = several FINGER pointers, then the system sends `ACTION_CANCEL` and swallows the palm. The research below is kept for history.

- `getToolType()`: `TOOL_TYPE_FINGER` (1), `TOOL_TYPE_STYLUS` (2),
  `TOOL_TYPE_MOUSE` (3), `TOOL_TYPE_ERASER` (4). In the NDK,
  `AMOTION_EVENT_TOOL_TYPE_PALM = 5` exists; in Java `TOOL_TYPE_PALM` is
  hidden. Whether HarmonyOS ever reports 5 is unknown.
  https://developer.android.com/ndk/reference/group/input
- `ACTION_CANCEL` (API 1): sent on palm rejection and navigation gestures;
  the app should drop the stroke made by that pointer.
  `FLAG_CANCELED` (API **33**) on the pointer-up marks an accidental touch —
  **not available on an API 31 device**.
  https://developer.android.com/develop/ui/views/touch-and-input/stylus-input/advanced-stylus-features
- Hover: `ACTION_HOVER_ENTER/MOVE/EXIT` with `AXIS_DISTANCE`; values are
  vendor-specific, use only as "pen is near".
  (same page)
- Touch size: `AXIS_TOUCH_MAJOR/MINOR`, `AXIS_SIZE` per pointer, usable as
  a large-contact heuristic.
- GameActivity passes all of these to native: `GameActivityMotionEvent`
  has `source`, `action`, `flags`, `buttonState`, `actionButton`,
  per-pointer `toolType` and axis values, and the historical samples
  (`GameActivityEvents.h`). Its default motion filter keeps only events
  with `source & SOURCE_TOUCHSCREEN` — stylus (0x5002 on this device) passes because of the
  shared pointer class bit, but we will install our own filter anyway
  (ADR 0001).
- Optional later: Jetpack low-latency front-buffer rendering (API 29+) and
  motion prediction — not needed for v1.

Plan (ADR 0006): stylus-priority rejection (drop fingers while pen is
active or recently active), always drop `TOOL_TYPE_PALM`, cancel stroke on
`ACTION_CANCEL`, optional "draw with finger" preference.

**User check** (probe APK): rest the palm on the screen while drawing with
the pen; then touch with one finger, two fingers; paste
`adb logcat -v time -s AsepriteProbe:V`.

### M0.6 probe APK (2026-10-22, read from the GameActivity 4.4.2 jar/headers)

- **GameActivity reports only `AXIS_X`/`AXIS_Y` by default.** Every other
  axis (pressure, size, touch/tool major/minor, orientation, tilt,
  distance) reads as 0 unless enabled with
  `GameActivityPointerAxes_enableAxis()` (global, call once before input
  arrives; `GameActivityEvents.h`). The laf backend (M2.1) must do this.
  The probe enables all 48 axes and logs unknown non-zero ones as `other:`.
- Motion and key filters are replaced with lambdas returning `true`
  (`android_app_set_motion_event_filter` / `_key_event_filter`). With all
  keys accepted, `onKeyDownNative` consumes Back and volume keys.
- Hover path: GameActivity installs only `OnTouchListener`,
  `OnGenericMotionListener` and `OnCapturedPointerListener` on its
  `SurfaceView` (all call `processMotionEvent`). `ACTION_HOVER_*` is
  dispatched through `View.dispatchHoverEvent()` first and reaches the
  generic listener only by falling through (the `SurfaceView` is not
  hoverable). `AsepriteActivity` installs an `OnHoverListener` that calls
  `processMotionEvent()` and returns `true` (so each hover event is sent once).
  Keep this in the real activity.
- `AsepriteActivity.dispatchKeyEvent` logs every `KeyEvent` reaching the
  activity (Java side, same tag, prefix `java key`), before GameActivity
  forwards it; this tells whether the pen double-tap reaches the app at all.
- `APP_CMD_KEY_EVENT` (19) / `APP_CMD_TOUCH_EVENT` (20) are only wake-ups
  for the glue loop (the "APP_CMD 20" seen in M0.5).
- Startup also logs every `InputDevice` (`getDeviceIds()`): name, sources,
  vendor/product, `hasKeys(718)`, motion ranges.

## 5. GameActivity vs NativeActivity

Status: **documented.** Decision in ADR 0001 (GameActivity).

- GameActivity = descendant of NativeActivity that uses a `SurfaceView`,
  extends `AppCompatActivity`, uses its own input buffers instead of
  `AInputQueue`, and adds IME through GameTextInput:
  https://developer.android.com/games/agdk/game-activity ,
  https://developer.android.com/games/agdk/game-activity/migrate-native-activity
- Jetpack `androidx.games:games-activity`, stable **4.4.2** (2026-05-06);
  4.3.0 added mouse support, 4.4.0 ANR protection and `ALooper_pollOnce`:
  https://developer.android.com/jetpack/androidx/releases/games
- Library's own Gradle: `minSdkVersion 21`, built with NDK 23.
- Laf already has the IME hook we need: `os::System::setTextInput()`
  (`laf/os/system.h:205`), called by `ui::Entry` (`src/ui/entry.cpp:170,301,317`)
  and `ui::TextEdit` (`src/ui/textedit.cpp:478,480`).

## 6. Skia for Android

Status: **documented / read from source.** Decision in ADR 0005.

- laf requires branch `aseprite-m124` (`laf/README.md`), tag
  `m124-08a5439a6b` (`laf/misc/skia-tag.txt`).
- Prebuilt packages: Windows/macOS/Linux only, no Android:
  https://github.com/aseprite/skia/releases/tag/m124-08a5439a6b
- Build recipe (desktop): https://github.com/aseprite/skia#readme —
  `depot_tools`, `python3 tools/git-sync-deps`, `python3 bin/fetch-ninja`,
  `is_official_build=true`, all `skia_use_system_*=false`, FreeType +
  HarfBuzz on.
- Skia m124 pins **NDK r26b** for its Android bots
  (`infra/bots/assets/android_ndk_linux/create.py`); `ndk=` arg sets
  `target_os="android"`; default `ndk_api=21`
  (`gn/BUILDCONFIG.gn`), https://skia.org/docs/user/build/
- On Android with `skia_use_gl=true` Skia builds the EGL native interface
  and links `EGL`/`GLESv2` (`BUILD.gn:975-981`).
- **Pixel format pitfall:** Skia is RGBA by default on non-Windows
  (`include/core/SkTypes.h:36-44`) and BGRA only for
  `is_linux || is_fuchsia` (`BUILD.gn:38-40`). laf's
  `cmake/FindSkia.cmake:229-233` forces `SK_R32_SHIFT=16` for
  `UNIX AND NOT APPLE`, which would be wrong for Android. Must be guarded.
- laf font manager per platform is chosen in
  `laf/text/skia_font_mgr.cpp:114-124`; Android needs
  `SkFontMgr_New_Android()` (Skia `skia_enable_fontmgr_android`).

### Built 2026-10-21 (M0.2)

`android/scripts/build-skia.sh` produced `~/deps/skia-m124-android/out/Release-arm64`
(`libskia.a` 31 MB, `libskshaper.a`, `libskunicode.a` 20 MB with ICU,
`libskparagraph.a`, `libskottie.a`, harfbuzz/freetype/png/webp/jpeg/zlib/expat
as separate `.a`); `file` on an extracted object: `ELF 64-bit LSB relocatable,
ARM aarch64`. Build: 3m20s on 12 cores. Final `args.gn`:

```
ndk = "/home/max/Android/Sdk/ndk/26.1.10909125"
ndk_api = 29
target_os = "android"
target_cpu = "arm64"
is_debug = false
is_official_build = true
skia_use_system_expat = false
skia_use_system_icu = false
skia_use_system_libjpeg_turbo = false
skia_use_system_libpng = false
skia_use_system_libwebp = false
skia_use_system_zlib = false
skia_use_system_freetype2 = false
skia_use_system_harfbuzz = false
skia_use_freetype = true
skia_use_harfbuzz = true
skia_pdf_subset_harfbuzz = true
skia_use_gl = true
skia_use_vulkan = false
skia_enable_fontmgr_android = true
extra_cflags_cc = [ "-frtti" ]
```

- Same as ADR 0005 plus `target_os="android"` explicit; `bin/gn` is not
  fetched by `git-sync-deps` here, the script runs `bin/fetch-gn`.
  `bin/fetch-ninja` is not needed, the SDK CMake ninja works.
- `git-sync-deps` pulls emsdk (1.1 GB, unused) unless
  `GIT_SYNC_DEPS_SKIP_EMSDK=1` is set (tools/git-sync-deps:260; the script
  sets it, and delete `third_party/externals/emsdk` if an earlier sync left
  it); one run failed with
  "Thread failure detected" (network) and succeeded on retry, the script
  retries once.
- No `SK_R32_SHIFT` anywhere in the Skia build (Android is RGBA).
- Link set for laf (M0.3): libskia + skshaper + skunicode + skparagraph if
  needed, plus freetype/harfbuzz/expat/png/webp/jpeg/zlib `.a` files.

### laf `LAF_ANDROID` build (M0.3, 2026-10-22)

Work is on laf branch `mobile/laf-android` (from `500768c`), **uncommitted**.
Build tree: `build/laf-android` (outside laf). Result: `laf-base laf-gfx
laf-ft laf-text laf-os` (+ `clip`) build for `aarch64-none-linux-android29`
with `LAF_BACKEND=skia`; compile lines have `-DLAF_ANDROID
-DSK_BUILD_FOR_ANDROID` and no `SK_R32_SHIFT`. A throw-away shared library
linking `laf-os laf-text laf-ft laf-gfx laf-base` with
`-Wl,--no-undefined` links fine (`/tmp/lafprobe`, not kept), so the
Skia link set in `FindSkia.cmake` is complete for what laf uses.

Patches (file: what):

- `base/CMakeLists.txt`: `ANDROID` branch before the Unix one defines
  `LAF_ANDROID` (not `LAF_LINUX`), links `log`; `platform_unix.cpp` is not
  built (it is the Linux distro parser and needs `<map>` that only
  `platform.h` includes under `LAF_LINUX`). `platform.cpp` therefore returns
  an empty `Platform` on Android; `Platform::os` is still `OS::Linux`
  (no `OS::Android` yet, to avoid touching Aseprite callers).
- `base/fs.cpp`, `base/process.cpp`: `LAF_LINUX || LAF_ANDROID` (case
  sensitive paths, `/proc/<pid>/exe`, includes). Not touched on purpose:
  `base/thread_tests.cpp`, `platform.h` (X/distro bits), `os/tablet_options.h`,
  `os/system.h` (X11).
- `base/log.cpp`: under `LAF_ANDROID` also `__android_log_write` with tag
  `laf`; `LOGva()` got a `level` argument for the priority.
  Capture with `adb logcat -s laf`.
- `cmake/FindSkia.cmake`: `SKIA_ARCH=arm64` for `ANDROID`
  (fatal error for other ABIs); `GLESv2` + `EGL`; `SK_BUILD_FOR_ANDROID`;
  no `SK_R32_SHIFT=16`, no fontconfig; `--start-group` kept; extra static libs
  `zlib` (Skia's), `expat`, `webp`, `jpeg`, plus `log`, `android`.
- `os/CMakeLists.txt`: Android uses `os/none/system.cpp` + new
  `os/android/event_queue.{h,cpp}` (a small mutex/condvar queue, because
  `common/event_queue.cpp` needs an `EventQueueImpl` and `CommonSystem` uses
  it); no X11 sources/libs; the Skia-backend OS sources (`os/skia/*`) are not
  built on Android yet. `os/common/system.cpp` skips `makeSkia()` on Android
  for the same reason. Both were reverted in M1.3 (laf `3bb409f`).
- `text/skia_font_mgr.cpp`: `SkFontMgr_New_Android(nullptr)`
  (`include/ports/SkFontMgr_android.h`).
- `dlgs/CMakeLists.txt`: no sources on Android; `FileDialog::make()` already
  returns `nullptr` for unknown platforms.
- `clip/CMakeLists.txt` (nested submodule `laf/clip`): Android uses
  `clip_none.cpp`, otherwise it fails on `xcb/xcb.h`. Committed on branch
  `mobile/android` of the user's clip fork
  (https://github.com/MaksimCharyev/clip; `laf/.gitmodules` points there).
  Push the clip commit **before** the laf commit that bumps it, otherwise a
  fresh `git submodule update --init --recursive` fails on a missing commit.

Gotchas:

- The NDK toolchain sets `CMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY`, so
  `find_library(... PATH <skia dir>)` silently fails. `FindSkia.cmake`
  switches it to `BOTH` on Android and restores it at the end of the file.
- `FindSkia.cmake` downloads/builds its own zlib with `ExternalProject`
  (host build, no toolchain) when `ZLIB_LIBRARIES` is unset. On Android the
  NDK sysroot `libz` is used. FreeType inside `libskia.a` calls plain
  `inflate*`, while Skia's `libzlib.a` is Chromium-prefixed (`Cr_z_*`,
  needed by Skia's libpng), so **both** are needed.
- `os/dnd.cpp` references `os::default_decode_png/jpg/...` (defined only in
  `os/win/dnd.cpp`) when `CLIP_ENABLE_IMAGE`; with `--whole-archive` on
  `laf-os` the link fails. Same on desktop Linux, only hidden by lazy
  archive linking; do not use `--whole-archive` for `liblaf-os.a`.
- NDK cmake prints `Compatibility with CMake < 3.10` deprecation warnings from
  `android.toolchain.cmake` itself (NDK r26b, not our code).
- Desktop FindSkia output for Linux was compared old vs new by the
  reviewer (link libraries, options, definitions, includes): identical.
- Not verified: desktop compile with the real Skia backend (no desktop Skia
  on this host); desktop checked with `LAF_BACKEND=none`, tests ON,
  `ctest` 34/34 passed (host gcc, SDK cmake 3.31.6 + ninja).
- Linux/Android behaviour: runtime is not verified (no device run).

### Root project cross-build, non-GUI libs (M0.4, 2026-10-22)

Branch `mobile/m0-libs` (from `mobile/setup`), **uncommitted**. No laf change
was needed. Configure with `android/scripts/configure-android.sh` (build tree
`build/android`, git-ignored): NDK r26b toolchain, `arm64-v8a`,
`ANDROID_PLATFORM=29`, `c++_static`, `LAF_BACKEND=skia`, ADR 0003 section 5
OFF options plus `ENABLE_CCACHE=OFF`, `ENABLE_TESTS=OFF`,
`LAF_WITH_EXAMPLES=OFF`. Configure takes about 45 s the first time.

Result: `doc-lib render-lib fixmath-lib filters-lib` build for
`aarch64-none-linux-android29` in 23 s on 12 cores (160 steps, no warnings
from our code), together with `cityhash`, `zlibstatic`, `laf-base`,
`laf-gfx`. Also built and linking: `giflib cmark fmt json11 lua lualib lauxlib
tinyxml2 tinyexpr pixman archive_static TinyEXIFstatic png_static dio-lib
cfg-lib flic-lib tga-lib undo obs view-lib laf-os laf-text laf-ft laf-dlgs
clip`. (`libdoc-lib.a` is 30 MB with `-g`.)

Patches (shared files, desktop behaviour unchanged):

- `CMakeLists.txt` (libwebp lookup for the Skia backend) and
  `cmake/FindJpegTurbo.cmake`: `find_library(... NO_DEFAULT_PATH)` into
  `SKIA_LIBRARY_DIR` silently failed under the NDK toolchain
  (`CMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY`, same gotcha as in
  `FindSkia.cmake`), giving `WEBP_LIBRARIES-NOTFOUND` /
  `LIBJPEG_TURBO_LIBRARY-NOTFOUND` (webp and jpeg would then be silently
  dropped from `app-lib`). Fixed with `NO_CMAKE_FIND_ROOT_PATH` on those two
  calls; the option is a no-op when `CMAKE_FIND_ROOT_PATH` is empty
  (desktop). Not done globally on purpose: setting the mode to `BOTH` would
  let host `/usr/lib` libraries leak into other `find_library` calls.

Surprises, no action needed or deferred:

- Root `PNG_LIBRARY=skia` is chosen by `UNIX AND NOT APPLE AND skia`, which
  is true on Android too. It works: `libskia.a` contains libpng. `png_static`
  builds anyway but is not linked.
- `third_party/libwebp` is not built with the Skia backend (webp comes from
  Skia's `libwebp.a`).
- pixman builds only its portable C code (`pixman-x86.c`/`pixman-arm.c` are
  CPU detection stubs; no NEON). Warning
  `unknown attribute '__force_align_arg_pointer__'` in `pixman-glyph.c`
  comes from upstream's `!__x86_64__` test; harmless on arm64, not silenced
  (submodule, not patched). json11 prints `-Wunqualified-std-cast-call`
  warnings (upstream third_party).
- The NDK `Compatibility with CMake < 3.10` deprecation noise comes from
  `android.toolchain.cmake`, as in M0.3.
- Desktop: root project configured with host gcc, SDK cmake 3.31.6,
  `-DLAF_BACKEND=none -DENABLE_UPDATER=OFF -DENABLE_NEWS=OFF`: OK. A desktop
  Skia configure/build was not possible (no desktop Skia here); the diff only
  touches `find_library` options, reviewed by hand.

First `app-lib` attempt (`cmake --build build/android --target app-lib`,
not fixed, input for M1): stops early on two classes of errors.

1. **`gen` is a build-time code generator and was built for the target.**
   Every `*.xml.h`, `pref.xml.cpp`, `command_ids.ini.h`,
   `check-translations.txt` rule (`src/app/CMakeLists.txt:13-87`, collected
   by `generate_files` at `:92`) runs `${GEN_EXE}`; with the target-built
   aarch64 binary the host tried `qemu-aarch64-static` and failed with
   `Could not open '/system/bin/linker64'` (66 of 68 failed steps). Fixed in
   M1.1, see the next section.
2. `src/updater/user_agent.cpp:57-60`: `base::Platform` has no `distroName` /
   `distroVer` because `laf/base/platform_unix.cpp` is not built on Android
   (M0.3 decision) and `platform.h` declares those members only for
   `LAF_LINUX`. `updater-lib` is built even with `ENABLE_UPDATER=OFF`
   (`src/CMakeLists.txt` adds it unconditionally) because `app-lib` links it
   for `user_agent`. Still open, M1.2 (guard in `user_agent.cpp`, see ROADMAP).

### Host-built `gen` (M1.1, 2026-10-22)

Branch `mobile/m1-host-gen` (from `mobile/m0-probe`), uncommitted. No CMake
file was edited: `src/CMakeLists.txt:76-79` already imports `gen` when
`GEN_EXE` is set, and `src/app/CMakeLists.txt` uses `${GEN_EXE}` /
`${GEN_DEP}` (empty then) everywhere.

- `android/scripts/build-host-gen.sh` configures the root project in
  `build/host-tools` with host `gcc`/`g++` (override `HOST_CC`/`HOST_CXX`),
  SDK cmake + ninja, `Release`, `LAF_BACKEND=none`, all optional features OFF,
  and builds only `gen`. Output: `build/host-tools/bin/gen`
  (`ELF 64-bit ... x86-64`). First run 62 s (46 ninja steps), re-run 2 s
  ("no work to do"). It `unset`s `CMAKE_TOOLCHAIN_FILE/ANDROID_*` so nothing
  from the Android environment leaks in.
- `configure-android.sh` runs it first and passes `-DGEN_EXE=...`. The cache
  entry shows as `GEN_EXE:UNINITIALIZED` (normal for `-D` without a type).
- `build/android` was deleted and re-configured from scratch (42 s);
  `cmake --build build/android --target generate_files` -> 67/67 steps OK,
  `build/android/src/app/pref.xml.h` exists, `qemu`/`linker64` count in the
  log: 0. The host root configure (gcc, `LAF_BACKEND=none`, updater/news OFF,
  tests OFF) still configures (`/tmp` tree, 118 s).
- `cmake --build build/android --target app-lib -- -k 0`: 1048 ninja steps,
  3m47s wall, 1047 succeed. `qemu`/`linker64` count 0. Every `app-lib` object
  compiles for Android, including `app.cpp`, `file_selector.cpp`,
  `app_os_object.cpp`, `state_with_wheel_behavior.cpp`, `ui/window.cpp`,
  `pref/preferences.cpp` (the sites listed in ROADMAP M1.2 are runtime
  `#if` decisions, not compile errors).

Remaining `app-lib` compile errors for M1.2, one class, 4 diagnostics, one
file (`updater-lib`, which `app-lib` needs; `libapp-lib.a` and everything
downstream are not produced until it compiles):

| Class | First example | Count |
|-------|---------------|-------|
| `base::Platform` has no `distroName`/`distroVer` on non-`LAF_LINUX` | `src/updater/user_agent.cpp:57` (also `:58`, `:59`, `:60`) | 4 errors in 1 file |

Warnings (not errors, 19): json11 `-Wunqualified-std-cast-call` (8,
`third_party/json11/json11.cpp:156` etc.), deprecated `draw_text` /
`drawTextWithDelegate` (5: `src/ui/graphics.cpp:489`, `src/ui/entry.cpp:907`,
`src/app/script/graphics_context.cpp:66,81`, `src/app/ui/skin/skin_theme.cpp:1530`),
unused `srcMaskBitmap` (2: `src/app/util/new_image_from_mask.cpp:41,99`),
unused private field `m_news` (`src/app/ui/home_view.h:97`). All upstream,
none caused by Android. Link errors (undefined `os::` symbols etc.) cannot
show up before `aseprite` is linked (M1.8).

### `app-lib` for Android (M1.2, 2026-10-22, branch `mobile/m1-app-lib`)

- `src/updater/user_agent.cpp`: `#elif LAF_ANDROID` writes
  `"Android " << android_get_device_api_level()` (`<android/api-level.h>`;
  the function is an out-of-line symbol from API 29, which is our minSdk,
  `ANDROID_PLATFORM=29`). The 4 errors are gone.
- `cmake --build build/android --target app-lib` succeeds (`lib/libapp-lib.a`,
  warnings only the upstream ones listed above). Desktop check: a separate
  host tree (`build/desktop-check`, gcc, `LAF_BACKEND=none`, updater ON)
  configures and `user_agent.cpp`, `app_os_object.cpp`, `select_shortcut.cpp`,
  `state_with_wheel_behavior.cpp` compile. A full desktop `app-lib` was not
  built (needs Skia). `LAF_ANDROID` is defined only by `laf-base` when
  `ANDROID`; `LAF_LINUX` is NOT defined on Android (so every `#if LAF_LINUX`
  below is false on Android, `__linux__` is true).
- Link preview (`--target aseprite -- -k 0`, still an executable until M1.8):
  one undefined symbol, `os::SkiaSurface::SkiaSurface(sk_sp<SkSurface> const&)`
  (referenced from `render_text.cpp`, `shader_helpers.cpp`), because the M0.3
  hack skips the Skia OS sources in laf (reverted by M1.3). Nothing else.

Per-site decisions (Android = `LAF_ANDROID`, `LAF_LINUX` false):

| Site | Android behaviour today | Decision | Reason |
|------|-------------------------|----------|--------|
| `app.cpp:82` include `os/x11/system.h` | not included | keep ("other") | X11 only |
| `app.cpp:309` `detectStylusPattern = x11StylusId` | not set | keep | X11 stylus heuristics; Android reports tool type directly (M2) |
| `app.cpp:395` `manager->invalidate()` vs `updateAllDisplays()` | `updateAllDisplays()` | keep | the Linux branch is an X11-specific workaround |
| `app.cpp:501` window icon | skipped | keep | launcher icon comes from the manifest |
| `file_selector.cpp:23,47` X11 display in `spec` | skipped | keep | no X11 |
| `file_selector.cpp:64` `get_initial_path_to_select_filename` | `setFileName(initialPath)` | keep | X11 dialog quirk; laf-dlgs has no Android impl, `FileDialog::make()` returns null and the built-in Aseprite selector is used |
| `file_selector.cpp:105` remember last folder | not stored | keep | applies to the native X11 dialog; revisit with SAF dialog (M4) |
| `app_os_object.cpp:29` `app.os.name` | was `nil` | **changed**: `"Android"` | nil is wrong for scripts; no desktop effect |
| `app_os_object.cpp:64` `app.os.linux` | `true` (laf `Platform::os` is `Linux` on Android) | **changed**: `false` on Android | scripts mean desktop Linux; there is no `OS::Android` in laf |
| `state_with_wheel_behavior.cpp:285` brush-size wheel sign | not inverted | **changed**: invert like Windows/Linux | laf convention (x11: up = -1) is the Windows/Linux sign. CONTRACT for M1.3/M2: the Android backend must report wheel deltas with that sign (up = negative), otherwise remove `LAF_ANDROID` here |
| `ui/window.cpp:866` `|| true` (no blit on X11) | blit allowed on CPU surface, off with GPU | keep | X11 specific (slow XPutImage copies); GPU path already disables it |
| `pref/preferences.cpp:56` native dialog off by default | left at default (true), falls back to built-in selector as `make()` is null | keep | the override is for X11 and macOS 10.11 bugs |
| `select_shortcut.cpp:184` Win key label/tooltip | label "Super" + tooltip (same as before, `__linux__` was true) | **changed** `__linux__` to `LAF_LINUX || LAF_ANDROID` | roadmap said `LAF_LINUX` only, but that would drop the label on Android; an external keyboard Meta key is the logo/Super key, so Linux wording is right. Desktop-neutral |
| `keyboard_shortcuts.cpp:47-66` | untouched | M3.5 | |

### Gradle skeleton (M0.5, 2026-10-22)

- Versions: Gradle 8.14.6, AGP 8.13.2 (latest stable 8.x). AGP 9.4.1 is the
  latest stable but requires build-tools 36 and Gradle 9.6; AGP 9.5 is still
  RC. Revisit when moving to compileSdk 36.
- `androidx.games:games-activity` 4.4.2 is the current release on Google
  Maven (checked 2026-10-22). `GameActivity` extends `AppCompatActivity`, so
  the theme must be an AppCompat one and `androidx.appcompat` is needed.
- `checkDebugDuplicateClasses` fails with kotlin-stdlib 1.8.22 vs
  stdlib-jdk7/jdk8 1.6.21 pulled transitively by AndroidX; fixed with the
  `kotlin-bom:1.8.22` platform in `app/build.gradle` (there is no Kotlin code).
- Groovy DSL needs `prop = value` assignment syntax (the space syntax is
  deprecated for Gradle 10); the build is warning-free with `--warning-mode all`.
- `AConfiguration_getUiMode` does not exist in the NDK, it is
  `AConfiguration_getUiModeType`.
- The ADR 0001 `configChanges` list encodes to `0x40007ffc` in the built
  manifest. The APK has only `lib/arm64-v8a/libaseprite.so` and no
  permissions except AndroidX's own signature-level
  `<applicationId>.DYNAMIC_RECEIVER_NOT_EXPORTED_PERMISSION` (not INTERNET).
- Gradle prints "SDK XML version 4" once if cmdline-tools are newer than AGP;
  harmless.
- Not verified without the tablet: that HarmonyOS 4.3 starts the activity,
  honours the immersive mode and reports the GL strings; see the tablet
  checklist in the M0.5 report.

### laf os/android (M1.3, 2026-10-22)

laf branch `mobile/laf-android` (`3bb409f`, `da608dc`). Build trees used:
`build/laf-m13-android`, `build/laf-m13-desktop`. Reviewed (M1.3 review).
The temporary os/none system and the skipped os/skia sources from M0.3 are
reverted.

- **GameActivity outside Gradle.** The games-activity 4.4.2 AAR `prefab`
  dir has prebuilt `libgame-activity_static.a` (4 ABIs) + headers; no glue
  sources needed (`abi.json` says `c++_shared`, links fine with
  `c++_static`). laf takes `game-activity::game-activity_static` from:
  (1) an existing target (recommended for M1.4: `find_package(game-activity
  REQUIRED CONFIG)` in the wrapper before `add_subdirectory`), (2)
  `find_package(game-activity CONFIG QUIET GLOBAL)`, (3) cache var
  `LAF_ANDROID_GAMEACTIVITY_DIR=<prefab dir>` (path under
  `~/.gradle/caches/8.14.6/transforms/<hash>/transformed/games-activity-4.4.2/prefab`,
  the hash changes).
- **Linking.** `laf-os` adds `INTERFACE -Wl,-u,Java_com_google_androidgamesdk_GameActivity_initializeNativeCode`;
  `android_main` lives in `liblaf-os.a` (no `--whole-archive`).
  `common/main.cpp` is not built on Android. Android test executables would
  not link (the `-u` pulls the glue) — keep `LAF_WITH_TESTS=OFF` there.
- **Weak hook `os::android_pre_main`** must be defined in an object of the
  final `.so` target itself (e.g. `src/main/`), not in `app-lib.a` — a weak
  reference does not pull an archive member (M1.7).
- **Glue 4.4.2** already calls `AConfiguration_fromAssetManager` in
  `android_app_pre_exec_cmd`; laf calls it again on CONFIG_CHANGED. Window
  size truth = `ANativeWindow_getWidth/Height`.
- **Logs:** tag `laf`; lifecycle INFO always; per-event verbose logs after
  the user runs `adb shell setprop debug.laf.verbose 1` and restarts the app.
- **Environment:** HOME = XDG_CONFIG_HOME = `internalDataPath`; TMPDIR =
  `Context.getCacheDir()` via JNI (fallback `<parent>/cache`).
- **Event loop:** `getEvent` → `pollLooper(0)`, then `ALooper_pollOnce`
  until an event or the deadline; `-1` only while paused without a window
  (ADR 0007 §4); `queueEvent` from other threads → `ALooper_wake`. App code
  may run inside `getEvent()` (resize → `handleWindowResize`), like X11
  ConfigureNotify. Input is only logged until M2.
- **Capabilities on Android:** WindowScale, ColorSpaces,
  GpuAccelerationSwitch (no MultipleWindows/CanResizeWindow/CustomMouseCursor).
- **Scroll sign contract (M1.2 ↔ M2.1):** `delta.y = -AXIS_VSCROLL`,
  `delta.x = +AXIS_HSCROLL` (like X11/Windows; `state_with_wheel_behavior.cpp:285`
  relies on it).

**Open follow-ups from the M1.3 review** (owner laf-backend unless noted):
1. (before M1.8) After `APP_CMD_DESTROY`, `getEvent(kWithoutTimeout)`
   returns immediately → busy loop if the app doesn't exit at once (e.g. a
   "Save changes?" dialog with no window) → ANR. Fix: bounded wait
   (≤100 ms) after DESTROY + watchdog `_exit(0)` 2–3 s after DESTROY if
   `app_main` hasn't returned (event_queue.cpp:75-78, app.cpp:357-366).
2. (M1.6 blocker) `SkiaWindowBase::resizeSkiaSurface`
   (`os/skia/skia_window_base.h:86-107`) abandons the GrDirectContext and
   destroys the GL context on every resize → rotation recreates GL. Fix in
   `skia_window_base.h`: opt-in fast path (protected virtual
   `keepGpuContextOnResize()` overridden by `SkiaWindowAndroid`) that only
   recreates the render target, plus a protected `recreateGpuRenderTarget()`
   called after a new EGLSurface in `onNativeWindowAttached()`.
3. `skia_system.h:52-58` capabilities: use `#if LAF_ANDROID … #else` instead
   of an early return; keep `#if SK_SUPPORT_GPU` for GpuAccelerationSwitch.
4. `os/android/main.cpp:48` `std::exit(0)` runs static destructors while
   app threads may live → consider `_exit(0)` (architect: ADR 0001 says exit).
5. Key filter: also pass `AKEYCODE_HEADSETHOOK` (79) and `AKEYCODE_MUTE` (91)
   to the system; update ADR 0006 §11 (architect).
6. TMPDIR fallback dir isn't created → `mkdir` it.

## 7. Integration points in the code (for all agents)

| Concern | Where |
|---|---|
| Entry point | `src/main/main.cpp:88` `app_main()`; laf `main()` in `laf/os/common/main.cpp:60` |
| System factory | `laf/os/common/system.cpp:44` `System::make()` → `makeSkia()` (`laf/os/skia/skia_system.cpp:15`) |
| Skia system per platform | `laf/os/skia/skia_system.h:21-31` (base class), `:74` `makeWindow()` |
| Skia window per platform | `laf/os/skia/skia_window.h:12-23`; X11 example `laf/os/skia/skia_window_x11.cpp:50-100` |
| GL context interface | `laf/os/gl/gl_context.h` (add `gl_context_egl.h`) |
| Skia GL surface | `laf/os/skia/skia_gl.cpp:31-56` `attachGL()`, `:68` `createRenderTarget()` |
| GPU on/off | pref `gpu_acceleration` (`data/pref.xml:157`), applied at `src/app/app.cpp:407-409` |
| Capabilities | `laf/os/capabilities.h` (no `MultipleWindows` on Android, "like a phone device") |
| Event pump | `src/ui/manager.cpp:403-438` (`getEvent(ev, timeout)`), event → message switch `:439-617` |
| Pointer types | `laf/os/pointer_type.h:13-21` |
| Eraser by pointer | `src/app/ui/editor/editor.cpp:1837-1846` |
| Pinch / pan | `src/app/ui/editor/state_with_wheel_behavior.cpp:71,513`; macOS producer `laf/os/osx/view.mm:549,576` |
| Stroke cancel | `src/app/ui/editor/drawing_state.cpp:322-328` |
| Pressure into tools | `src/app/tools/tool_loop_manager.cpp:188,410`, `src/app/tools/pointer.h:52` |
| Modifiers query | `laf/os/system.h:187,193` (`isKeyPressed`, `keyModifiers`) |
| IME | `laf/os/system.h:205`, `src/ui/entry.cpp:170,301,317` |
| Shortcut platform attr | `src/app/ui/keyboard_shortcuts.cpp:47-66`; key names `src/ui/shortcut.cpp:35-139` |
| Platform defaults for prefs | `src/app/pref/preferences.cpp:55-65` |
| Data lookup | `src/app/resource_finder.cpp:84-118` (data), `:169-209` (user dir) |
| App path | `laf/base/fs_unix.h:170-188` (`/proc/self/exe`) |
| Log sink | `laf/base/log.cpp:37-38,80-95` (stderr / file) |
| Platform macro | `laf/base/CMakeLists.txt:113-123` |
| Clipboard backend select | `laf/clip/CMakeLists.txt:81-104` (`clip_none.cpp` fallback) |
| File dialog backend | `laf/dlgs/CMakeLists.txt:7-13` |
| Exe target | `src/CMakeLists.txt:184-229` |

## 8. EULA notes

- Personal builds only. ADR 0004 forbids release signing, store metadata,
  and the `INTERNET` permission. Logs/crash reports are collected by the
  user via `adb` (no upload service).

## 9. Device facts (verified on the user's tablet, 2026-10-21)

Collected with read-only `adb` (`getprop`, `dumpsys display|input|SurfaceFlinger`,
`getevent -lt`). Supersedes the "unverified" notes in §2–§4 where they differ.

### System

| Property | Value |
|----------|-------|
| Model | `SLG-W09` (product `SLG-W29`, device `HWSLG`) |
| Firmware | HarmonyOS 4.3.0 (`SLG-W29 4.3.0.130(C10E22R2P2)`), `EmotionUI_15.0.0` |
| Android API | **31** (`ro.build.version.release=12`) — `FLAG_CANCELED` (API 33) not available |
| ABIs | `arm64-v8a,armeabi-v7a,armeabi` |
| SoC / GPU | Kirin T92C, **Maleoon 920A**, OpenGL ES **3.2** (`ro.opengles.version=196610`), Vulkan driver `hvgr_v300` present |
| EGL | 1.4, has `EGL_KHR_partial_update`, `EGL_KHR_swap_buffers_with_damage`, `EGL_ANDROID_presentation_time`, `EGL_KHR_surfaceless_context` |
| Display | 1840×2800 native (portrait), used landscape 2800×1840; density **400** (scale 2.5), 270×276 dpi |
| Refresh | modes 30 / 60 / 90 / 120 / 144 Hz; default mode 60 Hz; active mode was 120 Hz at capture time (adaptive) |

### Input devices

- `huawei,ts_pen` (`/dev/input/event4`, sources `0x5103` = TOUCHSCREEN|STYLUS|KEYBOARD):
  pen on the screen. Axes: X/Y (raw 14720×22400, 8× screen), **PRESSURE**
  (raw 0..16384, calibration `physical`), **TILT_X/Y** (±90°), ORIENTATION.
  No `ABS_DISTANCE`. `BTN_DIGI` = pen in range (hover), `BTN_TOUCH` = tip down.
  Hover is reported before contact (observed ~40 ms DIGI→TOUCH) → Android
  `ACTION_HOVER_*` with `TOOL_TYPE_STYLUS` expected (confirm in M0.6 probe).
- `input_mt_wrapper` (`/dev/input/event3`, sources `0x1103`): fingers.
  Protocol A (`SYN_MT_REPORT`), `ABS_MT_TRACKING_ID` 0..9, pressure,
  orientation, `TOUCH_MAJOR/MINOR`. Up to 4 simultaneous contacts seen.
- `HUAWEI M-Pencil 3 Keyboard` (`event9`, BT HID, vendor `0x12d1` product `0x10a5`)
  and `HUAWEI M-Pencil 3 Mouse` (`event8`): the pen's Bluetooth side channel.
- `HUAWEI Magnetic Keyboard` (+ Consumer Control): the user also has the
  Huawei keyboard attached — hardware keyboard path (M2.3) is testable.

### Pen double-tap (M-Pencil 3)

- Observed: one double-tap on the barrel → on `event9`:
  `MSC_SCAN 0x0007006f` (HID usage F20), `KEY_F20` (Linux 190) **DOWN/UP twice
  within ~22 ms**. `/system/usr/keylayout/Generic.kl:217` maps `key 190 F20`.
  This matches the Krita/Drawpile report (§3) of two down/up pairs per
  double-tap; the Android keycode is expected to be the Huawei-specific 718 —
  confirm in the M0.6 probe APK (log `KeyEvent.getKeyCode()`, `getScanCode()`,
  device name).
- Handling rule: treat a burst of F20 pairs within ~100 ms as **one**
  double-tap gesture (debounce), and ignore the key-up.
- Only 1 of 3 double-taps by the user was registered in the capture. Unknown
  whether taps were too soft or the system's own double-tap action consumed
  some. Re-check in M0.6 with the system double-tap action set to "none".

### Palm vs finger (raw evdev)

- Palm contact: `ABS_MT_TOUCH_MAJOR` raw values have **bit 30 set**
  (`0x40000000`; also bits 23/24 seen) with low byte ~`0xde`–`0xf5`;
  several tracking ids at once (up to 4).
- Finger contacts: `TOUCH_MAJOR` low byte `0x75`–`0x8c`, high bits clear
  (rarely `0x00800000`/`0x01000000`).
- Android's input config reports raw TouchMajor range 0..1 for this device,
  so the cooked `getTouchMajor()` is probably not usable directly. Whether
  the framework turns the palm flag into `TOOL_TYPE_PALM` or `ACTION_CANCEL`
  must be checked in the M0.6 probe. Baseline strategy stays **stylus
  priority** (ignore fingers while the pen is in range/touching).

### M0.5 hello APK on the device (2026-10-22)

Installed by the user (`adb install -r`), logcat read by the main session.
Result: **passed** — solid colour full-screen, no crash.

- Startup: `sdk=31 density=400`, window and `ANativeWindow` 2800×1840,
  `GL_RENDERER: Maleoon 920A`, `OpenGL ES 3.2`, GLSL ES 3.20. First frame
  displayed +415 ms after launch.
- Refresh: `Display.getRefreshRate()` reports **120 Hz**, not 144; steady
  ~120 fps with `swapInterval=1`, dips to 70–80 fps while rotating. Getting
  144 Hz probably needs `ANativeWindow_setFrameRate()` / a preferred display
  mode (M4.3).
- Rotation (4×): no activity recreation; `APP_CMD_CONFIG_CHANGED` +
  `WINDOW_RESIZED` (1840×2800 ↔ 2800×1840) + `CONTENT_RECT_CHANGED`. The
  configChanges list from ADR 0001 works on HarmonyOS 4.3. (The stub logs
  the startup `AConfiguration` on config change, so `orientation=` there is
  stale — cosmetic.)
- Home / Recents / power button (5 cycles): `PAUSE → TERM_WINDOW` (EGL
  destroyed) `→ STOP → SAVE_STATE`, then `START → RESUME → INIT_WINDOW`
  with a fresh EGL context each time. No crash.
- HarmonyOS sends **`APP_CMD_LOW_MEMORY` right after every `SAVE_STATE`**
  (every time the app goes to background), even with plenty of memory.
  Don't treat it as real memory pressure (no cache purge/autosave storm).
- **Swiping the app away from Recents kills the process (SIGKILL,
  "remove task") — no `APP_CMD_DESTROY`, `android_main` never returns.**
  So Aseprite must flush data-recovery/session state on `PAUSE`/`STOP`,
  never rely on a clean exit (M2.5).
- Touch input reaches the native side (`APP_CMD 20` + motion events,
  currently ignored by the stub).

### M0.6 input probe on the device (2026-10-22)

Probe APK `0.0.2-m0-probe` installed by the user; logcat
(`AsepriteProbe`, ~6500 lines) read by the main session. The user did not
follow the step order exactly, so steps were identified from the events.

**Pen double-tap — confirmed.**
- Arrives at the Activity (`dispatchKeyEvent`) **and** in native code
  (GameActivity key filter accepting all keys) as **`keyCode=718`,
  `scanCode=190`**, `source=KEYBOARD`, `flags=FROM_SYSTEM`,
  `metaState=0x200000`, device **"HUAWEI M-Pencil 3 Mouse"** (id 126,
  vendor 0x12d1 product 0x10a5). The separate evdev node "M-Pencil 3
  Keyboard" from §9 is merged into that Android device.
- 11 double-taps captured (hovering and away from the screen — both
  work), **each = 2× DOWN/UP pairs 6–16 ms apart**. Debounce rule from
  ADR 0006 (pairs within ~100 ms = one gesture) is confirmed.
- System setting (read-only, `settings list secure`): `Double_Touch_index=0`,
  `Double_Touch_Keycode=131`; the system did not consume the gesture with
  this setting while the app was focused.

**Stylus (device 5 "huawei,ts_pen", source 0x5002).**
- `tool=STYLUS` only; no `TOOL_TYPE_ERASER` (M-Pencil 3 has no eraser end).
- **Hover works**: `HOVER_ENTER / HOVER_MOVE / HOVER_EXIT` reach native code
  (Java `OnHoverListener` path from the probe). `AXIS_DISTANCE` is always 0
  — only in-range/out-of-range is known, not the height.
- `pressure` 0..0.857 (continuous), `tilt` 0..1.18 rad, `orientation`
  −1.6..1.4 rad — all live once the axes are enabled with
  `GameActivityPointerAxes_enableAxis()` (they are 0 by default, M2.1 must
  enable PRESSURE/TILT/ORIENTATION). `buttonState` always 0.

**Fingers and palm (device 4 "input_mt_wrapper").**
- Palm is reported as **several `tool=FINGER` pointers** (5–8 at once), with
  larger `touchMajor` (up to ~48 vs ≤ ~23 for fingertips) and higher
  `pressure` (up to ~15 vs ≤ ~7). **No `TOOL_TYPE_PALM`**, no
  `FLAG_CANCELED` (API 31).
- HarmonyOS's own palm rejection then sends **`ACTION_CANCEL` for the whole
  finger stream** (6 times; 0.1–2 s after the palm lands). So the app must
  treat `ACTION_CANCEL` as "discard the current finger gesture/stroke",
  exactly as ADR 0006 §5 says.
- Re-test (11:47, palm resting, then drawing): after the palm's
  `ACTION_CANCEL` the system **swallows the resting palm completely** — no
  finger events at all while the pen draws (stylus DOWN/MOVE/UP and hover
  keep arriving normally). So HarmonyOS already does palm rejection at the
  system level; the app only has to (1) discard the finger gesture/stroke
  on `ACTION_CANCEL` and (2) ignore fingers while the pen is in range
  (covers the 0.1–2 s before the system's CANCEL).
- One-, two-, three-finger taps and pinch arrive as expected
  (`POINTER_DOWN/UP`, up to 8 pointers during palm contact).
- Hardware keyboard: "HUAWEI Magnetic Keyboard" (id 125) is also visible.
