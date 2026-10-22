# Tablet test checklist (MatePad 11.5S PaperMatte + M-Pencil 3)

Manual tests the user runs on the tablet. Agents never install apps or
clear logs on the device (CONTEXT D5); the user runs `adb install` and
`adb logcat -c`. For every failed step paste back the output of the
logcat command given for that step (or the "common capture" below).

Common capture (start it before the step, stop with Ctrl+C after):

    adb logcat -c
    adb logcat -v time -s Aseprite:V AsepriteProbe:V laf:V AndroidRuntime:E DEBUG:V libc:F

`adb` lives in `~/Android/Sdk/platform-tools/adb` (not in `PATH`).

Build and install (laptop, repo root):

    source android/scripts/env.sh
    (cd android && ./gradlew :app:assembleDebug)
    adb install -r android/app/build/outputs/apk/debug/app-debug.apk

## M0 — stub + input probe APK (`0.0.x-m0-*`, branch mobile/m0-probe)

Status 2026-10-22: all steps passed (NOTES.md §9 "M0.5" and "M0.6").
Re-run as a regression whenever android/ or the native stub changes.

1. **Install and launch.** Install as above, open "Aseprite" from the
   launcher.
   Expected: full-screen solid colour, no status/navigation bar, no crash.
   Logcat has `startup: sdk=31 density=400`, `window: 2800x1840`,
   `GL_RENDERER: Maleoon 920A`, `GL_VERSION: OpenGL ES 3.2`, and
   `device id=...` lines.
   If it fails: common capture from launch, plus
   `adb logcat -v time -b crash -d`.

2. **Frame rate.** Leave the app idle 10 s.
   Expected: `... frames in 2.00 s = ~120 fps` every 2 s.
   If it fails: `adb logcat -v time -s Aseprite:V` (10 s).

3. **Rotation ×4.** Rotate portrait ↔ landscape four times.
   Expected: no restart (no new `android_main: start`), each rotation logs
   `APP_CMD_CONFIG_CHANGED`, `APP_CMD_WINDOW_RESIZED: window=1840x2800` /
   `2800x1840`, colour fills the whole screen.
   If it fails: common capture.

4. **Background / foreground ×5.** Home gesture, reopen from launcher;
   then Recents, reopen; then power button off/on.
   Expected per cycle: `APP_CMD_PAUSE` → `APP_CMD_TERM_WINDOW` →
   `EGL surface and context destroyed` → `APP_CMD_STOP`, then
   `APP_CMD_START` → `APP_CMD_RESUME` → `APP_CMD_INIT_WINDOW` → `window:`
   + `GL_RENDERER` lines; colour visible again, no crash, no black screen.
   (`APP_CMD_LOW_MEMORY` right after `SAVE_STATE` is normal on HarmonyOS.)
   If it fails: common capture.

5. **Kill from Recents.** Swipe the app away in Recents.
   Expected: process gone, no `APP_CMD_DESTROY` (known HarmonyOS
   behaviour), no crash dialog. Relaunch works.
   If it fails: `adb logcat -v time -d | grep -iE "aseprite|ActivityManager.*kill"`.

6. **Pen hover → draw → lift.** Bring the pen close without touching,
   move, touch and draw a line, lift, move away.
   Expected: light blue while hovering, dark blue while drawing; logcat
   `java hover ACTION_HOVER_ENTER`, native `HOVER_ENTER`, `HOVER_MOVE`...,
   `HOVER_EXIT`, `DOWN`, `MOVE` with `tool=STYLUS(2)`, `pressure` > 0,
   `tilt`/`orientation` non-zero, `UP`. Each hover event appears **once**.
   If it fails: `adb logcat -v time -s AsepriteProbe:V`.

7. **Palm while drawing.** Rest the palm on the screen, then draw with the
   pen; also land the palm in the middle of a pen stroke.
   Expected: palm shows as several `tool=FINGER` pointers (large
   `touchMajor`), then a magenta flash = `action=CANCEL`; afterwards no
   finger events while the pen draws; stylus events continue normally.
   If it fails: `adb logcat -v time -s AsepriteProbe:V`.

8. **Fingers.** One-finger tap, two-finger tap, three-finger tap, pinch.
   Expected: green; `DOWN` / `POINTER_DOWN` / `POINTER_UP` / `UP`,
   `pointerCount` matches the fingers (max 8 per event — GameActivity
   limit).
   If it fails: `adb logcat -v time -s AsepriteProbe:V`.

9. **Pen double-tap ×3 hovering, ×3 away from the screen.**
   Expected: yellow flash each time; per double-tap **two** DOWN/UP pairs
   6–16 ms apart, `java key ... keyCode=718 ... scanCode=190` and native
   `key # ... keyCode=718 scanCode=190` from device "HUAWEI M-Pencil 3
   Mouse". The system must not trigger its own action.
   If it fails: `adb logcat -v time | grep -iE "AsepriteProbe|keycode|KeyEvent"`
   and `adb shell settings list secure | grep -i double`.

10. **Back gesture and volume keys (probe-only behaviour).** Swipe the
    back gesture; press volume up/down.
    Expected in this probe: app stays open, volume does not change,
    `key # ... keyCode=4` (Back) / `24`/`25` (volume) logged — the probe
    consumes all keys on purpose. (The real backend in M1 must let volume
    through; re-check then.) Leave the app with the Home gesture.
    If it fails (app crashes or exits): common capture.

11. **Magnetic keyboard (if attached).** Press a few keys.
    Expected: `key #` lines from device "HUAWEI Magnetic Keyboard",
    yellow flash.
    If it fails: `adb logcat -v time -s AsepriteProbe:V`.
