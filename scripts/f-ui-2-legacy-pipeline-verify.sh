#!/usr/bin/env bash
# F-UI-2 via full patch pipeline (diagnostic / rebuild golden). Not for daily gate.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
HAP="$ROOT/host/fw2-hap/entry/build/default/outputs/default/entry-default-unsigned.hap"
LIBS="$ROOT/host/fw2-hap/entry/libs/arm64-v8a"
TARGET="${HDC_TARGET:-127.0.0.1:5555}"
BUNDLE=com.codelite.fw2.host
LOG="$ROOT/docs/logs/f-ui-2-legacy-$(date +%Y%m%d-%H%M%S).txt"

echo "[f-ui-2-legacy] restore tooltip baseline + clean aui"
F_UI2_FORCE_LEGACY_RESTORE=1 bash "$ROOT/host/fw2-hap/scripts/restore-f-ui2-baseline.sh"
python3 "$ROOT/host/fw2-hap/scripts/patch-wx-stattext-inplace.py"
bash "$ROOT/host/fw2-hap/scripts/merge-wx-xrc-fui2.sh"

for base in libwx_ohosu_core libwx_ohosu_aui libwx_ohosu_xrc libwx_baseu; do
  cp -f "$LIBS/${base}-3.3-OHOS.so.4.0.0" "$LIBS/${base}-3.3-OHOS.so.4" 2>/dev/null || true
  cp -f "$LIBS/${base}-3.3-OHOS.so.4.0.0" "$LIBS/${base}-3.3-OHOS.so" 2>/dev/null || true
done

echo "[f-ui-2-legacy] pack HAP"
cd "$ROOT/host/fw2-hap/entry"
zip -u "$HAP" \
  libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so.4.0.0 \
  libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so.4 \
  libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so \
  libs/arm64-v8a/libwx_ohosu_aui-3.3-OHOS.so.4.0.0 \
  libs/arm64-v8a/libwx_ohosu_aui-3.3-OHOS.so.4 \
  libs/arm64-v8a/libwx_ohosu_aui-3.3-OHOS.so \
  libs/arm64-v8a/libwx_ohosu_xrc-3.3-OHOS.so.4.0.0 \
  libs/arm64-v8a/libwx_ohosu_xrc-3.3-OHOS.so.4 \
  libs/arm64-v8a/libwx_ohosu_xrc-3.3-OHOS.so \
  libs/arm64-v8a/libwx_baseu-3.3-OHOS.so.4.0.0 \
  libs/arm64-v8a/libwx_baseu-3.3-OHOS.so.4 \
  libs/arm64-v8a/libwx_baseu-3.3-OHOS.so \
  libs/arm64-v8a/libwx_ohos_graphics.so \
  libs/arm64-v8a/libwx_ohosu_tooltip_stub.so \
  libs/arm64-v8a/libcodelite_app.so

echo "[f-ui-2-legacy] cold install"
hdc -t "$TARGET" uninstall "$BUNDLE" 2>/dev/null || true
hdc -t "$TARGET" install "$HAP"
hdc -t "$TARGET" shell hilog -r
hdc -t "$TARGET" shell aa start -a EntryAbility -b "$BUNDLE"
sleep 25
hdc -t "$TARGET" shell hilog -x 2>/dev/null | tee "$LOG" | grep -E \
  "FUI_XRC|FUI_MENU|LoadMenuBar|GetTextExtent|B-[5-7]|THREAD_BLOCK|cppcrash" || true

echo "[f-ui-2-legacy] log: $LOG"
bash "$ROOT/host/fw2-hap/scripts/check-f-ui-2-golden-gates.sh" "$LOG" || true
