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

## M1 — first `hello_laf` APK (laf os/android backend, from M1.4)

Run when M1.4 produces the `hello_laf` variant. A black or garbage screen
is fine until M1.5. If a step fails, paste
`adb logcat -d -s laf:V AndroidRuntime:E DEBUG:F` and `adb logcat -b crash -d`.

1. **Install and launch.** Expected: no crash; `adb logcat -d -s laf` has
   `android_main: start (sdk 31)`, `HOME=XDG_CONFIG_HOME=/data/user/0/…/files
   TMPDIR=/data/user/0/…/cache`, `startup: density=400 …`,
   `input filters installed`, `calling app_main()`, `Window created: …`,
   `APP_CMD_RESUME`, `APP_CMD_INIT_WINDOW: window 2800x1840`,
   `Window attached: 2800x1840`.
2. **No CheckJNI errors.** `adb logcat -d | grep -E "JNI DETECTED|JNI: exception|FATAL"`
   → empty.
3. **Rotate ×5.** No crash; `APP_CMD_CONFIG_CHANGED: … orientation=` flips
   between 1 and 2; `Window resized: 1840x2800` / `2800x1840`.
4. **Home → back ×5** and **power off/on ×3.** Leaving: `APP_CMD_PAUSE`,
   `APP_CMD_TERM_WINDOW`, `Window detached`, `APP_CMD_STOP`,
   `APP_CMD_SAVE_STATE`; returning: `APP_CMD_START`, `APP_CMD_RESUME`,
   `APP_CMD_INIT_WINDOW`, `Window attached: …`. No crash.
5. **Idle in background.** Home, wait 30 s,
   `adb shell top -b -n 1 | grep aseprite` → ~0% CPU.
6. **Back gesture.** App neither closes nor crashes (ignored until M2/M3).
7. **Volume up/down.** The system volume panel appears.
8. **Verbose log** (user runs `adb shell setprop debug.laf.verbose 1`,
   swipes the app away, relaunches): finger → `motion: … tool=1`; pen →
   `tool=2`, hover actions `0x9`/`0x7`/`0xa`; pen double-tap → two pairs
   `key: action=0/1 keyCode=718 scanCode=190`; volume → **no**
   `keyCode=24/25` lines; Back → `keyCode=4`. Afterwards
   `adb shell setprop debug.laf.verbose 0`.
9. **Swipe from Recents.** Process killed without `APP_CMD_DESTROY`;
   relaunch passes step 1.
