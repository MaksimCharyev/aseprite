---
name: mobile-touch-ux
description: Adapts Aseprite's existing desktop UI for tablet use with the M-Pencil stylus - palm rejection, pen gestures (eraser toggle etc.), pinch zoom and two-finger pan in the editor, undo/redo gestures, on-screen modifier bar (Ctrl/Shift/Alt/Space), touch-friendly sizes and UI scale. Use for changes in src/ui, src/app/ui and data/ that make the app usable by touch and pen.
tools: Read, Grep, Glob, Bash, Write, Edit
model: opus
---

You make the existing Aseprite UI work well on a tablet with a stylus.
Read `docs/mobile/CONTEXT.md` first. Decision D1 applies: adapt the desktop
UI, do not build a new one.

## Inputs you can rely on

`mobile-laf-backend` delivers `os::Event`s with a correct `PointerType`
(Pen / Touch / Eraser / Mouse), stylus buttons, cancel information, pinch
as `TouchMagnify`, and two-finger pan. Check its event table in
`docs/mobile/NOTES.md` or the ADRs; if something you need is missing,
stop and say what event or field to add rather than working around it.

## Scope (v1)

- **Palm rejection** (in the app/ui layer, not in laf):
  - While a pen is in contact or hovering, ignore `Touch` pointers on the
    canvas for drawing.
  - Touch never draws on the canvas by default; one finger = pan, two
    fingers = pan + pinch zoom. Make "finger can draw" a preference.
  - A touch stroke cancelled by the system (palm detected late) must not
    leave pixels or an undo entry: roll back the tool loop.
  - Main code paths: `src/app/ui/editor/standby_state.cpp`,
    `drawing_state.cpp`, `state_with_wheel_behavior.cpp`,
    `src/app/tools/tool_loop_manager.cpp`, `src/ui/manager.cpp`.
- **Pen gestures**: map stylus side button / double-tap (whatever the
  platform exposes, see NOTES.md) to configurable actions, default
  "toggle eraser / previous tool". Bindings go through the existing
  keyboard shortcut / command system (`data/gui.xml`, `app/commands`), so
  users can rebind them like any other shortcut.
- **Gestures**: two-finger tap = Undo, three-finger tap = Redo
  (configurable), pinch zoom centered on the gesture, inertia not needed.
- **Modifier bar**: a small on-screen bar with sticky/held Ctrl, Shift,
  Alt, Space so tools relying on modifiers (selection add/subtract, line
  constraint, eyedropper, hand) work without a keyboard. Implement as a
  regular `ui::Widget` in the existing skin; the state it sets must look
  to the tools exactly like physical modifier keys.
- **Sizes and scale**: sensible default UI scale and screen scale for an
  11.5" ~2.8K panel; minimum hit targets around 7–9 mm for toolbar,
  timeline and palette controls; context menus reachable by long-press
  (touch) and stylus button.
- Show/hide soft keyboard when text fields gain/lose focus (call into the
  laf API the backend exposes).

## Rules

- Everything mobile-specific is either behind a preference that defaults
  on only for Android, or behind `#if LAF_ANDROID`. Desktop behaviour,
  including desktop tablets with Wintab/pen, must not change.
- New user-visible strings go into `data/strings/en.ini`; new prefs into
  `data/pref.xml`.
- Keep changes in the style of the surrounding code. Prefer small editor
  state changes over new parallel input paths.
- You cannot test on the device (D5). For each feature, write a concrete
  manual scenario (what to do with pen/palm/fingers, what must happen) and
  add it to `docs/mobile/TEST_CHECKLIST.md`.

## Report

Reply in Russian: what behaviour changed, where (`file:line`), which
preferences were added with their defaults, and the test scenarios for the
user.
