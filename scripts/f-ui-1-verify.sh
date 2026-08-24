#!/usr/bin/env bash
# F-UI-1: GetTextExtent fallback cold-boot gate (wxOHOS only; no CodeLite business edits)
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
HAP="$ROOT/host/fw2-hap/entry/build/default/outputs/default/entry-default-unsigned.hap"
LIBS="$ROOT/host/fw2-hap/entry/libs/arm64-v8a"
TARGET="${HDC_TARGET:-127.0.0.1:5555}"
BUNDLE=com.codelite.fw2.host
LOG="$ROOT/docs/logs/f-ui-1-verify-$(date +%Y%m%d-%H%M%S).txt"

echo "[f-ui-1] apply wx patches (stattext chain includes graphics renderer)"
python3 "$ROOT/host/fw2-hap/scripts/patch-wx-stattext-inplace.py"

if [[ "${F_UI_1_TRACE:-0}" == "1" ]] && [[ -f "$ROOT/host/fw2-hap/scripts/patch-codelite-hang-trace.py" ]]; then
  echo "[f-ui-1] apply CreateGUIControls hang traces (optional; set F_UI_1_TRACE=1)"
  python3 "$ROOT/host/fw2-hap/scripts/patch-codelite-hang-trace.py"
else
  echo "[f-ui-1] skip hang traces (F_UI_1_TRACE!=1; traces currently crash at 0x5da2b4)"
fi

# Keep .so / .so.4 in sync with patched .so.4.0.0 (ignore identical-file cp exit 1)
cp -f "$LIBS/libwx_ohosu_core-3.3-OHOS.so.4.0.0" "$LIBS/libwx_ohosu_core-3.3-OHOS.so.4" || true
cp -f "$LIBS/libwx_ohosu_core-3.3-OHOS.so.4.0.0" "$LIBS/libwx_ohosu_core-3.3-OHOS.so" || true
cp -f "$LIBS/libwx_ohos_graphics.so" "$LIBS/libwx_ohosu_tooltip_stub.so" 2>/dev/null || true

echo "[f-ui-1] pack libwx + graphics (as tooltip_stub) + libcodelite_app into HAP"
cd "$ROOT/host/fw2-hap/entry"
zip -u "$HAP" \
  libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so.4.0.0 \
  libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so.4 \
  libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so \
  libs/arm64-v8a/libwx_ohos_graphics.so \
  libs/arm64-v8a/libwx_ohosu_tooltip_stub.so \
  libs/arm64-v8a/libcodelite_app.so

echo "[f-ui-1] cold install on $TARGET"
hdc -t "$TARGET" uninstall "$BUNDLE" 2>/dev/null || true
hdc -t "$TARGET" install "$HAP"
hdc -t "$TARGET" shell hilog -r
hdc -t "$TARGET" shell aa start -a EntryAbility -b "$BUNDLE"

echo "[f-ui-1] collect hilog (12s)"
sleep 12
hdc -t "$TARGET" shell hilog -x 2>/dev/null | tee "$LOG" | grep -E \
  "FUI_TRACE|GraphicsRenderer|OHOS GetTextExtent|after GetBestXButtonSize|before GetBestXButtonSize|CreateGUIControls|LoadMenuBar|AuiManager|MainBook|Notebook|B-[5-7]|P-3\.2|THREAD_BLOCK|cppcrash|SIGSEGV" \
  || true

echo "[f-ui-1] log saved: $LOG"
echo "[f-ui-1] pass signals:"
echo "  1) [FUI_TRACE] wxOHOS GraphicsRenderer created / slot filled"
echo "  2) [FULL_UI_TRACE] after GetBestXButtonSize (or LoadMenuBar)"
echo "  3) B-6 / LoadMenuBar / AUI / MainBook (next stage)"
