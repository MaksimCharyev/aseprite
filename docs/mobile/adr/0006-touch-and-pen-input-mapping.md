# ADR 0006: How touch, stylus, palm and pen gestures map onto existing laf/Aseprite events

- Status: **Accepted** (2026-10-22, after the M0.6 input probe on the
  tablet, NOTES.md §9 "M0.6 input probe on the device")
- Date: 2026-10-21, revised 2026-10-22
- Owner: mobile-architect; implementers: mobile-laf-backend (M2, M3.1-M3.2),
  mobile-touch-ux (M3.3-M3.6)

## Context

### What the code already supports

- `os::PointerType` has `Touch`, `Pen`, `Eraser`, `Touchpad`
  (`laf/os/pointer_type.h:14`), and `os::Event` carries pressure, precise
  wheel deltas and a magnification factor (`laf/os/event.h`).
- `ui::Manager` turns OS events into `ui::Message`s
  (`src/ui/manager.cpp:403-617`), including `TouchMagnify` (`:602`).
  `AppEnter`/`AppLeave` exist in `laf/os/event.h:34-35` but the manager
  ignores them (see ADR 0007).
- The editor picks the eraser by pointer type on **every** mouse message:
  `Editor::updateToolByTipProximity()` (`src/app/ui/editor/editor.cpp:1837-1847`,
  called at `:2177,2212,2225,2255`) calls
  `ActiveToolManager::eraserTipProximity()` for `PointerType::Eraser` and
  `regularTipProximity()` otherwise (`src/app/tools/active_tool.cpp:151-165`).
  So a "proximity eraser" set from a command would be reset by the next pen
  move; a toggle must go through the selected tool instead.
- Pinch zoom: `StateWithWheelBehavior::onTouchMagnify()`
  (`src/app/ui/editor/state_with_wheel_behavior.cpp:513`), two-finger
  scrolling via precise wheel in `onMouseWheel()` (`:71-120`). macOS produces
  exactly these (`laf/os/osx/view.mm:549,576`).
- Stroke cancel: `DrawingState::onKeyUp` cancels the tool loop when the
  scancode is `kKeyEsc` (`src/app/ui/editor/drawing_state.cpp:320-330`); a
  `MouseUp` before that commits the stroke. **But** `DrawingState::onKeyDown`
  (`:302-317`) executes the `Cancel` command bound to Esc, and that binding is
  `type=all` (`data/gui.xml:36-38`), which runs `DiscardBrush` and
  `InputChain::cancel` (`src/app/commands/cmd_cancel.cpp:56-72`) →
  `DocView::onCancel` → `DeselectMask` (`src/app/ui/doc_view.cpp:659-668`).
  A synthesized Esc **KeyDown** would therefore drop the user's selection.
- While a stroke is in progress `DrawingState::onKeyDown` only lets
  `Zoom/Undo/Redo/Cancel` through and swallows other shortcuts
  (`drawing_state.cpp:306-317`), so a gesture shortcut fired mid-stroke is
  ignored for free.
- Shortcuts are data-driven (`data/gui.xml`, platform attribute chosen in
  `src/app/ui/keyboard_shortcuts.cpp:47-66`; on Android the `#else` branch
  picks the `linux` attribute today). Scancodes `kKeyUnknown1..8` exist
  (`laf/os/keys.h:140-147`) but have no names in `src/ui/shortcut.cpp`,
  `src/app/app_menus.cpp:230`, `src/app/script/keys.cpp:125-132`.
- There is **no existing eraser toggle command**: the only eraser shortcut
  is the tool key `<key tool="eraser" shortcut="E" />` (`data/gui.xml:601`),
  which selects the eraser but does not switch back
  (`src/app/commands/commands_list.h` has no such command).

### What the tablet reports (verified, NOTES.md §9, M0.6)

- **Pen double-tap** (M-Pencil 3, no side button): `KeyEvent`
  `keyCode=718`, `scanCode=190`, source KEYBOARD, `flags=FROM_SYSTEM`,
  device "HUAWEI M-Pencil 3 Mouse" (vendor 0x12d1, product 0x10a5). It
  reaches both `Activity.dispatchKeyEvent` and native code once the
  GameActivity key filter accepts all keys. Each double-tap = **two
  DOWN/UP pairs 6-16 ms apart**. Works while hovering and with the pen away
  from the screen. The system setting `Double_Touch_index=0` did not
  intercept it.
