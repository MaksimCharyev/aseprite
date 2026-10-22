# ADR 0001: GameActivity (AGDK) as the Android glue, app_main on the glue thread

- Status: Accepted
- Date: 2026-10-21
- Owner: mobile-architect

## Context

Aseprite is a C++ program whose entry point is `app_main()`
(`src/main/main.cpp:88`), called from the laf `main()`
(`laf/os/common/main.cpp:60-69`). It owns its own blocking main loop: the UI
pumps `os::EventQueue::getEvent(ev, timeout)` from
`ui::Manager::generateMessagesFromOSEvents()` (`src/ui/manager.cpp:403-438`)
and expects to be the only thread touching UI state.

On Android we need: a surface to render into (`ANativeWindow`), lifecycle
callbacks (window created/destroyed, pause/resume, config change), touch,
stylus and key events with full `MotionEvent` data (tool type, button state,
flags, history), and soft-keyboard text input for `ui::Entry`, which already
calls `os::System::setTextInput()` (`laf/os/system.h:205`,
`src/ui/entry.cpp:170,301,317`).

CONTEXT.md D4 prefers GameActivity over NativeActivity unless an ADR says
otherwise.

## Decision

Use **GameActivity** from Jetpack `androidx.games:games-activity` (stable
4.4.2 as of 2026-05; pin an exact version in Gradle), linked as the prefab
**static** library `game-activity_static` together with its
`native_app_glue`.

- A minimal Java subclass `AsepriteActivity extends GameActivity` exists only
  for things GameActivity cannot do from native code (later: SAF intents,
  clipboard). No Kotlin.
- `android_main(struct android_app*)` lives in `laf/os/android/` and is the
  Android counterpart of `laf/os/common/main.cpp`. It stores the
  `android_app*` in the laf Android system, prepares the environment (see
  ADR 0002) and calls `app_main(argc, argv)` **on the glue thread**. That
  thread becomes the Aseprite "main" thread.
- `os::EventQueueAndroid::getEvent(ev, timeout)` drives the glue: it calls
  `ALooper_pollOnce(timeoutMs, ...)`, processes `APP_CMD_*` via
  `android_poll_source::process`, drains `android_app_swap_input_buffers()`
  and turns them into `os::Event`s. This mirrors how the X11 queue waits on
  the display fd with a timeout (`laf/os/x11/event_queue.cpp:78`).
- `APP_CMD_TERM_WINDOW` is handled synchronously inside `getEvent()` (destroy
  the EGL surface before returning) because the glue blocks the Java UI thread
  until the command is processed.
- Motion events: replace the default motion filter (it only accepts
  `source & SOURCE_TOUCHSCREEN`, see `default_motion_filter` in
  `android_native_app_glue.c`) with one that also accepts stylus and mouse
  sources and hover events. Key filter: keep volume/camera excluded, keep
  everything else (M-Pencil 3 double-tap arrives as evdev `KEY_F20` on the
  pen's BT HID keyboard device; Android key code expected 718, confirm in
  M0.6; one double-tap = two DOWN/UP pairs, debounced per ADR 0006 §6;
  see NOTES.md §9).

## Alternatives

1. **NativeActivity** (framework class). No Java code at all, but: input via
   `AInputQueue` only, no soft-keyboard text API (the usual workaround is JNI
   to `InputMethodManager` plus a hidden `EditText`), renders to the window
   directly (no `SurfaceView`), no `WindowInsets` helpers, updated only with
   the OS. We would have to write the IME and insets parts ourselves.
2. **Plain Activity + SurfaceView + hand-written JNI** (SDL-style). Full
   control, but a lot of glue we would own and maintain; GameActivity is
   exactly this, already written and maintained.
3. **SDL2/SDL3 as the platform layer under laf.** Brings its own Android
   Java glue, but adds a large dependency between laf and the OS, duplicates
   laf's event model, and still needs Huawei-specific key handling.

## Consequences

- Gradle needs `prefab true` and a `games-activity` dependency; the native
  CMake does `find_package(game-activity REQUIRED CONFIG)` and links
  `game-activity::game-activity_static`.
- One Java file plus `AndroidManifest.xml` with
  `android:configChanges` covering orientation, screenSize, screenLayout,
  smallestScreenSize, keyboard, keyboardHidden, navigation, touchscreen,
  density, fontScale, locale, layoutDirection, uiMode, colorMode, so the
  activity is never recreated (rotation, attaching/detaching the Huawei
  Magnetic Keyboard changes keyboard/navigation, system font or language
  changes). `app_main` is not re-entrant: a recreation would start a second
  `android_main` in the same process.
- IME: `System::setTextInput(true, caret)` maps to
  `GameActivity_showSoftInput()`; text arrives through `GameTextInput` state
  and is converted into `os::Event::KeyDown` with `unicodeChar`.
- Because `app_main` blocks the glue thread, any long synchronous operation
  in Aseprite (big file load) delays lifecycle commands; the glue's
  ANR protection (4.4.0) helps, but long tasks are a known risk to watch.
- When `app_main` returns (File > Exit) we call `GameActivity_finish()` and
  return from `android_main`; the process is not reused for a second run
  (static state in Aseprite is not designed for re-entry), so the activity
  uses `android:launchMode="singleTask"` and we `exit(0)` after finishing.

## Sources

- https://developer.android.com/games/agdk/game-activity
- https://developer.android.com/games/agdk/game-activity/migrate-native-activity
- https://developer.android.com/jetpack/androidx/releases/games
- https://android.googlesource.com/platform/frameworks/opt/gamesdk/+/refs/heads/main/game-activity/
