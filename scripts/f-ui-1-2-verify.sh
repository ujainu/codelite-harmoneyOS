#!/usr/bin/env bash
# F-UI-1.2: strict close-out — GetBestXButtonSize return + CreateGUIControls progression (trace only)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
HAP="$ROOT/host/fw2-hap/entry/build/default/outputs/default/entry-default-unsigned.hap"
LIBS="$ROOT/host/fw2-hap/entry/libs/arm64-v8a"
TARGET="${HDC_TARGET:-127.0.0.1:5555}"
BUNDLE=com.codelite.fw2.host
LOG="$ROOT/docs/logs/f-ui-1-2-verify-$(date +%Y%m%d-%H%M%S).txt"

echo "[f-ui-1.2] apply wx platform patches (renderer + pen)"
python3 "$ROOT/host/fw2-hap/scripts/patch-wx-stattext-inplace.py"

echo "[f-ui-1.2] apply FUI_TRACE probes to libcodelite_app (no CodeLite source edits)"
# Re-apply trace from clean post-ui-boot copy when re-running
CL="$LIBS/libcodelite_app.so"
CL_PRE="$LIBS/libcodelite_app.so.bak-before-fui12-trace"
if [[ -f "$CL_PRE" ]]; then
  cp -f "$CL_PRE" "$CL"
  python3 "$ROOT/host/fw2-hap/scripts/patch-wx-stattext-inplace.py" >/dev/null
fi
python3 "$ROOT/host/fw2-hap/scripts/patch-f-ui-1-2-trace.py"

cp -f "$LIBS/libwx_ohosu_core-3.3-OHOS.so.4.0.0" "$LIBS/libwx_ohosu_core-3.3-OHOS.so.4" || true
cp -f "$LIBS/libwx_ohosu_core-3.3-OHOS.so.4.0.0" "$LIBS/libwx_ohosu_core-3.3-OHOS.so" || true
cp -f "$LIBS/libwx_ohos_graphics.so" "$LIBS/libwx_ohosu_tooltip_stub.so" 2>/dev/null || true

echo "[f-ui-1.2] pack HAP"
cd "$ROOT/host/fw2-hap/entry"
zip -u "$HAP" \
  libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so.4.0.0 \
  libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so.4 \
  libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so \
  libs/arm64-v8a/libwx_ohos_graphics.so \
  libs/arm64-v8a/libwx_ohosu_tooltip_stub.so \
  libs/arm64-v8a/libcodelite_app.so

echo "[f-ui-1.2] cold install on $TARGET"
hdc -t "$TARGET" uninstall "$BUNDLE" 2>/dev/null || true
hdc -t "$TARGET" install "$HAP"
hdc -t "$TARGET" shell hilog -r
hdc -t "$TARGET" shell aa start -a EntryAbility -b "$BUNDLE"

echo "[f-ui-1.2] collect hilog (20s)"
sleep 20
hdc -t "$TARGET" shell hilog -x 2>/dev/null | tee "$LOG" | grep -E \
  "FUI_TRACE|GetBestXButtonSize|LoadMenuBar|AUI Update|MainBook|GetTextExtent|B-[5-7]|cppcrash|SIGSEGV|THREAD_BLOCK" \
  || true

echo "[f-ui-1.2] log saved: $LOG"
echo "[f-ui-1.2] pass criteria:"
echo "  F-UI-1 PASS:  GetBestXButtonSize return + LoadMenuBar enter"
echo "  F-UI-1 partial: enter + return, stop before LoadMenuBar"
echo "  F-UI-1 fail: enter without return"
