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
| M0.4 | Cross-build non-GUI libs | build-engineer | see below | **done** (2026-10-22), branch `mobile/m0-libs`, uncommitted; see NOTES section 6 |
| M0.5 | Gradle skeleton + GameActivity "hello" APK | build-engineer | see below | **done** (2026-10-22), branch `mobile/m0-gradle`; passed on the tablet (NOTES §9, "M0.5 on the device") |
| M0.6 | Input probe APK | touch-ux | see below | todo |
| M0.7 | M0 review + device info checklist | reviewer | see below | todo |

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

| # | Task | Owner | Accept | Status |
|---|------|-------|--------|--------|
| M1.1 | `laf/os/android` skeleton: system, event queue, window, screen | laf-backend | see below | todo |
| M1.2 | CPU raster window path + `hello_laf` APK | laf-backend | see below | todo |
| M1.3 | EGL context + Skia GL surface | laf-backend | see below | todo |
| M1.4 | Asset packaging + extraction + env (ADR 0002) | build-engineer | see below | todo |
| M1.5 | Aseprite as `libaseprite.so` in the APK | build-engineer | see below | todo |
| M1.6 | First-run defaults for Android | touch-ux | see below | todo |
| M1.7 | M1 review + tablet checklist | reviewer | see below | todo |

**M1.1 `laf/os/android` skeleton.** New files under `laf/os/android/`:
`main.cpp` (`android_main` → `app_main`, see ADR 0001), `system.h/.cpp`
(`SystemAndroid`, base for `SkiaSystem` in
`laf/os/skia/skia_system.h:21-31`; capabilities: none of
MultipleWindows/CanResizeWindow/CustomMouseCursor; GPU switch on),
`event_queue.h/.cpp` (`getEvent` = `ALooper_pollOnce` + APP_CMD processing
+ input buffer swap), `window.h/.cpp` (single window bound to
`app->window`; `ResizeWindow` event on `APP_CMD_WINDOW_RESIZED` /
`CONFIG_CHANGED`; `CloseWindow` never sent by the OS), `screen.cpp`
(size and density from `AConfiguration`). Wire into
`laf/os/CMakeLists.txt` and `laf/os/skia/skia_window.h:12-23`,
`skia_system.h:21-31`, `laf/os/common/system.cpp:44-63`. No input yet.
Accept: laf builds for Android with `LAF_BACKEND=skia`; desktop build OK.

**M1.2 CPU raster path.** `SkiaWindowAndroid::onPaint` copies the Skia
bitmap to `ANativeWindow_lock` buffers (`WINDOW_FORMAT_RGBA_8888`), like
`laf/os/skia/skia_window_x11.cpp:58-100`; surface recreated on
`APP_CMD_INIT_WINDOW`, painting skipped while there is no window. Build
`laf/examples/hello_laf.cpp` as an alternative APK variant
(`-PlafExample=hello_laf`).
Accept: tablet shows the hello_laf window content, colours correct (red is
red — validates the RGBA decision in ADR 0005), survives Home → back.

**M1.3 EGL + Skia GL.** `laf/os/gl/gl_context_egl.h` implementing
`os::GLContext` (`laf/os/gl/gl_context.h`) with EGL 1.4, GLES 3 context,
window surface from `ANativeWindow`; destroy surface on
`APP_CMD_TERM_WINDOW`, recreate on `INIT_WINDOW` without losing the
`GrDirectContext` when possible. `SkiaGL::attachGL`
(`laf/os/skia/skia_gl.cpp:31`) must work with it.
Accept: hello_laf runs with GPU on; logcat shows "OS: Using OpenGL
backend"; Home → back and rotate 10× without crash or black screen.

**M1.4 Assets.** Gradle task copies the CMake `copy_data` output
(`src/CMakeLists.txt:179`) to `assets/data/`, writes
`assets/data.manifest` with a content hash; native extractor in
`laf`-independent Android glue (`android/app/src/main/cpp/`) runs before
`app_main`, sets `HOME`, `XDG_CONFIG_HOME`, `TMPDIR` (ADR 0002), and
logs extraction time. `base::get_user_docs_folder()`
(`laf/base/fs_unix.h:199`) returns the external files dir on Android.
Accept: tablet, first launch logs "extracted N files in X ms", second
launch logs "data up to date";
`adb shell run-as com.github.maksimcharyev.aseprite ls files/aseprite/data`
lists `gui.xml pref.xml widgets ...`.

