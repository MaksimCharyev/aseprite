# ADR 0004: minSdk 29, targetSdk 34, arm64-v8a only, NDK r26b

- Status: Accepted (device verified 2026-10-21: API 31, HarmonyOS 4.3.0, NOTES.md §9)
- Date: 2026-10-21
- Owner: mobile-architect

## Context

- The device runs HarmonyOS 4.x. The only concrete public data point is a
  MatePad 12X with **HarmonyOS 4.2.0 reporting Android 12 (API 31)** (Krita
  Artists thread, see NOTES.md). HarmonyOS 3.0 was also reported as API 31.
  **Verified** on the user's MatePad 11.5S (SLG-W09): HarmonyOS 4.3.0,
  `ro.build.version.release=12`, API **31** (NOTES.md §9).
- APIs we want: `AMotionEvent` tool types / stylus buttons (API 14-23),
  `ANativeWindow_setBuffersDataSpace` (28), Jetpack low-latency
  front-buffer graphics (29, optional later), `ANativeWindow_setFrameRate`
  (30, optional, call via `__builtin_available`), `FLAG_CANCELED` (33, will
  not exist on an API 31 device).
- Skia `aseprite-m124` pins NDK **r26b** for its own Android bots
  (`infra/bots/assets/android_ndk_linux/create.py`), and its GN default
  `ndk_api` is 21.

## Decision

- `minSdk 29`, `compileSdk 35` (only `platforms;android-35` is installed),
  `targetSdk 34`
  (avoids Android 15 forced edge-to-edge behaviour changes; irrelevant to an
  API 31 device but keeps emulator testing predictable).
- ABI: `arm64-v8a` only (`abiFilters`).
- NDK: **r26b** for Skia and Aseprite alike (`ndkVersion "26.1.10909125"`
  in Gradle). Skia GN arg `ndk_api=29`.
- Debug-signed APK only. No release signing config, no store metadata,
  no CI publishing (EULA constraint in CONTEXT.md).
- No `INTERNET` permission in the manifest (news/updater are disabled; this
  also makes it obvious the build is personal-use only).

## Alternatives

- `minSdk 31` (match the device exactly): gains nothing we need and blocks
  testing on older emulators.
- `minSdk 21` (GameActivity minimum): needs runtime checks for many NDK
  calls for no benefit.
- Newer NDK (r27/r28): untested with Skia m124's GN files; may be tried
  later if r26b blocks something.

## Consequences

- The device reports API 31 (≥ 29), so `minSdk 29` stands; API 30
  calls such as `ANativeWindow_setFrameRate` are available on the device
  but still go through `__builtin_available`.
- Palm rejection cannot rely on `FLAG_CANCELED`; see NOTES.md and ADR 0006.