- **Stylus** (device "huawei,ts_pen", source 0x5002): only
  `TOOL_TYPE_STYLUS`, never `TOOL_TYPE_ERASER`; `buttonState` always 0.
  `HOVER_ENTER/MOVE/EXIT` arrive through the Java `OnHoverListener` path of
  `AsepriteActivity` (GameActivity alone does not route hover reliably,
  NOTES.md §4 "M0.6 probe APK"). `AXIS_DISTANCE` is always 0 (in range /
  out of range only). Pressure (0..~0.86), tilt, orientation are live only
  after `GameActivityPointerAxes_enableAxis()`; by default GameActivity
  reports X/Y only.
- **Palm**: several `TOOL_TYPE_FINGER` pointers (5-8) with larger
  `touchMajor` (~48 vs ≤ ~23) and pressure (~15 vs ≤ ~7). No
  `TOOL_TYPE_PALM`, no `FLAG_CANCELED` (API 31). HarmonyOS sends
  `ACTION_CANCEL` for the whole finger stream 0.1-2 s after the palm lands,
  and afterwards swallows the resting palm entirely while the pen draws
  (pen DOWN/MOVE/UP and hover keep arriving).
- **Fingers**: 1/2/3-finger taps and pinch arrive as normal
  `POINTER_DOWN/UP`; up to **8** pointers were seen during palm contact.

## Decision

Pure signal translation lives in laf's Android backend
(`laf/os/android/`); anything a user may want to change lives in Aseprite
preferences / keyboard shortcuts.

1. **Axes.** At startup the backend enables `AXIS_PRESSURE`,
   `AXIS_TILT`, `AXIS_ORIENTATION`, `AXIS_TOUCH_MAJOR`, `AXIS_TOUCH_MINOR`
   with `GameActivityPointerAxes_enableAxis()` (once, before input arrives).
   Tilt/orientation are read but not forwarded in v1 (laf has no field;
   keep the code path ready).
2. **Stylus** (`TOOL_TYPE_STYLUS`) → `MouseDown/Move/Up`, `LeftButton`,
   `PointerType::Pen`, pressure from `AXIS_PRESSURE`; every historical
   sample becomes its own `MouseMove`. `TOOL_TYPE_ERASER` →
   `PointerType::Eraser` (kept for other pens; never seen on M-Pencil 3).
   `BUTTON_STYLUS_PRIMARY` → `RightButton`, secondary → `MiddleButton`
   (kept for other pens; `buttonState` is always 0 on M-Pencil 3).
3. **Hover.** Keep `AsepriteActivity`'s `OnHoverListener` →
   `processMotionEvent()` (returns `true`). `HOVER_ENTER` → `MouseEnter`,
   `HOVER_MOVE` → `MouseMove` (no button, `PointerType::Pen`),
   `HOVER_EXIT` → `MouseLeave`. Because `AXIS_DISTANCE` is always 0, the
   backend only tracks a boolean **pen in range** = hovering or touching.
   Android normally sends `HOVER_EXIT` immediately before the stylus
   `ACTION_DOWN` and `HOVER_ENTER` after `ACTION_UP`; the backend must not
   treat that exit as "pen left" (defer `MouseLeave` and the
   out-of-range transition by ~50 ms and drop them if a stylus `DOWN`
   arrives first). Verify the exact order in the M2.2 tablet test.
