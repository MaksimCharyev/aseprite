# Building Aseprite for Android (MatePad)

Personal-use builds only (see `EULA.txt`, CONTEXT.md). The host toolchain is
temporary (D7): everything lives under `~/Android` and `~/deps`, nothing is
installed system-wide, no shell rc file is modified, no sudo.

Host used for these instructions: Arch-based Omarchy Linux, x86_64, 12 cores.

## Environment

`android/scripts/env.sh` sets everything; build scripts source it, and you
can source it in a shell (`source android/scripts/env.sh`):

| Variable | Value |
|---|---|
| `ANDROID_HOME`, `ANDROID_SDK_ROOT` | `~/Android/Sdk` |
| `ANDROID_NDK_HOME`, `ANDROID_NDK` | `$ANDROID_HOME/ndk/26.1.10909125` (r26b) |
| `JAVA_HOME` | `~/Android/jdk-17` |
| `SKIA_DIR` | `~/deps/skia-m124-android` |
| `PATH` | gets `$JAVA_HOME/bin`, `cmdline-tools/latest/bin`, `platform-tools`, SDK `cmake/<ver>/bin` (with its `ninja`) prepended |

An externally set `JAVA_HOME` / `ANDROID_HOME` (e.g. from mise) is ignored on
purpose, Gradle needs JDK 17. Override with `ASEPRITE_ANDROID_JAVA_HOME` /
`ASEPRITE_ANDROID_HOME`; `ANDROID_NDK_HOME`, `ANDROID_NDK_VERSION` and
`SKIA_DIR` can be exported directly. env.sh warns if the JDK is not 17 and
does not grow `PATH` when sourced repeatedly.

## M0.1 Install the toolchain (no sudo)

Versions actually installed on 2026-10-21: Temurin 17.0.20.1+1, cmdline-tools
23.0.0 (build 16111833), platform-tools 37.0.1, platforms;android-35,
build-tools;35.0.0, ndk;26.1.10909125, cmake;3.31.6 (bundles Ninja 1.12.1).

```
mkdir -p ~/Android/Sdk ~/Android/dl && cd ~/Android/dl

# JDK 17 (Temurin tarball; verify against the .sha256.txt next to it)
curl -fLO https://github.com/adoptium/temurin17-binaries/releases/download/jdk-17.0.20.1%2B1/OpenJDK17U-jdk_x64_linux_hotspot_17.0.20.1_1.tar.gz
curl -fLO https://github.com/adoptium/temurin17-binaries/releases/download/jdk-17.0.20.1%2B1/OpenJDK17U-jdk_x64_linux_hotspot_17.0.20.1_1.tar.gz.sha256.txt
sha256sum -c OpenJDK17U-jdk_x64_linux_hotspot_17.0.20.1_1.tar.gz.sha256.txt
mkdir -p ~/Android/jdk-17 && tar xzf OpenJDK17U-jdk_x64_linux_hotspot_17.0.20.1_1.tar.gz -C ~/Android/jdk-17 --strip-components=1

# cmdline-tools (sha1 e025545c62a8e64c7559119566a569fb1dec5f60)
curl -fLO https://dl.google.com/android/repository/commandlinetools-linux-16111833_latest.zip
unzip -q commandlinetools-linux-16111833_latest.zip -d x
mkdir -p ~/Android/Sdk/cmdline-tools && mv x/cmdline-tools ~/Android/Sdk/cmdline-tools/latest

# platform-tools (adb)
curl -fLO https://dl.google.com/android/repository/platform-tools-latest-linux.zip
unzip -q platform-tools-latest-linux.zip -d ~/Android/Sdk      # creates ~/Android/Sdk/platform-tools

cd ~ && rm -rf ~/Android/dl
source /path/to/aseprite/android/scripts/env.sh
yes | sdkmanager --licenses            # prints a deprecation note, harmless
sdkmanager "platforms;android-35" "build-tools;35.0.0" \
           "ndk;26.1.10909125" "cmake;3.31.6"
```

Verify:

```
source android/scripts/env.sh
sdkmanager --list_installed
$ANDROID_NDK_HOME/toolchains/llvm/prebuilt/linux-x86_64/bin/clang --version   # clang 17.0.2 (NDK r26b)
adb version
cmake --version && ninja --version
```

Gotcha: the current `sdkmanager` is a shim that downloads the "Android CLI"
into `~/.android/bin` on first use, and `sdkmanager --list` shows package ids
with `/` (`cmake/3.31.6`) while `install` still takes `;` ids.

## M0.2 Build Skia for Android

```
android/scripts/build-skia.sh
```

Clones `aseprite/skia` at `08a5439a6be7` (tag `m124-08a5439a6b`) into
`$SKIA_DIR`, runs `tools/git-sync-deps` with `GIT_SYNC_DEPS_SKIP_EMSDK=1` (emsdk is not
needed and is skipped; a stamp file `third_party/externals/.synced-<commit>`
marks a successful sync), fetches `gn`, configures
`out/Release-arm64` and builds `skia modules`. Result:
`$SKIA_DIR/out/Release-arm64/libskia.a`, `libskshaper.a`, `libskunicode.a`.
Final `args.gn`: NOTES.md section 6. Re-running is incremental. If you synced
before the skip existed, reclaim 1.1 GB with
`rm -rf ~/deps/skia-m124-android/third_party/externals/emsdk`.

Measured: build step 3m20s wall clock on 12 cores (1875 ninja steps);
dependency sync took several minutes more, network bound.

## Disk usage

| Path | Size |
|---|---|
| `~/Android/Sdk` (ndk 2.1 GB, platform-tools, cmdline-tools, cmake, build-tools, platforms) | ~2.7 GB |
| `~/Android/jdk-17` | 0.3 GB |
| `~/deps/skia-m124-android` (.git 0.7 GB, externals ~7 GB incl. emsdk 1.1 GB, out 0.17 GB) | ~8.7 GB (~7.6 GB without emsdk) |
| `~/.android` (adb keys, Android CLI binary) | ~0.1 GB |
| `~/.gradle` (appears with the first Gradle build, M0.5) | ~1-2 GB expected |

## Uninstall

Nothing was installed system-wide and no shell rc file was edited, so
removal is deleting directories. Run:

```
adb kill-server                                    # before deleting the SDK
rm -rf ~/Android                                   # SDK, NDK, CMake, Ninja, adb, JDK
rm -rf ~/deps/skia-m124-android ~/deps/skia-build.log && rmdir ~/deps   # rmdir fails harmlessly if ~/deps holds other things
rm -rf ~/.gradle                                   # after the first Gradle build (M0.5)
rm -rf ~/.android/bin ~/.android/cli               # Android CLI downloaded by sdkmanager
rm -rf ~/.java/.userPrefs/google                   # created by sdkmanager; leave sibling dirs (jetbrains, ...) alone
rm -rf <repo>/build/android                        # planned Android build tree (git-ignored via `build`)
```

`~/.android/adbkey*` and `analytics.settings` are created by adb itself
(RSA key the tablet trusts); delete `~/.android` entirely only if you no
longer want adb authorised on this host, and the tablet will ask to
re-authorise.

Environment variables introduced (all set only inside shells that source
`android/scripts/env.sh`; none are persisted): `ANDROID_HOME`,
`ANDROID_SDK_ROOT`, `ANDROID_NDK_HOME`, `ANDROID_NDK`,
`ANDROID_NDK_VERSION`, `JAVA_HOME`, `SKIA_DIR`, plus PATH entries. Also
`SKIA_JOBS` (optional, read by `build-skia.sh`). Repo files to remove at the
end: `android/`, `docs/mobile/` if you do not want them.
