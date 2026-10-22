# ADR 0008: Native build — host-built `gen`, Gradle drives the root CMake, native variants

- Status: Accepted
- Date: 2026-10-22
- Owner: mobile-architect; implementer: mobile-build-engineer (M1.1, M1.7)

## Context

- M0.4 (NOTES.md §6 "Root project cross-build") showed that `app-lib`
  cannot be built for Android because the code generator `gen` is built
  for the target (aarch64) and then executed on the host by every
  generation rule in `src/app/CMakeLists.txt:13-87` (`${GEN_EXE}`:
  widgets `*.xml.h`, `pref.xml.h/.cpp`, `theme.xml.h`, `strings.ini.h`,
  `command_ids.ini.h`, `check-translations`), collected in the
  `generate_files` target (`:92`).
- Upstream already has the hook: if `GEN_EXE` is set at configure time,
  `src/CMakeLists.txt:76-79` imports it as an executable and does not
  build `gen` (used for macOS cross builds). No CMake change is needed.
- `gen` itself only needs `laf-base`, `cfg-lib`, tinyxml2
  (`src/gen/CMakeLists.txt:18-21`).
- GameActivity is consumed as a **prefab** package
  (`find_package(game-activity REQUIRED CONFIG)`,
  `android/app/src/main/cpp/CMakeLists.txt`), which only exists when
  CMake is run by Gradle with `buildFeatures.prefab = true`.
- Today Gradle builds a stand-alone stub (`android/app/src/main/cpp/`,
  the M0.5 hello + M0.6 probe) as `libaseprite.so`, and packages
  `data/` straight from the source tree as assets
  (`android/app/build.gradle`, `assets.srcDirs = ['../../data']`, i.e. at
  the APK asset root, not under `assets/data/`).

## Decision

1. **Host `gen`.** New script `android/scripts/build-host-gen.sh`:
   configures `build/host-tools` (git-ignored) with the host compiler,
   the SDK CMake/Ninja, `LAF_BACKEND=none` and every optional feature OFF
   (`ENABLE_UPDATER/NEWS/DRM/STEAM/SENTRY/DESKTOP_INTEGRATION/WEBSOCKET/
   TESTS/CCACHE=OFF`, `LAF_WITH_EXAMPLES/TESTS=OFF`), and builds only the
   `gen` target. Idempotent and incremental (re-run picks up `src/gen`
   changes). `configure-android.sh` calls it first and passes
   `-DGEN_EXE=<repo>/build/host-tools/bin/gen`.
2. **Gradle drives the root CMake.** `android/app/src/main/cpp/CMakeLists.txt`
   becomes a thin wrapper: sets the ADR 0003 §5 options as cache
   variables, `GEN_EXE`, Skia paths, and `add_subdirectory(<repo root>)`;
   the root project (via `src/CMakeLists.txt` under `if(ANDROID)`) defines
   the `aseprite` SHARED target. `build.gradle` gets a `preBuild`
   dependency on an `Exec` task running `build-host-gen.sh`. The CMake
   build type for the native part is `RelWithDebInfo` in the debug APK
   (a `-O0` Aseprite is too slow to judge on the tablet); AGP strips the
   packaged `.so`, unstripped copies stay under `app/build/` for
   `ndk-stack`.
3. **Native variants**, selected with a Gradle property
   `-PnativeTarget=<name>` (default `aseprite`), all producing
   `libaseprite.so` so the manifest's `android.app.lib_name` never
   changes:
   - `aseprite` — the app;
   - `hello_laf` — `laf/examples/hello_laf.cpp` on the Android backend
     (bring-up, M1.4-M1.5);
   - `probe` — the M0.6 input probe stub (kept through M3 to re-check
     input facts after firmware updates; removed in M4.5). The Java-side
     key logging in `AsepriteActivity.dispatchKeyEvent` is enabled only
     in this variant (`BuildConfig` field from the same property). The
     `OnHoverListener` stays in all variants (ADR 0006 §3).
   `versionName` carries the variant suffix so the user can tell what is
   installed (`adb shell dumpsys package ... | grep versionName`).
4. **Assets** stay sourced from the tree but move under `assets/data/`
   (ADR 0002) via a generated assets directory plus `data.manifest`
   (M1.6). Files that the desktop `copy_data` target adds
   (`README.md`, `AUTHORS.md`, `EULA.txt`, `docs/LICENSES.md`,
   `src/CMakeLists.txt:156-179`) are copied too, so About/credits work.
   Translations (`clone_strings`, network) are not needed for v1.
5. The stand-alone `configure-android.sh` tree stays for fast
   compile checks of libraries by agents and the reviewer. For targets
   that need GameActivity headers (laf-os on Android, the final link),
   Gradle is the reference; the build engineer may point the stand-alone
   tree at Gradle's prefab output if it stays simple, otherwise those
   targets are checked through Gradle only.

## Alternatives

- **`ExternalProject_Add` of a host build inside the Android configure.**
  Automatic but nests a second CMake with a different toolchain inside the
  NDK one (env leakage, `CMAKE_FIND_ROOT_PATH` side effects seen in M0.3/
  M0.4), and Gradle re-runs it on every configure. Rejected; a script is
  simpler to debug and the `GEN_EXE` hook already exists.
- **Commit pre-generated headers.** Goes stale whenever `data/*.xml`
  changes; rejected.
- **Build the root project outside Gradle and import `libaseprite.so` as a
  prebuilt (`jniLibs`).** Loses prefab (GameActivity) integration and the
  one-command build. Rejected.
- **Drop the probe now.** It costs nothing as a variant and is the only
  tool to re-verify pen/palm facts. Kept until M4.

## Consequences

- One extra host build tree (`build/host-tools`, a few minutes the first
  time). It is inside the repo build dir, so nothing new for the
  "Uninstall" section of `BUILDING.md` beyond `build/` itself.
- `./gradlew :app:assembleDebug` becomes the single command for the app.
- Host compiler is needed (gcc already used for desktop checks in M0.3/
  M0.4); document it in `BUILDING.md`.