4. **Palm rejection** (no PALM tool type exists here, so the strategy is
   built on what the device does send):
   1. **Stylus priority.** A finger *stream* (from the first finger
      `ACTION_DOWN` until `ACTION_UP`/`ACTION_CANCEL`) that **starts**
      while the pen is in range, touching, or within the grace period after
      the pen left range is ignored entirely, including later
      `POINTER_DOWN`s and even after the pen leaves. Grace period default
      **500 ms**, preference `touch.pen_rejection_timeout` (0-2000 ms),
      passed to laf through a new `#if LAF_ANDROID` field of
      `os::TabletOptions` (`laf/os/tablet_options.h`, already applied by
      the app at `src/app/app.cpp:315` and
      `src/app/commands/cmd_options.cpp:1017`).
      (500 ms covers the gap between lifting the pen and the palm landing;
      the system's own CANCEL covers longer rests.)
   2. **Pen arrival cancels fingers.** If a finger stream is active when
      the pen enters range or touches down, the finger stream is cancelled
      exactly as in 4.3 (the palm usually lands before the pen enters
      hover range).
   3. **`ACTION_CANCEL`** (any tool type) → if the stream holds a button
      (a stroke/drag is in progress): emit `KeyUp` with the internal
      scancode `kKeyPointerCancel` (a `kKeyUnknown` slot, see 6), then
      `MouseUp`. `DrawingState::onKeyUp` treats `kKeyPointerCancel` like
      `kKeyEsc` (one-line change at `drawing_state.cpp:323`), so the tool
      loop is cancelled and no undo entry is created; for other states
      (scroll, marquee) the `MouseUp` simply ends the drag. If no button is
      held, emit nothing. **Never synthesize an Esc KeyDown** (it runs
      `Cancel type=all` → `DeselectMask`, see Context). Fallback if the app
      change is rejected: `KeyUp(kKeyEsc)` alone (no KeyDown), then
      `MouseUp`. The rest of the stream (until `ACTION_DOWN`) is ignored.
   4. **Not in v1:** `touchMajor`/pressure palm heuristics. The probe
      shows they would work (~48 vs ≤ 23) but the system CANCEL plus 4.1
      already cover the observed cases. Revisit only if the M3.1 tablet
      test shows stray marks.
   5. Pointer-count guard: any stream that reaches **more than 3**
      simultaneous finger pointers is treated as a palm: cancel it (4.3)
      and ignore it to the end.
5. **One finger** (stream not rejected) → `PointerType::Touch` mouse
   events. Preference `touch.draw_with_finger` (default **off**): when
   off, a one-finger drag that starts on the editor canvas scrolls (hand
   behaviour, app side, M3.1); taps on widgets/menus always work as clicks.
6. **Virtual keys.** Discrete gestures become `KeyDown`+`KeyUp` with
   scancodes taken from `kKeyUnknown1..8` (numeric values unchanged, new
   names in laf `keys.h` and `src/ui/shortcut.cpp`):

   | Slot | Name | Source | Default binding (`android=` in gui.xml) |
   |------|------|--------|------------------------------------------|
   | `kKeyUnknown1` | `PenDoubleTap` | key 718 (debounced) | `ToggleEraser` |
   | `kKeyUnknown2` | `TwoFingerTap` | 2-finger tap | `Undo` |
   | `kKeyUnknown3` | `ThreeFingerTap` | 3-finger tap | `Redo` |
   | `kKeyUnknown4` | `PointerCancel` | `ACTION_CANCEL` (KeyUp only) | none, internal, not offered in the shortcut UI |

   `keyboard_shortcuts.cpp:47-66` gets an `#elif LAF_ANDROID` branch that
   reads `android=` before `shortcut=` (and not `linux=`).
   - **Pen double-tap debounce:** the first key-718 `DOWN` emits the
     gesture; further 718 `DOWN`s within **100 ms** of the last one, all
     718 `UP`s and any `repeatCount > 0` are dropped. Measured pairs are
     6-16 ms apart, so 100 ms is a 6× margin while still allowing a
     deliberate second double-tap (people cannot double-tap twice in
     < 200 ms).
   - **Finger taps:** a tap is a stream where the maximum simultaneous
     pointer count is exactly 2 (or 3), all pointers went down within
     150 ms of the first, the stream ended with `ACTION_UP` (no
     `ACTION_CANCEL`) within 300 ms of the first `DOWN`, no pointer moved
     more than ~24 device px (≈ 2 mm at 400 dpi), and the stream was not
     rejected by 4.1/4.5. Anything with 4+ pointers is a palm, never a tap.
7. **Two fingers** → precise `MouseWheel` (`PointerType::Touchpad`) for
   pan and `TouchMagnify` for pinch, as macOS does. The gesture **starts
   only** from a clean two-finger stream: exactly 2 pointers, not
   rejected, no CANCEL, and after the movement exceeds the tap slop
   (so a two-finger tap never pans). A third pointer ends the gesture
   (3-finger tap or palm). If one finger of the stream had already sent a
   `MouseDown` (finger drawing on), it is cancelled per 4.3 before the
   gesture starts, so no dot is left.
8. **`ToggleEraser` command** (new, `src/app/commands/cmd_toggle_eraser.cpp`,
   registered in `commands_list.h`, string in `data/strings/en.ini`
   `[commands]`, owner mobile-touch-ux): if the selected tool is not the
   eraser, remember it and `ToolBar::instance()->selectTool(eraser)`;
   if it is the eraser, select the remembered tool (or the pencil if none).
   Same pattern as `WindowWithHand::enableHandTool()`
   (`src/app/ui/window_with_hand.cpp:31-59`). It goes through the selected
   tool, not `eraserTipProximity()`, because the editor resets proximity on
   every pen message (Context). The toolbar shows the switch; the eraser
   keeps its own brush size. Not Android-only (harmless on desktop, can be
   bound to a key there). Mid-stroke it is swallowed by `DrawingState`
   (Context), so the toggle applies between strokes only.
9. **Fallback** without the pen gesture (other pen, or a future firmware
   change): the on-screen bar (M3.5) has an eraser button bound to the
   same `ToggleEraser` command.

10. **Pointer limits.** GameActivity copies at most 8 pointers per motion
    event (`GAMEACTIVITY_MAX_NUM_POINTERS_IN_MOTION_EVENT`,
    `GameActivityEvents.h:159`); the prefab glue is prebuilt, so the
    `_OVERRIDE` macro must **never** be set. The palm produced up to 8
    pointers, i.e. exactly the cap, so extra contacts are silently lost:
    the backend must tolerate `actionIndex >= pointerCount` on
    `POINTER_DOWN/UP` (skip the event, keep the stream state consistent)
    and treat "8 pointers" as "palm" (§4.5 already does for > 3).
11. **Key filter and system keys** (M0.7 review: the probe accepts and
    consumes every key, `android/app/src/main/cpp/main.cpp:780`). The real
    backend's key filter returns `false` (system handles it) for
    `KEYCODE_VOLUME_UP/DOWN/MUTE`, `CAMERA`, `ZOOM_IN/OUT`, `FOCUS`,
    `MEDIA_*`, `POWER`, `SLEEP/WAKEUP`, `BRIGHTNESS_*`; everything else
    (including 718 and Back) reaches native code. **Back**
    (`KEYCODE_BACK`, also the HarmonyOS back gesture) → `KeyDown/KeyUp`
    `kKeyEsc`, never finishes the activity: Esc closes the open
    menu/popup/dialog as on desktop. When nothing is open, Esc in the
    editor runs `Cancel type=all` (deselect) — acceptable and the same as
    a keyboard Esc; the app never exits through Back (File > Exit only).
    If the user finds deselect-on-Back annoying, M3.6 may route Back to a
    no-op when no window is open (touch-ux). Owners: laf-backend (filter,
    mapping, M2.3), touch-ux (Back policy review, M3.6).

## Alternatives

- **New `os::Event` types for gestures / an explicit cancel event.**
  Cleaner, but every consumer (`ui::Manager`, shortcuts UI) needs new code
  and gestures could not be rebound by users. The `kKeyPointerCancel`
  KeyUp gives the same effect with one app line. Revisit if more states
  need a real cancel.
- **Esc KeyDown+KeyUp emulation for ACTION_CANCEL** (this ADR's first
  draft). Rejected: KeyDown triggers `Cancel type=all` → `DeselectMask`
  and `DiscardBrush`.
- **`ToggleEraser` via `eraserTipProximity()`.** Rejected: reset on the
  next pen message by `Editor::updateToolByTipProximity()`.
- **Reporting `PointerType::Eraser` from laf while an "eraser mode" flag
  is set.** Works with zero app code but puts user state in laf, makes the
  gesture non-rebindable and invisible in the toolbar. Rejected.
- **Huawei Pen Kit / HMS SDK.** Not needed: key 718 reaches the app; Pen
  Kit is ArkTS/HarmonyOS NEXT only.
- **Rely on system palm rejection only.** The system's CANCEL arrives
  0.1-2 s late; without stylus priority the palm would paint (or pan) for
  that long.
- **`touchMajor` heuristics in v1.** Not needed (see 4.4).

## Consequences

- laf fork: names for `kKeyUnknown1..4` in `keys.h`, numeric values
  unchanged (rebases stay trivial).
- App: names in `src/ui/shortcut.cpp`, `src/app/app_menus.cpp:230`,
  `src/app/script/keys.cpp:125`; one line in `drawing_state.cpp:323`;
  `android=` attribute; `ToggleEraser` command; preferences
  `touch.draw_with_finger`, `touch.pen_rejection_timeout` in
  `data/pref.xml` with an Options section; the timeout reaches laf via
  `os::TabletOptions` (no new laf API).
- The backend keeps a small state machine: pen-in-range flag with
  deferred exit, last-pen-activity timestamp, current finger stream
  (rejected / tap candidate / gesture / mouse), double-tap timestamp.
  It should be unit-testable on the host (pure C++ fed with synthetic
  events), which the reviewer can run without the tablet.
- `AsepriteActivity` keeps the `OnHoverListener`; the Java key logging of
  the probe moves behind the probe variant (ADR 0008).
