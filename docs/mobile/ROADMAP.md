# Aseprite Mobile — Roadmap

Owner: `mobile-architect`. Binding decisions: `docs/mobile/CONTEXT.md`,
ADRs in `docs/mobile/adr/`. Research and code map: `docs/mobile/NOTES.md`
(section 7 lists every integration point with `file:line`).

Conventions:

- Each task fits one agent session. **Owner** is the agent that implements
  it; `mobile-reviewer` reviews every task diff (not listed each time) and
  owns the review/checklist task that closes each milestone.
- **Accept** is what the user (or, for build-only tasks, the reviewer)
  checks. "Tablet:" means a manual step on the MatePad; agents provide the
  exact `adb` commands in their report (D5).
- Branch per task: `mobile/<milestone>-<topic>` in the repo and, for laf
  changes, the same name in `laf/` (fork). Commit only when the user says so.
- Status: `todo` / `wip` / `done` / `blocked(<why>)`.

Common logcat filter used below (tags are defined in M0.5/M1.1):

```
adb logcat -c && adb logcat -v time -s Aseprite:V AsepriteProbe:V laf:V AndroidRuntime:E DEBUG:V libc:F
```

---

## M0 — Toolchain and probes

Goal: everything cross-compiles for `arm64-v8a`, an APK installs, and we
know how the device really reports pen/touch/keys.

| # | Task | Owner | Accept | Status |
|---|------|-------|--------|--------|
| M0.0 | Submodules initialized, laf remotes per D3 | architect | `git submodule status` has no `-` prefix | **done** (2026-10-21, laf `500768c`) |
| M0.1 | Host toolchain | build-engineer | see below | **done** (2026-10-21) |
| M0.2 | Skia m124 for Android | build-engineer | see below | **done** (2026-10-21) |
| M0.3 | `LAF_ANDROID` platform in laf CMake + base | build-engineer | see below | **done** (2026-10-22), laf branch `mobile/laf-android` + clip branch `mobile/android` (both forks) |
| M0.4 | Cross-build non-GUI libs | build-engineer | see below | **done** (2026-10-22), branch `mobile/m0-libs`; see NOTES section 6 |
| M0.5 | Gradle skeleton + GameActivity "hello" APK | build-engineer | see below | **done** (2026-10-22), branch `mobile/m0-gradle`; passed on the tablet (NOTES §9, "M0.5 on the device") |
| M0.6 | Input probe APK | touch-ux | see below | **done** (2026-10-22): double-tap = keyCode 718 confirmed, hover OK, palm = FINGER + ACTION_CANCEL (NOTES §9 "M0.6"); palm while drawing re-tested OK (system swallows the palm after CANCEL) |
| M0.7 | M0 review + device info checklist | reviewer | see below | **done** (2026-10-22): M0 accepted by the reviewer; checklist in TEST_CHECKLIST.md |

**M0.1 Host toolchain.** Install, user-local without sudo where possible,
under `~/Android/Sdk`: JDK 17, Android cmdline-tools, `platform-tools`,
`platforms;android-35`, `build-tools;35.0.0`, `ndk;26.1.10909125` (r26b),
`cmake;3.31.x`, Ninja. Write
`docs/mobile/BUILDING.md` with the exact commands and env vars
(`ANDROID_HOME`, `ANDROID_NDK_HOME`). Ask the user before installing
anything system-wide.
Accept: `sdkmanager --list_installed` shows the packages;
`$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin/clang --version`
prints clang from NDK r26b; `adb version` works.

**M0.2 Skia for Android** (ADR 0005). `android/scripts/build-skia.sh`:
clone `aseprite/skia` at `08a5439a6be7…` into
`$HOME/deps/skia-m124-android`, sync deps, `gn gen` with the ADR 0005
args, build `skia` + `modules` (skshaper, skunicode).
Accept: `out/Release-arm64/{libskia.a,libskshaper.a,libskunicode*.a}`
exist; `file` on an object inside says `ELF 64-bit ... ARM aarch64`;
`args.gn` copied into NOTES.md §6.

**M0.3 `LAF_ANDROID` platform** (ADR 0003), in the laf fork.
`laf/base/CMakeLists.txt:113-123` adds the `ANDROID` branch;
`laf/cmake/FindSkia.cmake` Android branch (`SK_BUILD_FOR_ANDROID`, no
`SK_R32_SHIFT=16`, GLESv2/EGL/log/android, no fontconfig, `SKIA_ARCH`
arm64); `laf/clip/CMakeLists.txt:81` → `clip_none.cpp` on Android;
`laf/dlgs/CMakeLists.txt:7-13` → no-op dialog on Android;
`laf/os/CMakeLists.txt` selects `os/none` sources on Android for now;
`laf/base/log.cpp` also writes to `__android_log_write(tag "laf")` under
`LAF_ANDROID`; fix `base/` compile errors with `LAF_LINUX || LAF_ANDROID`
where behaviour is POSIX-generic.
Accept: `cmake -G Ninja -DCMAKE_TOOLCHAIN_FILE=$NDK/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=29 -DLAF_BACKEND=skia ...` on `laf/`
alone builds `laf-base laf-gfx laf-ft laf-text laf-os` with
`LAF_WITH_EXAMPLES=OFF LAF_WITH_TESTS=OFF`; desktop laf build +
`ctest` still pass.

