# Environment for the Android build of Aseprite (source this file, do not run it).
#
# Everything lives under ~/Android and ~/deps (see docs/mobile/BUILDING.md,
# "Uninstall"). Nothing here touches shell rc files.
#
# An externally set JAVA_HOME / ANDROID_HOME (mise, Android Studio, ...) is
# deliberately NOT inherited: Gradle needs JDK 17. Override explicitly with
# ASEPRITE_ANDROID_JAVA_HOME / ASEPRITE_ANDROID_HOME. ANDROID_NDK_HOME,
# ANDROID_NDK_VERSION and SKIA_DIR can be overridden directly.

export ANDROID_HOME="${ASEPRITE_ANDROID_HOME:-$HOME/Android/Sdk}"
export ANDROID_SDK_ROOT="$ANDROID_HOME"
export ANDROID_NDK_VERSION="${ANDROID_NDK_VERSION:-26.1.10909125}"
export ANDROID_NDK_HOME="${ANDROID_NDK_HOME:-$ANDROID_HOME/ndk/$ANDROID_NDK_VERSION}"
export ANDROID_NDK="$ANDROID_NDK_HOME"
export JAVA_HOME="${ASEPRITE_ANDROID_JAVA_HOME:-$HOME/Android/jdk-17}"
export SKIA_DIR="${SKIA_DIR:-$HOME/deps/skia-m124-android}"

# SDK CMake (bundles ninja): pick the newest version, tolerate a missing dir.
_cmake_dir=""
for _d in "$ANDROID_HOME"/cmake/*/bin; do
  [ -d "$_d" ] && _cmake_dir="$_d"   # glob is sorted lexically; last wins
done
unset _d

# Prepend only the entries not already on PATH (no growth on re-source).
_new_path=""
for _p in "$JAVA_HOME/bin" "$ANDROID_HOME/cmdline-tools/latest/bin" \
          "$ANDROID_HOME/platform-tools" ${_cmake_dir:+"$_cmake_dir"}; do
  case ":$PATH:$_new_path:" in
    *":$_p:"*) ;;
    *) _new_path="${_new_path:+$_new_path:}$_p" ;;
  esac
done
[ -z "$_new_path" ] || export PATH="$_new_path:$PATH"
unset _cmake_dir _new_path _p

# Gradle/AGP in this project needs JDK 17.
if [ -x "$JAVA_HOME/bin/java" ]; then
  _jv=$("$JAVA_HOME/bin/java" -version 2>&1 | sed -n '1s/.*"\([0-9]*\)\..*/\1/p')
  [ "$_jv" = "17" ] || echo "env.sh: warning: JAVA_HOME=$JAVA_HOME is Java '${_jv:-?}', expected 17" >&2
  unset _jv
else
  echo "env.sh: warning: no JDK at JAVA_HOME=$JAVA_HOME (see docs/mobile/BUILDING.md)" >&2
fi
