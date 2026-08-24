#!/usr/bin/env bash
# Restore a frozen OHOS baseline into build + HAP staging paths.
#
# Usage:
#   host/fw2-hap/scripts/restore-ohos-baseline.sh [tag]
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
TAG="${1:-20260809-pre-f4}"
SRC="$ROOT/host/fw2-hap/baseline/$TAG"
BUILD="$ROOT/build-codelite-ohos"
HAP_LIBS="$ROOT/host/fw2-hap/entry/libs/arm64-v8a"
APP_O="$BUILD/LiteEditor/CMakeFiles/codelite_app.dir"

if [[ ! -d "$SRC" ]]; then
  echo "[baseline] missing: $SRC"
  echo "[baseline] run: host/fw2-hap/scripts/freeze-ohos-baseline.sh $TAG"
  exit 1
fi

echo "[baseline] restore tag=$TAG"

mkdir -p "$BUILD/lib" "$APP_O" "$HAP_LIBS"

restore() {
  local from="$1" to="$2"
  if [[ -f "$from" ]]; then
    cp -p "$from" "$to"
    echo "  -> $to"
  fi
}

echo "[baseline] build/lib/"
restore "$SRC/libs/libcodelite_app.so" "$BUILD/lib/libcodelite_app.so"
restore "$SRC/libs/libcodelite_app.so.pre-f4.bak" "$BUILD/lib/libcodelite_app.so.pre-f4.bak"
restore "$SRC/libs/liblibcodelite.so" "$BUILD/lib/liblibcodelite.so"
restore "$SRC/libs/libplugin.so" "$BUILD/lib/libplugin.so"
restore "$SRC/libs/libwxsqlite3.so" "$BUILD/lib/libwxsqlite3.so"

echo "[baseline] HAP staging libs/"
for f in "$SRC/libs"/*.so*; do
  [[ -f "$f" ]] || continue
  cp -p "$f" "$HAP_LIBS/$(basename "$f")"
done

if [[ -f "$SRC/objects/codelite_app.dir/frame.cpp.o" ]]; then
  echo "[baseline] frame.cpp.o from baseline"
  cp -p "$SRC/objects/codelite_app.dir/frame.cpp.o" "$APP_O/frame.cpp.o"
fi

for o in pluginmanager.cpp.o f4_ohos_link_stubs.cpp.o; do
  if [[ -f "$SRC/objects/codelite_app.dir/$o" ]]; then
    cp -p "$SRC/objects/codelite_app.dir/$o" "$APP_O/$o"
  fi
done

if [[ -f "$SRC/objects/codelite_app.dir-all.tar" && "${RESTORE_ALL_OBJECTS:-0}" == "1" ]]; then
  echo "[baseline] extracting full codelite_app.dir (RESTORE_ALL_OBJECTS=1)"
  tar -xf "$SRC/objects/codelite_app.dir-all.tar" -C "$BUILD/LiteEditor/CMakeFiles"
fi

echo "[baseline] done. Verify boot before incremental patch."
