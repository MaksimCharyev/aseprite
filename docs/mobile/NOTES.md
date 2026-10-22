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

Status: **reported, not verified on the user's device.** Update: see §9, verified 2026-10-21 (evdev `KEY_F20`; Android key code 718 still to confirm in M0.6).

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

Status: **documented** (Android); device behaviour **not verified**.

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
  with `source & SOURCE_TOUCHSCREEN` — stylus (0x4002) passes because of the
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