**M1.5 Aseprite in the APK.** `src/CMakeLists.txt:184-229`: on `ANDROID`
build `add_library(aseprite SHARED main/main.cpp)` linked to `app-lib`
and laf's `android_main`; fix compile errors under `LAF_ANDROID`
(`src/app/app.cpp:82,309,395,501`, `src/app/file_selector.cpp:23-105`,
`src/app/script/app_os_object.cpp:29`,
`src/app/ui/editor/state_with_wheel_behavior.cpp:285`,
`src/ui/window.cpp:866`, `src/app/pref/preferences.cpp:56`). Gradle
`externalNativeBuild` drives the root CMake with the ADR 0003 options.
Accept: tablet shows the Aseprite main window with the Home tab, readable
(even if tiny) and with correct colours; logcat has no `FATAL`.

**M1.6 First-run defaults.** Under `#if LAF_ANDROID` in
`src/app/pref/preferences.cpp:55-65`: `gpu_acceleration=true`,
`screen_scale` and `ui_scale` from screen density (e.g. density ≥ 2.5 →
screen 2, ui 2; document the formula), native file dialog off,
`experimental.multipleWindows=false`.
Accept: tablet, fresh install: Home tab text and icons are comfortably
legible at arm's length; Edit > Preferences shows the chosen values.

**M1.7 Review.** Desktop build + tests, guards (`LAF_ANDROID`/`ANDROID`),
no EULA-violating additions, and a tablet checklist for the user.
Accept: user confirms checklist items.

---

## M2 — Input, lifecycle, files

| # | Task | Owner | Accept | Status |
|---|------|-------|--------|--------|
| M2.1 | Pen + finger → mouse events with pressure and history | laf-backend | see below | todo |
| M2.2 | Hover → cursor/brush preview | laf-backend | see below | todo |
| M2.3 | Hardware keyboard → scancodes + modifiers | laf-backend | see below | todo |
| M2.4 | Soft keyboard / IME via GameTextInput | laf-backend | see below | todo |
| M2.5 | Lifecycle robustness (pause/resume, surface loss, process death) | laf-backend | see below | todo |
| M2.6 | Files in app-specific storage | build-engineer | see below | todo |
| M2.7 | M2 review + tablet checklist | reviewer | see below | todo |

**M2.1** (ADR 0006 §1-2). `GameActivityMotionEvent` → `MouseDown/Move/Up`
with `PointerType::Pen/Eraser/Touch/Mouse`, pressure, every historical
sample emitted; only the first active pointer drives the mouse for now;
stylus primary button → right button. Coordinates in window pixels then
divided by `Window::scale()`.
Accept: tablet, pencil tool: smooth lines with fast strokes (no
polygonal corners), pressure works when the brush has pressure dynamics
enabled (`src/app/ui/dynamics_popup.cpp:538`), menus and buttons react to
pen taps and finger taps.

**M2.2** Hover enter/move/exit → `MouseEnter/MouseMove/MouseLeave`.
Accept: tablet, hovering the pen over the canvas shows the brush preview
following the tip; it disappears when the pen moves away.

**M2.3** `AKEYCODE_*` → `os::KeyScancode` table
(`laf/os/android/keys.cpp`), `metaState` → `KeyModifiers`,
`System::isKeyPressed/keyModifiers` (`laf/os/system.h:187,193`); Back key →
`kKeyEsc` (never finishes the activity).
Accept: tablet with a Bluetooth/USB keyboard: B selects brush, Ctrl+Z
undoes, Space+drag pans; Back key closes an open dialog.

**M2.4** `System::setTextInput()` (`laf/os/system.h:205`) shows/hides the
soft keyboard (`GameActivity_showSoftInput/hideSoftInput`); text from
GameTextInput state diff → `KeyDown` with `unicodeChar`; Backspace/Enter
mapped.
Accept: tablet, Sprite > Properties (or layer rename): tapping the field
opens the on-screen keyboard, typed text (incl. Cyrillic) appears, Enter
confirms, keyboard hides when the dialog closes.

**M2.5** Surface destroy/recreate without losing documents; on
`APP_CMD_PAUSE` trigger a data-recovery backup; `APP_CMD_LOW_MEMORY` →
log; resizing (split-screen / rotation) → `ResizeWindow`.
Accept: tablet: draw something, press Home, open 5 other apps, return —
drawing intact; rotate during drawing — no crash; force-stop via
`adb shell am force-stop com.github.maksimcharyev.aseprite`, relaunch —
Aseprite offers to recover the session.

