# ADR 0007: Android lifecycle — flush state when the app goes to background

- Status: Accepted
- Date: 2026-10-22
- Owner: mobile-architect; implementers: mobile-laf-backend (laf side,
  M1.3), mobile-touch-ux (app side, M1.8)

## Context

Verified on the tablet (NOTES.md §9 "M0.5 hello APK on the device"):

- Home / Recents / power button: `APP_CMD_PAUSE → TERM_WINDOW → STOP →
  SAVE_STATE`, back: `START → RESUME → INIT_WINDOW` (fresh EGL each time).
- **Swiping the app away from Recents SIGKILLs the process** ("remove
  task"): no `APP_CMD_DESTROY`, `android_main` never returns, so
  `App::~App()` never runs.
- HarmonyOS sends `APP_CMD_LOW_MEMORY` right after **every** `SAVE_STATE`,
  with plenty of free memory.

What Aseprite only does at a clean exit or on a timer:

- Preferences (tool settings, recent files, UI scale, window layout) are
  written in `App::~App()` (`src/app/app.cpp:670-671`) and
  `MainWindow` (`src/app/ui/main_window.cpp:84`); the config file itself is
  flushed by `flush_config_file()` (`src/app/ini_file.cpp:127-131`).
- Data recovery backs up modified documents from a background thread
  every `data_recovery_period` minutes (default 2.0, `data/pref.xml:163-164`;
  loop in `src/app/crash/backup_observer.cpp:128-145`, sleeping on
  `m_wakeup`). A session that was not closed cleanly is offered for
  recovery on the next start (`src/app/crash/session.h:53`
  `isCrashedSession()`), which is exactly what we want after a SIGKILL —
  as long as the latest backup is recent.
- `os::Event::AppEnter/AppLeave` exist (`laf/os/event.h:34-35`; produced
  by macOS, X11 and Windows backends) but `ui::Manager` ignores them
  (`src/ui/manager.cpp:439-617` has no case for them).

## Decision

1. **laf (Android backend):** on `APP_CMD_PAUSE` queue
   `os::Event::AppLeave`; on `APP_CMD_RESUME` queue `os::Event::AppEnter`.
   Same semantics as on desktop, no new event type. `APP_CMD_LOW_MEMORY` is
   only logged (`LOG(VERBOSE)`), never turned into an event.
   `TERM_WINDOW`/`INIT_WINDOW` stay internal to the backend (surface
   destroy/recreate, ADR 0001).
2. **ui:** `ui::Manager` gets a virtual hook `onAppLeave()` /
   `onAppEnter()` (empty by default, same pattern as
   `onNewDisplayConfiguration()`, `src/ui/manager.h:153`), called from
   `generateMessagesFromOSEvents()` for those two events. Desktop
   behaviour does not change because the default hooks are empty.
3. **app:** `CustomizedGuiManager` (`src/app/modules/gui.cpp:84-105`)
   overrides `onAppLeave()` under `#if LAF_ANDROID` (can be made
   unconditional later if wanted):
   - `Preferences::instance().save()` and `flush_config_file()`;
   - ask data recovery for an immediate backup: new
     `BackupObserver::backupNow()` that sets a flag and notifies
     `m_wakeup` (the background thread does the work; the UI thread does
     not block). `DataRecovery` exposes it.
   - log both steps with timings (tag `Aseprite`).
4. While paused, the backend's `getEvent()` may block in
   `ALooper_pollOnce(-1)` once the queue is empty (no redraws without a
   window); queued events (including `AppLeave`) are delivered first.

## Alternatives

- **New `os::Event` type (`AppPause`).** `AppLeave` already means "the app
  lost focus/foreground" on every desktop backend. Rejected.
- **Do the flush inside laf (callback from `getEvent()`).** laf would need
  to know about Aseprite preferences; and calling app code from inside
  `getEvent()` re-enters the app at an arbitrary point. Rejected.
- **Shorter `data_recovery_period` on Android only.** Cheap, but still
  loses up to N minutes and wastes battery/flash. Could be combined later;
  not the main mechanism.
- **Save on `APP_CMD_SAVE_STATE`/`STOP` instead of `PAUSE`.** PAUSE is the
  earliest reliable signal and also covers split-screen focus loss;
  backups are cheap when nothing changed (the observer skips unmodified
  documents).

## Consequences

- A SIGKILL after the backup finished loses nothing; the next launch
  offers recovery. A SIGKILL in the first moments after PAUSE may lose the
  last edits (backup still running) — acceptable, tell the user.
- One tiny ui change (two virtual hooks + two `case`s), one app override,
  one new `BackupObserver` method. All desktop-neutral.
- `LOW_MEMORY` noise does not trigger cache purges or autosave storms.
