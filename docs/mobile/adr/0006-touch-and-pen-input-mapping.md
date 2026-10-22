# ADR 0006: How touch, stylus, palm and pen gestures map onto existing laf/Aseprite events

- Status: Proposed (finalize after the M0 input probe logs from the device)
- Date: 2026-10-21
- Owner: mobile-architect; implementers: mobile-laf-backend (M2), mobile-touch-ux (M3)

## Context

Aseprite already understands several input sources because of macOS
trackpads and Wacom tablets:

- `os::PointerType` has `Touch`, `Pen`, `Eraser`, `Touchpad`
  (`laf/os/pointer_type.h:13-21`), and `os::Event` carries pressure,
  precise wheel deltas and a magnification factor
  (`laf/os/event.h:115-143`).
- `ui::Manager` turns them into `ui::Message`s
  (`src/ui/manager.cpp:552-608`), including `kTouchMagnifyMessage`
  (`manager.cpp:738-745`).
- The editor switches to the eraser automatically when the pointer is
  `PointerType::Eraser` (`src/app/ui/editor/editor.cpp:1837-1846`, calling
  `ActiveToolManager::eraserTipProximity()` /
  `regularTipProximity()` in `src/app/tools/active_tool.cpp:151,159`).
- Pinch zoom is handled in
  `StateWithWheelBehavior::onTouchMagnify()`
  (`src/app/ui/editor/state_with_wheel_behavior.cpp:513`), two-finger
  scrolling via precise wheel in `onMouseWheel()` (same file, `:71-120`).
  macOS produces exactly these (`laf/os/osx/view.mm:549,576`).
- Esc during a stroke cancels the tool loop, but only on key **up**
  (`DrawingState::onKeyUp`, `src/app/ui/editor/drawing_state.cpp:320-330`);
  a `MouseUp` before that commits the stroke.
- Shortcuts are data-driven (`data/gui.xml`, per-platform attributes read in
  `src/app/ui/keyboard_shortcuts.cpp:47-66`); scancodes
  `kKeyUnknown1..8` exist in laf (`laf/os/keys.h:140-147`) but have no
  names in `src/ui/shortcut.cpp:35-139`, so they cannot be bound yet.
- The user's pen is an M-Pencil 3 with **no side button**. A double-tap
  arrives on the pen's Bluetooth HID device as evdev `KEY_F20` (scan 190,
  `Generic.kl` "F20"), two DOWN/UP pairs ~22 ms apart per double-tap
  (NOTES.md §9). The Android key code is **expected** to be Huawei's 718
  (reported for HarmonyOS 4.2, NOTES.md §3); confirm in the M0.6 probe.
  FLAG_CANCELED is not available (device is API 31, NOTES.md §9).

## Decision (proposed)

Policy lives in laf's Android backend only where it is pure signal
translation; anything user-configurable lives in Aseprite preferences.

1. **Stylus** (`TOOL_TYPE_STYLUS`) → `MouseDown/Move/Up`, `LeftButton`,
   `PointerType::Pen`, pressure from `AXIS_PRESSURE`; every historical
   sample becomes its own `MouseMove` so strokes keep full 240 Hz+ fidelity.
   `TOOL_TYPE_ERASER` → `PointerType::Eraser`. Hover (`ACTION_HOVER_*`) →
   `MouseMove`/`MouseEnter`/`MouseLeave` with `PointerType::Pen` (brush
   preview).
2. **Stylus barrel button** (if the pen has one, `BUTTON_STYLUS_PRIMARY`)
   → `RightButton` (desktop Wacom convention); secondary → `MiddleButton`.
   Not applicable to the user's M-Pencil 3 (no side button); kept for
   other pens.
3. **Single finger** → `PointerType::Touch` mouse events, **but** while a
   pen is "recently active" (hovering, touching, or within a configurable
   timeout, default 1 s after pen up) finger contacts are dropped
   (palm rejection by stylus priority). `TOOL_TYPE_PALM` is always dropped.
   A preference "Draw with finger" (off by default) controls whether a
   finger may paint at all; when off, a single finger pans.
4. **Two fingers** → precise `MouseWheel` (`PointerType::Touchpad`) for
   pan and `TouchMagnify` for pinch, as macOS does. No new app code is
   needed for pan/zoom.
5. **`ACTION_CANCEL`** → only if the cancelled pointer currently holds a
   button (a stroke is in progress), synthesize `KeyDown(kKeyEsc)` +
   `KeyUp(kKeyEsc)` and then `MouseUp`. The tool loop is cancelled in
   `DrawingState::onKeyUp` (`drawing_state.cpp:320-330`), so the Esc pair
   must come first; a `MouseUp` alone would commit the stroke. If no
   button is held, send nothing (a stray Esc would close menus or
   deselect). Preferred follow-up: an explicit cancel hook (e.g. a
   cancel flag on the `MouseUp`/a dedicated event) instead of the Esc
   emulation.
6. **Discrete gestures become virtual keys**: pen double-tap (expected
   Android key 718, evdev `KEY_F20`; confirm in M0.6), two-finger tap,
   three-finger tap are emitted as `KeyDown/KeyUp` with dedicated scancodes taken from the
   `kKeyUnknown1..8` slots and given names (e.g. `PenDoubleTap`,
   `TwoFingerTap`, `ThreeFingerTap`) in `src/ui/shortcut.cpp`. Default
   bindings go in `data/gui.xml` with a new `android="..."` attribute read
   by `keyboard_shortcuts.cpp` under `#if LAF_ANDROID`: two-finger tap →
   Undo, three-finger tap → Redo, pen double-tap → a new
   `ToggleEraser` command (switches eraser on/off via the same mechanism
   as `eraserTipProximity()`), so users can rebind them in
   Edit > Keyboard Shortcuts.
   Pen double-tap debounce: a burst of key-down events within ~100 ms is
   **one** gesture (one double-tap delivers two DOWN/UP pairs); key-ups
   are ignored. Without this `ToggleEraser` fires twice and cancels
   itself.
7. **Fallback if the pen gesture never reaches the app**: the on-screen
   modifier bar (M3) has an eraser toggle bound to the same
   `ToggleEraser` command.

## Alternatives

- **New `os::Event` types for gestures.** Cleaner semantically, but every
  consumer (`ui::Manager`, shortcuts UI) needs new code and they could not
  be rebound by users. Rejected for now.
- **Huawei Pen Kit / HMS SDK.** No public Android-side SDK for M-Pencil
  double-tap was found; Pen Kit documentation is for HarmonyOS NEXT
  (ArkTS). Rejected; revisit only if key 718 does not arrive.
- **Rely on system palm rejection only.** The system sends
  `ACTION_CANCEL` in some cases, but on API 31 there is no per-pointer
  `FLAG_CANCELED`; stylus priority is the robust approach used by most
  drawing apps.

## Consequences

- Touching laf `keys.h` names is a fork-local change; keep the numeric
  values so upstream rebases stay trivial.
- The finger-vs-pen policy and the timeout need preferences
  (`data/pref.xml`) and probably a small section in Options.
- Everything here must be validated with the M0 input-probe logs before
  implementation starts.