**M0.4 Cross-build non-GUI Aseprite libs.** Root CMake with the Android
toolchain and the OFF options from ADR 0003 §5; build targets `doc-lib`,
`render-lib`, `fixmath-lib`, `filters-lib` and the `third_party` libs
they pull in (zlib, libpng, giflib, libwebp, tinyxml2, pixman if needed,
lua, cmark, fmt, simpleini, json11, qoi, tinyexpr, libarchive). Record
every patch needed in NOTES.md.
Accept: `cmake --build build/android --target doc-lib render-lib` succeeds
for arm64; desktop `cmake --build build` unchanged.

**M0.5 Gradle skeleton** (ADR 0001, 0004). `android/` with
`settings.gradle`, `app/build.gradle` (AGP current stable, `minSdk 29`,
`targetSdk 34`, `abiFilters 'arm64-v8a'`, `ndkVersion` r26b, `prefab true`,
`androidx.games:games-activity:4.4.2`), `AsepriteActivity extends
GameActivity`, manifest with `configChanges` per ADR 0001,
`launchMode="singleTask"`, **no INTERNET permission**, debug signing only,
`applicationId` `com.github.maksimcharyev.aseprite` (confirmed by the user).
Native `android_main` stub: logs `Build.VERSION.SDK_INT` (via
`AConfiguration_getSdkVersion`), display size/density, then clears the
window to a solid colour with EGL + `glClear` each frame.
Accept: `./gradlew :app:assembleDebug` produces
`app/build/outputs/apk/debug/app-debug.apk`. Tablet: `adb install -r` it,
the app opens full-screen in a solid colour, rotating doesn't crash,
logcat shows the SDK level line.

**M0.6 Input probe** (feeds ADR 0006). In the same skeleton (build flavour
or `#if` switch), log every motion and key event the glue receives, with
tag `AsepriteProbe`: action, source, deviceId, flags, buttonState,
actionButton, pointer count, per pointer id/toolType/x/y/pressure/
touchMajor/touchMinor/distance/tilt/orientation, historySize; for keys
keyCode/scanCode/metaState/repeat/flags. Install a motion filter that
accepts all sources, including hover.
Accept, tablet, with the logcat command above, each run pasted back:
(a) pen hover → draw → lift; (b) palm resting while drawing;
(c) one finger tap, two-finger tap, pinch; (d) pen double-tap ×3 while
hovering and ×3 away from the screen (confirms the Android key code for
evdev `KEY_F20`, expected 718, NOTES.md §9); (e) pen side button — n/a for
M-Pencil 3 (no side button).

**M0.7 Review.** Reviewer checks desktop build + tests, verifies EULA
constraints in Gradle/manifest, and writes the probe checklist (M0.6 runs
a–d) into the report. Device facts (getprop/dumpsys/getevent) are already
verified in NOTES.md §9. Architect then updates ADR 0006 and NOTES.md §3–§4
from the probe logs.
Accept: user has pasted the M0.6 probe logs; NOTES.md §3–§4 updated with
the Android-level results (double-tap key code, hover, palm tool type /
`ACTION_CANCEL`) and ADR 0006 moved to Accepted.

---

## M1 — Bring-up: laf on Android, Aseprite shows the Home tab

Inputs from M0 (NOTES.md §6, §9): `app-lib` is blocked only by the
target-built `gen` and `user_agent.cpp`; `data/` is already packaged as
APK assets (at the asset root, from the source tree); EGL/GLES 3.2 and the
lifecycle sequence are verified; swipe-from-Recents SIGKILLs the process;
`LOW_MEMORY` follows every `SAVE_STATE`; the display runs at 120 Hz with
`swapInterval=1`. ADRs: 0001 (GameActivity), 0002 (assets), 0003
(`LAF_ANDROID`), 0007 (background flush), 0008 (native build).

Order: M1.1-M1.2 (build-engineer) and M1.3 (laf-backend) can run in
parallel; M1.4 needs M1.3; M1.8 needs M1.2, M1.6, M1.7.

