# ADR 0002: Ship `data/` as APK assets, extract to internal storage, reuse existing lookup paths

- Status: Accepted
- Date: 2026-10-21
- Owner: mobile-architect

## Context

Aseprite reads its resources (`gui.xml`, `pref.xml`, `widgets/*.xml`,
skins/extensions, palettes, fonts, strings) as plain files found by
`app::ResourceFinder`. On Linux-like builds `ResourceFinder::includeDataDir()`
(`src/app/resource_finder.cpp:84-118`) searches, in order:

1. `$XDG_CONFIG_HOME/aseprite/data/<file>` (or `$HOME/.config/...`),
   via `includeHomeConfigDir()` (`resource_finder.cpp:154-165`);
2. `$BINDIR/data/<file>`, where `$BINDIR` comes from `base::get_app_path()`,
   which reads `/proc/self/exe` (`laf/base/fs_unix.h:170-188`).

On Android `/proc/self/exe` is `/system/bin/app_process64`, so option 2 can
never work. APK assets are not files on disk; they are only readable through
`AAssetManager`, and `AAssetDir` cannot list subdirectories.

User configuration (`aseprite.ini`, sessions, data recovery) is stored under
`includeUserDir()` (`resource_finder.cpp:169-209`), which on Linux also uses
`$XDG_CONFIG_HOME/aseprite`.

## Decision

1. The build copies the generated `bin/data` tree (output of the `copy_data`
   target, `src/CMakeLists.txt:179`) into the APK as `assets/data/` and
   generates `assets/data.manifest`: one relative path per line plus a
   header line with a content hash of the whole tree.
2. At startup, in `android_main` **before** `app_main`, native code:
   - sets `HOME=<filesDir>`, `XDG_CONFIG_HOME=<filesDir>`,
     `TMPDIR=<cacheDir>` via `setenv()`;
   - compares the manifest hash with `<filesDir>/aseprite/data/.stamp`; if
     different (first run or new APK), deletes `<filesDir>/aseprite/data/`
     and extracts every manifest entry via `AAssetManager`, then writes the
     stamp.
3. No changes to `ResourceFinder` are needed: path 1 above resolves to
   `<filesDir>/aseprite/data/...`, and user config lands in
   `<filesDir>/aseprite/`.
4. `base::get_user_docs_folder()` (`laf/base/fs_unix.h:199`) gets an Android
   branch that returns the app-specific external files dir
   (`/sdcard/Android/data/<appId>/files`) so the user can `adb push/pull`
   sprites; the path is passed from Java or computed from
   `ANativeActivity::externalDataPath` (`GameActivity::externalDataPath`).

## Alternatives

1. **Read assets directly through a virtual filesystem.** Would touch every
   `std::ifstream`/`FILE*` use in `src/app` and third-party loaders
   (tinyxml2, FreeType, Lua). Rejected: violates D1/“prefer hooks in laf”.
2. **Extract in Java (`AssetManager.list()` recursion) on the UI thread.**
   Works without a manifest but runs on the UI thread (ANR risk) and adds
   Java code. Rejected in favour of a build-time manifest.
3. **Ship data in a separate zip and unzip with libarchive.** Extra step
   and dependency on libarchive init order; no real gain.
4. **Add an Android branch to `ResourceFinder::includeDataDir()`.** Possible
   later if we want data in a read-only location separate from user
   overrides; not needed now.

## Consequences

- First launch after install/update costs one extraction (`data/` is a few
  MB; measure in M1) — log the duration.
- The user override location and the shipped data are the same directory,
  so user-installed extensions must not be placed under `aseprite/data/`
  (they go to `aseprite/extensions/`, which Aseprite already uses).
- `HOME`/`XDG_*` env vars also affect Lua `os.getenv` and any code reading
  `HOME`; this is intended.
- App uninstall deletes everything in `filesDir`; tell the user to keep
  sprites in the external files dir or (M4) save through SAF.
