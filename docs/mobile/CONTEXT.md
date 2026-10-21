# Aseprite Mobile — shared context for agents

Every agent in `.claude/agents/mobile-*.md` reads this file first. It holds
the decisions already made; do not re-open them without asking the user.

## Target

- Device: Huawei MatePad 11.5S PaperMatte, global firmware, HarmonyOS 4.x.
- HarmonyOS 4.x runs Android APKs, so the target is a regular **Android APK,
  ABI `arm64-v8a`**. HarmonyOS NEXT (.hap / ArkTS) is out of scope.
- Display: 11.5", ~2800×1840, 144 Hz, matte (PaperMatte) panel. Treat exact
  numbers (DPI, Android API level reported by HarmonyOS, GPU model) as
  unverified until the user reports `getprop` / `dumpsys` output from the
  device. Ask the user for that output when it matters.
- Stylus: Huawei M-Pencil. Needed in v1: **palm rejection** and **pen
  gestures** (side button / double-tap → e.g. eraser toggle). Pressure and
  tilt are not required in v1 but must not be blocked by the design.

## Decisions

| # | Decision |
|---|----------|
| D1 | Port + touch layer: keep the existing desktop UI (`src/ui`, `src/app/ui`), add a mobile platform backend plus touch/stylus adaptation. No UI rewrite. |
| D2 | Rendering: laf `skia` backend, Skia GPU over **OpenGL ES via EGL** on an `ANativeWindow`. Vulkan and CPU raster are not the main path (CPU raster is allowed as a temporary bring-up fallback). |
| D3 | laf changes live in **the user's own fork of `aseprite/laf`**: https://github.com/MaksimCharyev/laf (remotes in `laf/`: `origin` = fork, `upstream` = aseprite/laf). The `laf` submodule URL is switched to that fork. The Android backend goes in `laf/os/android/` next to `os/win`, `os/osx`, `os/x11`. |
| D4 | Android glue: native code driven by a small Gradle project in `android/` at the repo root (GameActivity from AGDK preferred over plain NativeActivity for IME/text input; the architect may revise with an ADR). |
| D5 | **No device access for agents.** Agents build the APK only. The user installs and tests by hand. Agents hand the user precise test checklists and the exact `adb logcat` filters/commands to run, and ask for the output back. |
| D6 | Prompts in English; agents report to the user **in Russian**. Code, comments, commit messages in English, following upstream style. |

## Legal constraint (EULA.txt)

Aseprite source is not open source. Compiling for personal use is allowed;
**redistributing binaries is not**. Builds are for the user's own tablet
only. Never add store publishing, public release CI, or upload APKs anywhere.

## Repo facts

- Main repo fork: https://github.com/MaksimCharyev/aseprite. Remotes:
  `origin` = fork (push here), `upstream` = aseprite/aseprite (pull only).
  Same layout in `laf/`. Use HTTPS remotes; SSH is not set up on this host.

- Submodules are not initialized in a fresh checkout
  (`git submodule update --init --recursive`). `laf/` is empty until then.
- Read `laf/README.md` and `INSTALL.md` for the exact Skia version laf
  expects before building Skia for Android.
- Platform abstraction: `laf/os` (`os::System`, `os::Window`, `os::Event`,
  `os::PointerType`). Aseprite UI layer: `src/ui` (`ui::Manager`,
  `src/ui/message_type.h` already has `kTouchMagnifyMessage`).
- Editor input: `src/app/ui/editor/*_state.cpp` (`standby_state`,
  `drawing_state`, `state_with_wheel_behavior`), tool loop in
  `src/app/tools/` (`pointer.h`, `tool_loop_manager.cpp`, `dynamics.h`).
- Resources: `data/` (gui.xml shortcuts, pref.xml, widgets/*.xml, skins).
  On Android they ship as APK assets and are extracted to internal storage.
- CMake options to turn off for Android unless a task needs them:
  `ENABLE_UPDATER`, `ENABLE_NEWS`, `ENABLE_DRM`, `ENABLE_STEAM`,
  `ENABLE_SENTRY`, `ENABLE_DESKTOP_INTEGRATION`, `ENABLE_WEBSOCKET`.

## Rules for all agents

1. Desktop builds must keep working. Android code is behind
   `#if LAF_ANDROID` / `if(ANDROID)` guards, or in Android-only files.
2. Match upstream style: `.clang-format`, file copyright headers, naming,
   comment density of the surrounding code.
3. Small, reviewable commits, one concern each. Commit only when the user
   asks. Never commit to `main`; work on a branch such as `mobile/<topic>`.
4. Write down anything you learned about the device or the toolchain that
   other agents need in `docs/mobile/NOTES.md`. Write architectural decisions
   as ADRs in `docs/mobile/adr/NNNN-title.md`.
5. If something cannot be verified without the device, say so plainly and
   produce a test step for the user instead of guessing.

## Workflow

Subagents cannot launch other subagents. The main Claude session
orchestrates:

1. `mobile-architect` → plan / ADR / task breakdown in `docs/mobile/ROADMAP.md`.
2. Implementation by `mobile-build-engineer`, `mobile-laf-backend`,
   `mobile-touch-ux`, as the roadmap assigns.
3. `mobile-reviewer` → review the diff, check desktop build + tests, write
   the manual device test checklist for the user.
4. User tests on the tablet, pastes logs back; loop.