| # | Task | Owner | Accept | Status |
|---|------|-------|--------|--------|
| M1.1 | Host-built `gen` for cross-compiling (ADR 0008 §1) | build-engineer | see below | **done** (2026-10-22), `mobile/m1-host-gen` |
| M1.2 | `app-lib` compiles for Android | build-engineer | see below | **done** (2026-10-22), `mobile/m1-app-lib` |
| M1.3 | `laf/os/android`: system, ALooper event loop, window, screen, lifecycle | laf-backend | see below | **done** (2026-10-22), laf `da608dc`; review follow-ups in NOTES §6 "laf os/android" |
| M1.4 | Gradle drives the root CMake; native variants `probe` / `hello_laf` / `aseprite` (ADR 0008 §2-3) | build-engineer | see below | **done** (2026-10-22): probe + hello_laf APKs; hello_laf passed on the tablet (NOTES §6 "M1.4 hello_laf APK on the device") |
| M1.5 | CPU raster window path, `hello_laf` visible | laf-backend | see below | **done** (2026-10-22), laf `b5f3f25`; verified with the aseprite variant on the tablet |
| M1.6 | EGL context + Skia GL surface | laf-backend | see below | todo |
| M1.7 | Assets under `assets/data/` + manifest + extraction (ADR 0002, 0008 §4) | build-engineer | see below | **done** (2026-10-22): 189 files extracted in 17 ms, "data up to date" on relaunch |
| M1.8 | Aseprite as `libaseprite.so` (`aseprite` variant) | build-engineer | see below | **done** (2026-10-22): Aseprite Home tab on the tablet (NOTES §6 "M1.8 … first run") |
| M1.9 | Flush preferences + data recovery on background (ADR 0007) | touch-ux | see below | todo |
| M1.10 | First-run defaults for Android | touch-ux | see below | todo |
| M1.11 | M1 review + tablet checklist | reviewer | see below | todo |

**M1.1 Host `gen`** (ADR 0008 §1). New `android/scripts/build-host-gen.sh`:
configure `build/host-tools` with the host compiler and the SDK
CMake/Ninja (`source android/scripts/env.sh`), `-DLAF_BACKEND=none`,
`ENABLE_UPDATER/NEWS/DRM/STEAM/SENTRY/DESKTOP_INTEGRATION/WEBSOCKET/TESTS/CCACHE=OFF`,
`LAF_WITH_EXAMPLES=OFF LAF_WITH_TESTS=OFF`; build only target `gen`;
idempotent. `android/scripts/configure-android.sh` runs it first and adds
`-DGEN_EXE=$repo/build/host-tools/bin/gen`. This uses the existing upstream
hook `src/CMakeLists.txt:76-79` (imported `gen`, no `GEN_DEP`), so no edit
of `src/app/CMakeLists.txt` (generation rules at `:13-87`, target
`generate_files` at `:92`) should be needed; if one is, explain why. Update
`docs/mobile/BUILDING.md` (host compiler requirement, new build dir) and
record results in NOTES.md §6. (NOTES §6 says "~line 819" for the rules;
the real lines are 13-87, line 819 is only the include dir.)
Accept (build, reviewer): `file build/host-tools/bin/gen` says x86-64;
after a fresh `configure-android.sh`,
`cmake --build build/android --target generate_files` succeeds and
`build/android/src/app/pref.xml.h` exists; `grep -c qemu` on the build log
is 0; `cmake --build build/android --target app-lib` gets past all
generated headers (remaining compile errors are listed in NOTES.md for
M1.2, not fixed here); desktop configure/build unchanged (`git diff`
touches no shared CMake, or the diff is desktop-neutral).

**M1.2 `app-lib` compiles for Android.** Fix what M1.1 uncovers, all
behind `LAF_ANDROID`, smallest change each:
`src/updater/user_agent.cpp:52-60` gets an `#elif LAF_ANDROID` branch
writing `"Android " << android_get_device_api_level()` (decision: guard in
`src`, do not invent `distroName/distroVer` in laf `base/platform.h:74-76`;
no ADR, one-file change); `src/app/app.cpp:82,309,395,501`,
`src/app/file_selector.cpp:23-105`, `src/app/script/app_os_object.cpp:29`,
`src/app/ui/editor/state_with_wheel_behavior.cpp:285`,
`src/ui/window.cpp:866`, `src/app/pref/preferences.cpp:56` (decide per site
whether Android behaves like Linux or like "other"; write the choice in the
commit message), `src/app/ui/select_shortcut.cpp:184` (`__linux__` is also
defined on Android → use `LAF_LINUX`). Do not touch
`keyboard_shortcuts.cpp:47-66` yet (M3.5). Uses laf as on branch
`mobile/laf-android` (os/none is fine for compiling).
Accept (build): `cmake --build build/android --target app-lib` succeeds for
arm64; desktop build + `ctest` unchanged.

