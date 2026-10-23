#!/usr/bin/env bash
# Fetch Aseprite's translations (https://github.com/aseprite/strings, CC BY
# 4.0) into build/strings at a pinned commit. Gradle packages the selected
# languages as assets/data/strings.git/<lang>.ini (see android/app/build.gradle),
# the same place the desktop ENABLE_I18N_STRINGS option uses.
#
# Usage: android/scripts/fetch-strings.sh
set -euo pipefail

STRINGS_URL="https://github.com/aseprite/strings.git"
STRINGS_COMMIT="0a3cef0"

repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
dir="${repo_dir}/build/strings"

if [[ ! -d "${dir}/.git" ]]; then
  git clone -q "${STRINGS_URL}" "${dir}"
fi
if ! git -C "${dir}" rev-parse -q --verify "${STRINGS_COMMIT}^{commit}" >/dev/null; then
  git -C "${dir}" fetch -q origin
fi
git -C "${dir}" checkout -q --detach "${STRINGS_COMMIT}"
echo "strings: ${dir} at $(git -C "${dir}" rev-parse --short HEAD)"
