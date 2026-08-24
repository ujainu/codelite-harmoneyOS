#!/usr/bin/env bash
# Capture a frozen OHOS IDE baseline for incremental patch builds (F-4+).
# Run after a device-verified boot (MV-1 / B-7 / Alive≥5s).
#
# Usage:
#   host/fw2-hap/scripts/freeze-ohos-baseline.sh [tag]
# Example:
#   host/fw2-hap/scripts/freeze-ohos-baseline.sh 20260809-pre-f4
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
TAG="${1:-$(date +%Y%m%d)-baseline}"
DEST="$ROOT/host/fw2-hap/baseline/$TAG"
BUILD="$ROOT/build-codelite-ohos"
WX="$ROOT/build-wx-ohos-gui/lib"
APP_O="$BUILD/LiteEditor/CMakeFiles/codelite_app.dir"

mkdir -p "$DEST/libs" "$DEST/objects/codelite_app.dir" "$DEST/objects/libcodelite.dir" "$DEST/meta"

copy_if() {
  local src="$1" dst="$2"
  if [[ -f "$src" ]]; then
    cp -p "$src" "$dst"
    echo "  + $(basename "$src")"
  else
    echo "  ! missing: $src" >&2
  fi
}

echo "[baseline] tag=$TAG"
echo "[baseline] dest=$DEST"

# --- Verified shared libraries (device boot anchors) ---
echo "[baseline] libs/"
copy_if "$BUILD/lib/libcodelite_app.so.pre-f4.bak" "$DEST/libs/libcodelite_app.so"
copy_if "$BUILD/lib/libcodelite_app.so.pre-f4.bak" "$DEST/libs/libcodelite_app.so.pre-f4.bak"
copy_if "$BUILD/lib/liblibcodelite.so.jul27.bak" "$DEST/libs/liblibcodelite.so"
copy_if "$BUILD/lib/libplugin.so" "$DEST/libs/libplugin.so"
copy_if "$BUILD/lib/libwxsqlite3.so" "$DEST/libs/libwxsqlite3.so"
copy_if "$WX/libwx_ohosu_core-3.3-OHOS.so.4.0.0" "$DEST/libs/libwx_ohosu_core-3.3-OHOS.so.4.0.0"
copy_if "$WX/libwx_ohosu_aui-3.3-OHOS.so.4.0.0" "$DEST/libs/libwx_ohosu_aui-3.3-OHOS.so.4.0.0"
copy_if "$WX/libwx_baseu-3.3-OHOS.so.4.0.0" "$DEST/libs/libwx_baseu-3.3-OHOS.so.4.0.0"

# --- Incremental patch targets (safe to rebuild) ---
echo "[baseline] patch objects/"
copy_if "$APP_O/pluginmanager.cpp.o" "$DEST/objects/codelite_app.dir/pluginmanager.cpp.o"
copy_if "$APP_O/pluginmanager.cpp.o.f4.bak" "$DEST/objects/codelite_app.dir/pluginmanager.cpp.o.f4.bak"
copy_if "$APP_O/f4_ohos_link_stubs.cpp.o" "$DEST/objects/codelite_app.dir/f4_ohos_link_stubs.cpp.o"
copy_if "$BUILD/CodeLite/CMakeFiles/libcodelite.dir/cl_standard_paths.cpp.o" \
  "$DEST/objects/libcodelite.dir/cl_standard_paths.cpp.o"

# --- Full codelite_app object set (reference / disaster recovery) ---
echo "[baseline] snapshot all codelite_app.dir/*.o ..."
if [[ -d "$APP_O" ]]; then
  tar -cf "$DEST/objects/codelite_app.dir-all.tar" -C "$BUILD/LiteEditor/CMakeFiles" codelite_app.dir
  echo "  + codelite_app.dir-all.tar ($(du -h "$DEST/objects/codelite_app.dir-all.tar" | awk '{print $1}'))"
fi

# --- frame.cpp.o: critical ABI anchor (may be MISSING after F-4 relink attempts) ---
FRAME_OK=0
if [[ -f "$DEST/objects/codelite_app.dir/frame.cpp.o" ]]; then
  :
elif [[ -f "$APP_O/frame.cpp.o" ]]; then
  cp -p "$APP_O/frame.cpp.o" "$DEST/objects/codelite_app.dir/frame.cpp.o"
fi
if [[ -f "$APP_O/frame.cpp.o" ]]; then
  # Heuristic: pre-F4 link was 2026-08-09 ~08:55; recompiles started ~09:47.
  frame_mtime=$(stat -f %m "$APP_O/frame.cpp.o" 2>/dev/null || stat -c %Y "$APP_O/frame.cpp.o")
  pre_f4_mtime=$(stat -f %m "$BUILD/lib/libcodelite_app.so.pre-f4.bak" 2>/dev/null || stat -c %Y "$BUILD/lib/libcodelite_app.so.pre-f4.bak")
  if [[ "$frame_mtime" -le "$pre_f4_mtime" ]]; then
    FRAME_OK=1
  fi
fi

# --- Manifest ---
{
  echo "tag=$TAG"
  echo "created=$(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "device_verified=manual  # set after MV-1/B-7/Alive gate"
  echo "pre_f4_lib=$BUILD/lib/libcodelite_app.so.pre-f4.bak"
  echo "frame_cpp_o_status=$([[ $FRAME_OK -eq 1 ]] && echo OK || echo STALE_OR_RECOMPILED)"
  echo ""
  echo "incremental_patch_allowlist:"
  echo "  - pluginmanager.cpp.o"
  echo "  - f4_ohos_link_stubs.cpp.o   # only if frame.cpp.o not in baseline"
  echo ""
  echo "never_recompile_without_baseline:"
  echo "  - frame.cpp.o"
  echo "  - any wxOHOS / render / input object"
  echo ""
  echo "f4_gate_strings:"
  echo "  - [F-4.path] plugin search path="
  echo "  - [F-4.scan] found"
} > "$DEST/meta/BASELINE.txt"

( cd "$DEST" && find . -type f ! -path './meta/SHA256SUMS' -exec shasum -a 256 {} + ) > "$DEST/meta/SHA256SUMS" 2>/dev/null \
  || ( cd "$DEST" && find . -type f ! -path './meta/SHA256SUMS' | while read -r f; do shasum -a 256 "$f"; done ) > "$DEST/meta/SHA256SUMS"

echo "[baseline] manifest: $DEST/meta/SHA256SUMS"
echo "[baseline] frame.cpp.o: $([[ $FRAME_OK -eq 1 ]] && echo OK || echo STALE — restore from Aug-9 08:55 backup if available)"
echo "[baseline] restore: host/fw2-hap/scripts/restore-ohos-baseline.sh $TAG"
echo "[baseline] patch:   host/fw2-hap/scripts/patch-codelite-app-incremental.sh"