**M1.3 `laf/os/android`** (laf fork, branch `mobile/laf-android`).
- **Revert the M0.3 temporary hacks**: `laf/os/CMakeLists.txt` Android
  branch (`:52-53` os/none, `:75` skia OS sources skipped, `:141`) and
  `laf/os/common/system.cpp:49` (`makeSkia()` skipped on Android).
- `main.cpp`: `android_main` (ADR 0001) stores the `android_app*`
  (accessor `os::android_app()` in an Android-only laf header for
  app-side code), sets `HOME`, `XDG_CONFIG_HOME`, `TMPDIR` (ADR 0002 §2;
  generic laf part), installs the motion filter (all sources incl. hover)
  and the key filter of ADR 0006 §11 (system keys pass to the system),
  enables the axes of ADR 0006 §1, calls an optional app pre-main hook
  (weak symbol, used by M1.7), then `app_main`; on return
  `GameActivity_finish()` + `exit(0)`.
- `event_queue.{h,cpp}`: resolve the TODO in `os/android/event_queue.h:24-26`:
  `getEvent(ev, timeout)` = drain queue, else `ALooper_pollOnce(timeoutMs)`
  and process `APP_CMD_*` / swap input buffers; `queueEvent()` from other
  threads calls `ALooper_wake()`. While paused with an empty queue it may
  block with `-1` (ADR 0007 §4).
- Commands: `INIT_WINDOW`/`TERM_WINDOW` → window surface attach/detach
  (synchronously, ADR 0001); `PAUSE`/`RESUME` → `AppLeave`/`AppEnter`
  (ADR 0007); `LOW_MEMORY` → log only (it follows every `SAVE_STATE` on
  HarmonyOS, NOTES §9); `CONFIG_CHANGED` → **refresh the configuration
  with `AConfiguration_fromAssetManager(app->config, assetManager)`**
  (otherwise density/orientation stay stale, M0.7 review) and, with
  `WINDOW_RESIZED`/`CONTENT_RECT_CHANGED`, emit `ResizeWindow`.
- `SystemAndroid` as base of `SkiaSystem` (`laf/os/skia/skia_system.h:21-31`;
  capabilities: no MultipleWindows/CanResizeWindow/CustomMouseCursor),
  single window bound to `app->window` (`skia_window.h:12-23`), screen size
  and density from `AConfiguration` (density exposed for M1.10).
- **JNI rule** (M0.7 review): cache `jclass`/`jmethodID`s once at startup
  (global refs) and call `ExceptionCheck()`/`ExceptionClear()` after every
  `GetMethodID`/`Call*` — debuggable builds run CheckJNI, which aborts on a
  pending exception. Do not copy the probe's `describeDevice` pattern
  (`android/app/src/main/cpp/main.cpp:515-594`).
- No input mapping yet (M2).
Accept (build): laf builds for Android with `LAF_BACKEND=skia` **and** the
`os/skia` sources; `hello_laf` links as a shared library with
`-Wl,--no-undefined`; desktop laf build + `ctest` pass.

**M1.4 Gradle drives the root CMake** (ADR 0008 §2-3). Wrapper
`android/app/src/main/cpp/CMakeLists.txt` → `add_subdirectory(<repo>)`
with the ADR 0003 §5 options, `GEN_EXE`, Skia dirs; `preBuild` runs
`build-host-gen.sh`; `-PnativeTarget=probe|hello_laf|aseprite` (default
`aseprite` once M1.8 lands; until then `probe`), all producing
`libaseprite.so`; `versionName` suffix per variant; Java key logging in
`AsepriteActivity.dispatchKeyEvent` only in `probe` (`BuildConfig`
field), `OnHoverListener` kept in all variants; native CMake build type
`RelWithDebInfo`. `laf_add_example` builds a SHARED library on Android.
Accept: `./gradlew :app:assembleDebug -PnativeTarget=probe` and
`-PnativeTarget=hello_laf` both produce an APK. Tablet: probe variant
behaves as in M0.6 (double-tap logs keyCode 718); `hello_laf` variant
starts and `adb logcat -s laf:V` shows `android_main`, the window size
2800×1840 and density 400 (black screen is fine here).

**M1.5 CPU raster path.** `SkiaWindowAndroid::onPaint` copies the Skia
bitmap to `ANativeWindow_lock` buffers (`WINDOW_FORMAT_RGBA_8888`), like
`laf/os/skia/skia_window_x11.cpp:58-100`; painting skipped while there is
no window; repaint after `INIT_WINDOW`.
Accept: tablet, `hello_laf` variant: content visible, red is red (RGBA,
ADR 0005); Home → back ×5 and rotate ×5 without crash or stale size.

