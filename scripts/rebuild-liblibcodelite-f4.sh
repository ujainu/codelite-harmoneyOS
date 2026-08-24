#!/usr/bin/env bash
# F-4: Rebuild liblibcodelite.so with OHOS GetPluginsDirectory → GetBinFolder().
# Prereq: full CodeLite/CMakeFiles/libcodelite.dir/*.o set (504 objects).
# cl_standard_paths.cpp.o alone is not enough for partial relink.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD="${ROOT}/build-codelite-ohos"
cd "$BUILD"

echo "[F-4] OHOS wxUSE_MDI=0 required in build-wx setup.h for compile unblock"
echo "[F-4] Building cl_standard_paths.cpp.o ..."
ninja CodeLite/CMakeFiles/libcodelite.dir/cl_standard_paths.cpp.o

echo "[F-4] Full relink lib/liblibcodelite.so ..."
ninja lib/liblibcodelite.so

if command -v llvm-nm >/dev/null 2>&1; then
  llvm-nm -C lib/liblibcodelite.so | rg "GetPluginsDirectory|GetBinFolder" || true
fi

echo "[F-4] Stage into HAP: host/fw2-hap/scripts/stage-wx-libs.sh"
