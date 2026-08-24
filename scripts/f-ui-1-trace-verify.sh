#!/usr/bin/env bash
# F-UI-1 dispatch trace only — confirm GetBestXButtonSize → GetTextExtent call path (no fallback fix).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
HAP="$ROOT/host/fw2-hap/entry/build/default/outputs/default/entry-default-unsigned.hap"
LIBS="$ROOT/host/fw2-hap/entry/libs/arm64-v8a"
TARGET="${HDC_TARGET:-127.0.0.1:5555}"
BUNDLE=com.codelite.fw2.host
LOG="$ROOT/docs/logs/f-ui-1-trace-$(date +%Y%m%d-%H%M%S).txt"

export F_UI_1_DISPATCH_TRACE=1
echo "[f-ui-1-trace] apply wx stattext patches + FUI_TRACE dispatch probes (no GetTextExtent fallback)"
python3 "$ROOT/host/fw2-hap/scripts/patch-wx-stattext-inplace.py"

echo "[f-ui-1-trace] skip CodeLite hang traces (wx-only path confirmation)"
cp -f "$LIBS/libwx_ohosu_core-3.3-OHOS.so.4.0.0" "$LIBS/libwx_ohosu_core-3.3-OHOS.so.4" || true
cp -f "$LIBS/libwx_ohosu_core-3.3-OHOS.so.4.0.0" "$LIBS/libwx_ohosu_core-3.3-OHOS.so" || true

echo "[f-ui-1-trace] pack libwx_ohosu_core into HAP"
cd "$ROOT/host/fw2-hap/entry"
zip -u "$HAP" \
  libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so.4.0.0 \
  libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so.4 \
  libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so

echo "[f-ui-1-trace] cold install on $TARGET"
hdc -t "$TARGET" uninstall "$BUNDLE" 2>/dev/null || true
hdc -t "$TARGET" install "$HAP"
hdc -t "$TARGET" shell hilog -r
hdc -t "$TARGET" shell aa start -a EntryAbility -b "$BUNDLE"

echo "[f-ui-1-trace] collect hilog (15s)"
sleep 15
hdc -t "$TARGET" shell hilog -x 2>/dev/null | tee "$LOG" | grep -E \
  "\[FUI_TRACE\]|P-3\.2|B-[5-7]|THREAD_BLOCK|cppcrash|SIGSEGV" \
  || true

echo "[f-ui-1-trace] log saved: $LOG"
echo "[f-ui-1-trace] expect ordered probes around GetBestXButtonSize, e.g.:"
echo "  wxMemoryDC::SelectObject"
echo "  wxGraphicsContext::Create(MemoryDC)"
echo "  wxGCDCImpl::ctor(MemoryDC)"
echo "  wxGCDCImpl::SetFont"
echo "  wxGCDCImpl::DoGetTextExtent  (inline wxDC::GetTextExtent target)"
echo "  wxCairoContext::GetTextExtent / wxOhosDCImpl::DoGetTextExtent (if reached)"
