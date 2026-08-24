#!/usr/bin/env bash
# Restore wx/codelite for development — prefer golden snapshot when present.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
SNAP="$ROOT/host/fw2-hap/snapshots/f-ui-2-pass-20260811-155014"

if [[ -d "$SNAP" && "${F_UI2_FORCE_LEGACY_RESTORE:-0}" != "1" ]]; then
  echo "[baseline] using golden snapshot restore"
  exec bash "$ROOT/host/fw2-hap/scripts/restore-f-ui-2-golden-snapshot.sh"
fi

# Legacy: replay from tooltip (only when golden snapshot absent)
LIBS="$ROOT/host/fw2-hap/entry/libs/arm64-v8a"

CORE_TOOLTIP="$LIBS/libwx_ohosu_core-3.3-OHOS.so.4.0.0.bak-tooltip-patch"
AUI_CLEAN="$ROOT/build-wx-ohos-gui/lib/libwx_ohosu_aui-3.3-OHOS.so.4.0.0"
AUI_BAK_CLEAN="$LIBS/libwx_ohosu_aui-3.3-OHOS.so.4.0.0.bak-clean"
CL_PRE="$LIBS/libcodelite_app.so.bak-before-fui12-trace"

echo "[baseline] restore libwx_ohosu_core from bak-tooltip-patch"
if [[ ! -f "$CORE_TOOLTIP" ]]; then
  echo "[baseline] missing $CORE_TOOLTIP" >&2
  exit 1
fi
cp -f "$CORE_TOOLTIP" "$LIBS/libwx_ohosu_core-3.3-OHOS.so.4.0.0"
cp -f "$CORE_TOOLTIP" "$ROOT/build-wx-ohos-gui/lib/libwx_ohosu_core-3.3-OHOS.so.4.0.0" 2>/dev/null || true

echo "[baseline] restore libwx_ohosu_aui from build-wx clean copy"
if [[ -f "$AUI_CLEAN" ]]; then
  cp -f "$AUI_CLEAN" "$LIBS/libwx_ohosu_aui-3.3-OHOS.so.4.0.0"
  cp -f "$AUI_CLEAN" "$LIBS/libwx_ohosu_aui-3.3-OHOS.so.4" 2>/dev/null || true
  cp -f "$AUI_CLEAN" "$LIBS/libwx_ohosu_aui-3.3-OHOS.so" 2>/dev/null || true
elif [[ -f "$AUI_BAK_CLEAN" ]]; then
  cp -f "$AUI_BAK_CLEAN" "$LIBS/libwx_ohosu_aui-3.3-OHOS.so.4.0.0"
else
  echo "[baseline] warn: no clean aui found (skip aui restore)"
fi

echo "[baseline] restore libcodelite_app (no F-UI-1.2 binary probes)"
if [[ -f "$CL_PRE" ]]; then
  cp -f "$CL_PRE" "$LIBS/libcodelite_app.so"
else
  echo "[baseline] warn: missing $CL_PRE (skip libcodelite restore)"
fi

echo "[baseline] F-UI-2 baseline files restored — run patch-wx-stattext-inplace.py + merge-wx-xrc-fui2.sh next"