**M1.6 EGL + Skia GL.** `laf/os/gl/gl_context_egl.h` implementing
`os::GLContext` (`laf/os/gl/gl_context.h`): EGL 1.4, GLES 3 context,
window surface from `ANativeWindow`, `swapInterval=1` (120 Hz, as measured;
144 Hz is M4.3). **Keep `EGLDisplay` + `EGLContext` (and the Skia
`GrDirectContext`) across `TERM_WINDOW`; destroy/recreate only the
`EGLSurface`** (`EGL_KHR_surfaceless_context` is available, NOTES §9; the
M0.5 stub tears everything down, `android/app/src/main/cpp/main.cpp:125-141`
— do not copy). Full context recreation only on `EGL_CONTEXT_LOST`.
`SkiaGL::attachGL` (`laf/os/skia/skia_gl.cpp:31`) must work with it.
Accept: tablet, `hello_laf` with GPU on: `adb logcat -s laf:V` shows
"OS: Using OpenGL backend" (`skia_gl.cpp:49`) once per launch (not per
resume); Home → back ×5 and power off/on ×5 without crash, black screen
or GPU-resource loss (no second "Using OpenGL backend", no
`GrDirectContext` recreation in the log); rotate ×5 OK.

**M1.7 Assets** (ADR 0002, ADR 0008 §4). Replace
`assets.srcDirs = ['../../data']` (assets at the APK root today) with a
Gradle task that copies `data/` plus the `copy_data` extras (`README.md`,
`AUTHORS.md`, `EULA.txt`, `docs/LICENSES.md`, `src/CMakeLists.txt:156-179`)
into a generated `assets/data/` and writes `assets/data.manifest` (file
list + content hash). The manifest is built from what is actually
packaged (AGP's default `ignoreAssetsPattern` drops `.*`, `_*` dirs, `*~`).
Native extractor in Aseprite's Android glue (pre-main hook from M1.3):
compares hash with `<filesDir>/aseprite/data/.stamp`, re-extracts on
change, logs time. `base::get_user_docs_folder()`
(`laf/base/fs_unix.h:199`) returns the external files dir on Android.
Accept: tablet, `aseprite` or `hello_laf` variant: first launch logs
"extracted N files in X ms" (N = number of lines in the manifest), second
launch "data up to date";
`adb shell run-as com.github.maksimcharyev.aseprite ls files/aseprite/data`
lists `gui.xml pref.xml widgets extensions ...`; reinstall of a changed
APK re-extracts.

**M1.8 Aseprite in the APK.** `src/CMakeLists.txt:184-229`: under
`ANDROID` build `add_library(aseprite SHARED main/main.cpp)` linked with
`app-lib` and laf's Android `os` (provides `android_main`); default Gradle
variant becomes `aseprite`.
Accept: tablet shows the Aseprite main window with the Home tab, correct
colours, readable even if tiny; Home → back ×3 and rotate ×3 keep it
drawn; logcat (filter at the top) has no `FATAL`/`F DEBUG`.

**M1.9 Background flush** (ADR 0007). ui: virtual
`Manager::onAppLeave/onAppEnter` (next to `src/ui/manager.h:153`) called
for `os::Event::AppLeave/AppEnter` in `generateMessagesFromOSEvents()`
(`src/ui/manager.cpp:439-617`); app: `CustomizedGuiManager`
(`src/app/modules/gui.cpp:84-105`) overrides `onAppLeave()` under
`#if LAF_ANDROID`: `Preferences::instance().save()` +
`flush_config_file()` (`src/app/ini_file.cpp:127`), and a new
`BackupObserver::backupNow()` (wake `m_wakeup`,
`src/app/crash/backup_observer.cpp:143`) through `DataRecovery`; log
timings with tag/prefix `Aseprite`.
Accept: tablet: change a preference (e.g. Edit > Preferences > a
checkbox), press Home → log shows "prefs saved in X ms" and "backup
requested"; swipe the app away from Recents, relaunch → the preference
change is kept. (Full drawing + recovery scenario is M2.5.) Desktop:
no behaviour change.

**M1.10 First-run defaults.** Under `#if LAF_ANDROID` in
`src/app/pref/preferences.cpp:50-65` (first run only):
`gpu_acceleration=true` (`data/pref.xml:157`), screen/UI scale from the
laf screen density: proposal `screen_scale = clamp(round(dpi / 133), 1, 4)`
→ **3** at 400 dpi with `ui_scale=1` (≈ desktop physical size at 92 dpi,
logical 933×613), `use_native_file_dialog=false`,
`multiple_windows=false`. `gui.cpp:126-162` then keeps the value.
Accept: tablet, fresh install (`adb uninstall` first): Home tab text and
icons legible at arm's length; Edit > Preferences shows the values; the
user tries 2/1 and 2/2 and reports which is best (feeds M3.7).

**M1.11 Review.** Desktop build + tests, guards (`LAF_ANDROID`/`ANDROID`),
no EULA-violating additions (no INTERNET, no release signing), JNI rule of
M1.3, and a tablet checklist for the user.
Accept: user confirms checklist items.

---

## M2 — Input, lifecycle, files

| # | Task | Owner | Accept | Status |
|---|------|-------|--------|--------|
| M2.1 | Pen + finger → mouse events with pressure and history | laf-backend | see below | todo |
| M2.2 | Hover → cursor/brush preview (no distance) | laf-backend | see below | todo |
| M2.3 | Hardware keyboard, key filter, Back | laf-backend | see below | todo |
| M2.4 | Soft keyboard / IME via GameTextInput | laf-backend | see below | todo |
| M2.5 | Lifecycle robustness (surface loss, SIGKILL, recovery) | laf-backend | see below | todo |
| M2.6 | Files in app-specific storage | build-engineer | see below | todo |
| M2.7 | M2 review + tablet checklist | reviewer | see below | todo |

**M2.1** (ADR 0006 §1-2, §10). Axes enabled in M1.3
(`GameActivityPointerAxes_enableAxis`: PRESSURE, TILT, ORIENTATION,
TOUCH_MAJOR/MINOR; without it pressure is always 0, NOTES §4).
`GameActivityMotionEvent` → `MouseDown/Move/Up`; stylus →
`PointerType::Pen` with pressure, finger → `PointerType::Touch`; every
historical sample emitted; only the first active pointer drives the mouse
for now (gesture/palm logic is M3). Coordinates in window pixels divided
by `Window::scale()`. Max 8 pointers per event
(`GAMEACTIVITY_MAX_NUM_POINTERS_IN_MOTION_EVENT`, `GameActivityEvents.h:159`):
guard `actionIndex >= pointerCount`, never define the `_OVERRIDE`.
Stylus button mapping kept but not testable (M-Pencil 3 `buttonState` is
always 0). `ACTION_CANCEL` here: plain `MouseUp` placeholder, replaced in
M3.1. Write the input translation so it is host-testable (pure C++ fed
with synthetic events).
Accept: tablet, pencil tool: fast strokes stay smooth (no polygonal
corners); with pressure dynamics on (`src/app/ui/dynamics_popup.cpp:538`)
light/heavy pressure changes the size; menus and buttons react to pen and
finger taps.

**M2.2** (ADR 0006 §3). Hover arrives through `AsepriteActivity`'s
`OnHoverListener` (keep it). `HOVER_ENTER/MOVE/EXIT` →
`MouseEnter/MouseMove/MouseLeave` with `PointerType::Pen`; no
`AXIS_DISTANCE` (always 0), only a "pen in range" flag; defer
`MouseLeave` ~50 ms and drop it when a stylus `DOWN` follows (Android
sends `HOVER_EXIT` just before `DOWN`). Log the actual order once
(verbose) so the tablet run confirms it.
Accept: tablet: hovering shows the brush preview following the tip; it
disappears when the pen leaves hover range; no preview flicker when the
tip touches down or lifts.

**M2.3** (ADR 0006 §11). `AKEYCODE_*` → `os::KeyScancode` table
(`laf/os/android/keys.cpp`), `metaState` → `KeyModifiers`,
`System::isKeyPressed/keyModifiers` (`laf/os/system.h:187,193`). Key
filter returns false for volume/camera/zoom/media/power/brightness keys
(system handles them); Back (key and gesture) → `kKeyEsc`, never finishes
the activity. Key 718 is not mapped here (M3.4); log it only.
Accept: tablet with the Huawei Magnetic Keyboard: B selects brush, Ctrl+Z
undoes, Space+drag pans, Shift+click draws a line; Back gesture closes an
open dialog/menu; volume keys change the volume while Aseprite is in
front.

**M2.4** `System::setTextInput()` (`laf/os/system.h:205`) shows/hides the
soft keyboard (`GameActivity_showSoftInput/hideSoftInput`); text from
GameTextInput state diff → `KeyDown` with `unicodeChar`; Backspace/Enter
mapped. With the Magnetic Keyboard attached the soft keyboard should not
pop up (system decides; just verify).
Accept: tablet without the keyboard: Sprite > Properties (or layer
rename): tapping the field opens the on-screen keyboard, typed text (incl.
Cyrillic) appears, Enter confirms, keyboard hides when the dialog closes.

**M2.5** (ADR 0007). Surface destroy/recreate without losing documents or
GPU resources (M1.6); split-screen / rotation → `ResizeWindow`;
`LOW_MEMORY` stays log-only; verify M1.9's flush under real edits.
Accept: tablet: (a) draw, Home, open 5 other apps, return — drawing intact;
(b) draw, Home, wait 3 s, **swipe the app away from Recents**, relaunch —
Aseprite offers to recover the session with the latest strokes;
(c) same with `adb shell am force-stop com.github.maksimcharyev.aseprite`;
(d) rotate while drawing — no crash.

**M2.6** Default open/save directory = external files dir
(`/sdcard/Android/data/<appId>/files`); Aseprite's own file selector
works there; `README.txt` created there explaining `adb push/pull`.
Accept: tablet: `adb push test.aseprite /sdcard/Android/data/com.github.maksimcharyev.aseprite/files/`,
open it via File > Open, edit, save, `adb pull` it back and open on
desktop Aseprite.

---

## M3 — Touch UX

The probe made several items simpler (ADR 0006, NOTES §9 M0.6): the
system already rejects a resting palm after its `ACTION_CANCEL`; there is
no PALM tool type and no FLAG_CANCELED to handle; touchMajor heuristics
are not needed for v1; the pen double-tap is a plain key 718.

| # | Task | Owner | Accept | Status |
|---|------|-------|--------|--------|
| M3.1 | Palm rejection + finger-stream state machine in the backend | laf-backend | see below | todo |
| M3.2 | App side of cancel + touch preferences + finger pan | touch-ux | see below | todo |
| M3.3 | Two-finger pan + pinch zoom | laf-backend | see below | todo |
| M3.4 | Virtual gesture keys in laf (double-tap 718, 2/3-finger taps) | laf-backend | see below | todo |
| M3.5 | Gesture shortcuts + `ToggleEraser` command | touch-ux | see below | todo |
| M3.6 | On-screen modifier bar (+ Back policy review) | touch-ux | see below | todo |
| M3.7 | Touch-friendly sizing pass | touch-ux | see below | todo |
| M3.8 | M3 review + tablet checklist | reviewer | see below | todo |

**M3.1** (ADR 0006 §4). In laf: pen-in-range flag (hover or touch,
deferred exit), last pen activity time; a finger stream that starts while
the pen is in range or within the grace period (default 500 ms, from
`os::TabletOptions` Android field) is ignored to its end; pen arrival
cancels an active finger stream; `ACTION_CANCEL` with a held button →
`KeyUp(kKeyPointerCancel)` then `MouseUp` (no Esc KeyDown ever); > 3
finger pointers (incl. the 8-pointer cap) = palm → cancel + ignore. Host
unit tests with synthetic sequences modelled on the M0.6 logs (palm
first, pen first, CANCEL after 0.1 s and 2 s).
Accept: build: host tests pass. Tablet (with M3.2): write a word with the
palm resting on the screen — no stray dots or lines, no canvas jumps;
put the palm down right after lifting the pen — nothing happens.

**M3.2** (ADR 0006 §4-5, §8 context). `DrawingState::onKeyUp` treats
`kKeyPointerCancel` like `kKeyEsc` (`src/app/ui/editor/drawing_state.cpp:323`);
name the scancode in `src/ui/shortcut.cpp` without exposing it in the
shortcut UI; prefs `touch.draw_with_finger` (default off) and
`touch.pen_rejection_timeout` (500 ms) in `data/pref.xml` + Options,
timeout passed via `os::TabletOptions` (`src/app/app.cpp:315`,
`cmd_options.cpp:1017`); with finger drawing off a one-finger drag on the
canvas scrolls.
Accept: tablet: with "Draw with finger" off a finger drag pans and never
paints; turn it on, make a rectangular selection, start a finger stroke
inside it and rest the palm until the system cancels — the stroke
disappears, Edit > Undo History has no entry for it **and the selection
is still there**.

**M3.3** (ADR 0006 §7). Clean two-finger stream only (exactly 2
pointers, not rejected, no CANCEL, movement beyond tap slop) → precise
`MouseWheel` (`PointerType::Touchpad`) + `TouchMagnify`, as
`laf/os/osx/view.mm:549,576`; consumed by
`state_with_wheel_behavior.cpp:71,513`. A third pointer ends the gesture.
Accept: tablet: two-finger drag pans smoothly, pinch zooms around the
fingers, no dot on gesture start even with finger drawing on; resting the
palm never pans.

**M3.4** (ADR 0006 §6). laf `keys.h`: names for `kKeyUnknown1..4`
(`PenDoubleTap`, `TwoFingerTap`, `ThreeFingerTap`, `PointerCancel`),
values unchanged. Key 718: first DOWN emits `PenDoubleTap`
KeyDown+KeyUp; DOWNs within 100 ms of the last, all UPs and repeats are
dropped (measured pairs 6-16 ms apart). Taps per ADR 0006 §6 (max count
exactly 2/3, all down within 150 ms, done within 300 ms, ≤ ~24 px
movement, no CANCEL). Host unit tests.
Accept: build: host tests incl. a "two pairs 6 ms / 16 ms apart = one
gesture" and "two double-taps 400 ms apart = two gestures" case. Tablet
(with M3.5).

**M3.5** (ADR 0006 §6, §8). App names in `src/ui/shortcut.cpp`,
`src/app/app_menus.cpp:230`, `src/app/script/keys.cpp:125`;
`keyboard_shortcuts.cpp:47-66` reads `android=` under `#elif LAF_ANDROID`;
defaults in `data/gui.xml`: PenDoubleTap → `ToggleEraser`, TwoFingerTap →
`Undo`, ThreeFingerTap → `Redo`. New `ToggleEraser` command
(`cmd_toggle_eraser.cpp`, `commands_list.h`, `data/strings/en.ini`)
switching the selected tool to the eraser and back via
`ToolBar::selectTool` (pattern `src/app/ui/window_with_hand.cpp:31-59`;
not `eraserTipProximity()`, which the editor resets on every pen
message, `editor.cpp:1837-1847`). There is no existing toggle command
(only the tool key `E`, `data/gui.xml:601`).
Accept: tablet: double-tap the pen (hovering or away from the screen) →
toolbar shows the eraser and the next stroke erases; double-tap again →
previous tool back; 10 double-taps = 10 toggles (never two per tap);
two-finger tap undoes, three-finger tap redoes; all three visible and
rebindable in Edit > Keyboard Shortcuts.

**M3.6** On-screen bar (Android-only or pref-gated widget in
`src/app/ui/`): sticky Shift / Ctrl / Alt / Space, Undo, Redo, Esc, Eraser
toggle (`ToggleEraser`, the fallback of ADR 0006 §9); modifiers fed
through `System::keyModifiers()` of the Android system. Write an ADR on
placement before coding. Review the Back policy of ADR 0006 §11 with the
user (deselect on Back when nothing is open: keep or no-op).
Accept: tablet without keyboard: Shift on + pencil draws straight lines;
Ctrl on + click picks a layer as on desktop; bar can be hidden.

**M3.7** Review hit targets and scale defaults (M1.10 feedback);
adjust `ui_scale`/`screen_scale` defaults, scrollbar thickness, tab sizes
via skin theme where needed. Must not change desktop defaults.
Accept: tablet: all toolbar buttons reliably hit with a finger on the
first try in a 20-tap test.

---

## M4 — Polish

| # | Task | Owner | Accept | Status |
|---|------|-------|--------|--------|
| M4.1 | SAF Open / Save As (ACTION_OPEN_DOCUMENT / CREATE_DOCUMENT) | laf-backend (JNI) + touch-ux (menu wiring) | see below | todo |
| M4.2 | Clipboard (text + image) via `ClipboardManager` | laf-backend | see below | todo |
| M4.3 | 144 Hz and latency | laf-backend | see below | todo |
| M4.4 | Logs and crash collection | build-engineer | see below | todo |
| M4.5 | Final review + full regression checklist | reviewer | see below | todo |

**M4.1** Implement `laf/dlgs` for Android using SAF intents in
`AsepriteActivity`; documents are copied to/from cache with the content
URI remembered for Save. Write an ADR first (copy vs fd-based I/O).
Accept: tablet: open a PNG from Downloads/Gallery, edit, Save → file in
Downloads updated; Save As to a new file in a chosen folder.

**M4.2** `clip_android.cpp` in the laf fork (JNI to `ClipboardManager`,
images via a `content://` FileProvider or PNG in cache).
Accept: tablet: copy text from a browser into a layer name field; copy a
selection in Aseprite and paste into another app that accepts images
(e.g. Notes).

**M4.3** `ANativeWindow_setFrameRate` (API 30, guarded), swap interval,
partial redraws, measure frame times; GL path default.
Accept: tablet: `adb shell dumpsys gfxinfo com.github.maksimcharyev.aseprite`
or the in-app FPS log shows ≥ 120 fps while panning a 256×256 sprite;
stroke latency visibly comparable to the system Notes app.

**M4.4** Log file in the external files dir (rotating), `LOG` level
configurable; on crash, tombstone + last log are what the user sends.
No network upload (EULA). Document `adb bugreport`/`adb pull` steps.
Accept: tablet: force a test crash in a debug menu → after relaunch, the
log file and instructions are where the doc says.

---

## Risks to watch

- Long synchronous operations blocking lifecycle commands (ADR 0001).
- BGRA assumptions hidden in laf/Aseprite (ADR 0005).
- HarmonyOS consuming pen double-tap globally (NOTES.md §3).
- `LAF_LINUX` uses added upstream between rebases (ADR 0003).
- Process death while in background: rely on data recovery (M2.5).