10. **Optional DESTROY path** (user toggles Developer options → "Don't keep
    activities" ON, then Home): `APP_CMD_DESTROY`, `app_main() returned …`,
    process exits, clean relaunch. Turn the option off afterwards; paste
    `adb logcat -d -s laf ActivityManager:I`.
11. **Dirs.** `adb shell run-as com.github.maksimcharyev.aseprite ls -la` →
    `files` and `cache` exist.

## M1 — Aseprite first-run defaults (`0.0.4-m1-aseprite`, M1.10, branch mobile/m1-defaults)

APK: `build/apk/aseprite-aseprite.apk`. The defaults apply only when the
configuration file has no saved value, and the first run of `0.0.3`
already saved `screen_scale=2`, so **uninstall first** (this also removes
`aseprite.ini`):

    adb uninstall com.github.maksimcharyev.aseprite
    adb install build/apk/aseprite-aseprite.apk
    adb logcat -c
    adb logcat -v time -s Aseprite:V laf:V AndroidRuntime:E DEBUG:V libc:F

1. **Launcher icon.** The launcher shows the Aseprite face icon (white
   square, two eyes, blue-grey bottom), label "Aseprite"; not the generic
   Android robot.
2. **Scale.** Launch. UI is ~1.5× bigger than in `0.0.3` (menu text about
   as big as on a desktop monitor). Edit > Preferences > General: Screen
   Scaling 300%, UI Elements Scaling 100%. Log: `Window attached: 2800x1840`.
3. **Safe area.** Log has one line
   `startup: safe area insets=(9 9 9 9) px, corner radii=(28 28 28 28) px from ro.config.fillet_radius_size, cutout=(0 0 0 0) waterfall=(0 0 0 0)`
   (HarmonyOS reports radius 0 through `Display.getRoundedCorner()`, so the
   Huawei property `ro.config.fillet_radius_size=11` dp is used). On screen:
   a 3-UI-pixel (9 px) strip of window colour around the whole UI; the "F"
   of "File" and the bottom status line are fully visible in all four
   corners, nothing is clipped by the rounded corners.
4. **Radius not right?** If something is still clipped (or the margin
   is too big), try other radii in pixels, e.g.
   `adb shell setprop debug.laf.corner_radius 60`, swipe the app away from
   Recents, relaunch; the log then says `from debug.laf.corner_radius` and
   the margin is `ceil(60 × 0.293) = 18` px. Report the smallest value
   where nothing is clipped, then `adb shell setprop debug.laf.corner_radius -1`
   to go back to the automatic value.
5. **Rotate ×3** (portrait/landscape). The margin stays on all four
   sides, the layout fills the new size; no `JNI: exception`/`FATAL` in
   the log.
6. **Change screen scale.** Preferences > Screen Scaling 200% > OK. The
   margin becomes 5 UI px (still 9-10 physical px) and the UI shrinks. Set
   300% back. Swipe the app away and relaunch: the chosen value is kept
   (saved values win over the Android default).
7. **Scale comparison (feeds M3.7).** Try 300%/100%, 200%/100% and
   200%/200% (Screen / UI Elements scaling) and report which one is the
   most comfortable with finger and pen.
8. **No CheckJNI errors.** `adb logcat -d | grep -E "JNI DETECTED|JNI: exception|FATAL"`
   → empty.

## M1.6 — EGL + Skia GL (`aseprite-hello_laf.apk`, `aseprite-aseprite-gpu.apk`, laf `mobile/laf-gpu`)

Before each run the user clears the log (`adb logcat -c`). Collect with
`adb logcat -d -s laf:V Aseprite:V AndroidRuntime:E DEBUG:F libEGL:V`.
Count lines with
`adb logcat -d -s laf | grep -cE "Using OpenGL backend \(GrDirectContext"`.

1. **hello_laf, GPU default.** Install, launch. Expected once:
   `GL: GPU acceleration on`, `GL: EGL 1.4 (…), surfaceless_context=1`,
   `GL: EGLContext created (OpenGL ES 3)`, `GL: EGLSurface #1 created
   (2800x1840)`, `OS: Using OpenGL backend (GrDirectContext #1, Maleoon
   920A, OpenGL ES 3.2 …)`, `GL: first frame on EGLSurface #1 …`. The
   picture is the same as with M1.5 (red/green/blue lines, red is red).
   No `Raster:` lines, no `failed (EGL error …)`.
2. **Rotate ×5.** Picture follows the rotation; `Window resized: …`
   only; **no** new `EGLSurface #` and no second `Using OpenGL backend`.
3. **Home → back ×5, power off/on ×5.** Each cycle: `GL: EGLSurface #N
   destroyed` after `APP_CMD_TERM_WINDOW`, `GL: EGLSurface #N+1 created`
   after `Window attached`, then `GL: first frame on EGLSurface #N+1`.
   Still exactly **one** `Using OpenGL backend` and one `EGLContext
   created`; no black screen.
4. **Raster override.** `adb shell setprop debug.laf.gpu 0`, swipe the app
   away, relaunch: `GL: debug.laf.gpu=0`, `Raster: using the raster
   backend for this window (GPU acceleration off)`, picture OK, no
   `EGLSurface` lines. Then `adb shell setprop debug.laf.gpu ""`.
5. **Aseprite, app default.** Since `mobile/m3-smooth` the Android default
   of `general.gpu_acceleration` is `true`: `GL: GPU acceleration on` once,
   no `GPU acceleration off`, no `Raster:` lines, then steps 1–3
   expectations. (If an old `aseprite.ini` has `gpu_acceleration=false`,
   the saved value wins: `GPU acceleration off` + `Raster:`; uninstall or
   force with step 6.)
6. **Aseprite, GPU forced.** `adb shell setprop debug.laf.gpu 1`, swipe
   away, relaunch: steps 1–3 expectations (Home tab instead of lines);
   open a sprite and draw with a finger/pen; open menus. Paste the log.
7. **Frame stats (optional).** `adb shell setprop debug.laf.verbose 1`,
   relaunch, scroll/draw for 10 s: `GL: N frames in 2.0 s (F fps),
   flush+swap avg A ms max M ms` lines (frames = frames the app drew, not
   the display rate). Reset `debug.laf.verbose 0` and `debug.laf.gpu ""`
   afterwards.

## Smoothness — GPU default + 144 Hz (`aseprite-aseprite.apk`, branch `mobile/m3-smooth`, laf `mobile/laf-smooth`)

Install `build/apk/aseprite-aseprite.apk`. Before each run the user clears
the log (`adb logcat -c`). Collect with
`adb logcat -d -s laf:V Aseprite:V AndroidRuntime:E DEBUG:F`.

1. **Launch.** Expected: `Aseprite: Display modes: … current <id> @60` (or
   another rate), `Aseprite: Display mode: preferred mode <id> (2800x1840
   @144 Hz)` (or the highest rate the panel lists), `laf: Frame rate:
   ANativeWindow_setFrameRate(144) requested`, `GL: GPU acceleration on`,
   exactly one `OS: Using OpenGL backend (GrDirectContext #1, …)`, no
   `Raster:` lines.
2. **Refresh rate while drawing.** Draw continuously with the pen and run
   `adb shell dumpsys display | grep -iE "mActiveSfDisplayMode|mActiveMode|renderFrameRate"`
   and `adb shell dumpsys SurfaceFlinger | grep -iE "activeMode|refresh"`.
   Expected: 144 (or 120) Hz, not 60. Paste both outputs.
3. **Rotate ×5, Home → back ×5, power off/on ×5.** No black screen; after
   each `Window attached` a new `Frame rate: ANativeWindow_setFrameRate(144)
   requested` and `GL: EGLSurface #N created`; still one `Using OpenGL
   backend`. Repeat step 2 after coming back.
4. **Baseline without votes.** `adb shell setprop debug.laf.frame_rate 0`,
   swipe away, relaunch: `Display mode: not requested` and `Frame rate: not
   requested`; `dumpsys display` should show 60 Hz again (the old
   behaviour). Then `adb shell setprop debug.laf.frame_rate 120` to compare
   with a 120 Hz request; reset with `adb shell setprop debug.laf.frame_rate ""`.
5. **Raster fallback.** `adb shell setprop debug.laf.gpu 0`, swipe away,
   relaunch: `GL: debug.laf.gpu=0`, `Raster: using the raster backend …`,
   UI works, drawing works. Reset `adb shell setprop debug.laf.gpu ""`.
6. **Frame stats (optional).** `adb shell setprop debug.laf.verbose 1`,
   relaunch, draw for 10 s: `GL: N frames in 2.0 s (F fps), …` — F close
   to the refresh rate while the stroke moves. Reset `debug.laf.verbose 0`.