**M2.6** Default open/save directory = external files dir
(`/sdcard/Android/data/<appId>/files`); Aseprite's own file selector
works there; `README.txt` created there explaining `adb push/pull`.
Accept: tablet: `adb push test.aseprite /sdcard/Android/data/com.github.maksimcharyev.aseprite/files/`,
open it via File > Open, edit, save, `adb pull` it back and open on
desktop Aseprite.

---

## M3 — Touch UX

| # | Task | Owner | Accept | Status |
|---|------|-------|--------|--------|
| M3.1 | Palm rejection (stylus priority, PALM, ACTION_CANCEL) | laf-backend + touch-ux prefs | see below | todo |
| M3.2 | Two-finger pan + pinch zoom | laf-backend | see below | todo |
| M3.3 | Virtual gesture keys + defaults (undo/redo taps) | touch-ux | see below | todo |
| M3.4 | Pen double-tap → `ToggleEraser` | touch-ux | see below | todo |
| M3.5 | On-screen modifier bar | touch-ux | see below | todo |
| M3.6 | Touch-friendly sizing pass | touch-ux | see below | todo |
| M3.7 | M3 review + tablet checklist | reviewer | see below | todo |

**M3.1** (ADR 0006 §3,5). Finger contacts ignored while pen active /
within timeout; `TOOL_TYPE_PALM` dropped; `ACTION_CANCEL` on a pointer
that holds a button → `KeyDown(kKeyEsc)` + `KeyUp(kKeyEsc)`, then
`MouseUp` (the loop is cancelled only in `DrawingState::onKeyUp`,
`drawing_state.cpp:320-330`; `MouseUp` first would commit the stroke);
no Esc when no button is held (it would close menus / deselect). Preferred
follow-up: an explicit cancel hook instead of the Esc emulation. Prefs
"Draw with finger" and "Pen rejection timeout" in `data/pref.xml` +
Options.
Accept: tablet: write a word with the palm resting on the screen — no
stray dots or lines; with "Draw with finger" off, a finger does not
paint; after a system-cancelled stroke no pixels from the cancelled
pointer remain and Edit > Undo history has no extra entry for it.

**M3.2** (ADR 0006 §4). Two pointers → precise `MouseWheel`
(`PointerType::Touchpad`) for translation and `TouchMagnify` for scale,
as in `laf/os/osx/view.mm:549,576`; consumed by
`state_with_wheel_behavior.cpp:71,513`.
Accept: tablet: two-finger drag pans the canvas smoothly, pinch zooms
around the fingers, no accidental paint on gesture start.

**M3.3** (ADR 0006 §6). Name `kKeyUnknown1..` slots in
`src/ui/shortcut.cpp` (and `src/app/app_menus.cpp:230`,
`src/app/script/keys.cpp:125`), emit them from laf for two/three-finger
taps, read an `android=` attribute in
`src/app/ui/keyboard_shortcuts.cpp:47-66`, defaults in `data/gui.xml`.
Accept: tablet: two-finger tap undoes, three-finger tap redoes; both
visible and rebindable in Edit > Keyboard Shortcuts.

**M3.4** (ADR 0006 §6-7). Pen double-tap (evdev `KEY_F20`; Android key
code expected 718, confirm in M0.6) → `PenDoubleTap`, debounced: a burst
of key-downs within ~100 ms is one gesture, key-ups ignored (one double-tap
sends two DOWN/UP pairs, NOTES.md §9; otherwise `ToggleEraser` fires twice).
New `ToggleEraser` command using
`ActiveToolManager::eraserTipProximity/regularTipProximity`
(`src/app/tools/active_tool.cpp:151,159`).
Accept: tablet: double-tap the pen → next stroke erases; double-tap
again → back to the previous tool (one toggle per double-tap, never two).
If the probe showed no key event for the double-tap, this task becomes
"bar button only" and the user is told.

**M3.5** On-screen bar (Android-only or pref-gated widget in
`src/app/ui/`): sticky Shift / Ctrl / Alt / Space, Undo, Redo, Esc, Eraser
toggle; modifiers fed through `System::keyModifiers()` of the Android
system so all existing code sees them. Write an ADR on placement before
coding.
Accept: tablet without keyboard: Shift on + pencil draws straight lines;
Ctrl on + click picks a layer/colour as on desktop; bar can be hidden.

**M3.6** Review hit targets and scale defaults (M1.6) on the tablet; adjust
default `ui_scale`/`screen_scale`, scrollbar thickness, tab sizes via skin
theme where needed. Must not change desktop defaults.
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
