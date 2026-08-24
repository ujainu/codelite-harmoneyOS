#!/usr/bin/env bash
# F-UI-3: MainFrame layout trace — locate first hang after LoadMenuBar
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
HAP="$ROOT/host/fw2-hap/entry/build/default/outputs/default/entry-default-unsigned.hap"
LIBS="$ROOT/host/fw2-hap/entry/libs/arm64-v8a"
TARGET="${HDC_TARGET:-127.0.0.1:5555}"
BUNDLE=com.codelite.fw2.host
LOG="$ROOT/docs/logs/f-ui-3-verify-$(date +%Y%m%d-%H%M%S).txt"

echo "[f-ui-3] restore clean libcodelite (no F-UI-1.2 binary probes)"
CL_PRE="$LIBS/libcodelite_app.so.bak-before-fui12-trace"
if [[ -f "$CL_PRE" ]]; then
  cp -f "$CL_PRE" "$LIBS/libcodelite_app.so"
fi

echo "[f-ui-3] apply wx platform patches (renderer + pen + stattext)"
python3 "$ROOT/host/fw2-hap/scripts/patch-wx-stattext-inplace.py"

echo "[f-ui-3] rebuild xrc with F-UI-2 traces (MenuBar baseline)"
bash "$ROOT/host/fw2-hap/scripts/merge-wx-xrc-fui2.sh"

# F-UI-3.0 multi-hook experiments removed — use f-ui-3-1-verify.sh instead.

cp -f "$LIBS/libwx_ohosu_core-3.3-OHOS.so.4.0.0" "$LIBS/libwx_ohosu_core-3.3-OHOS.so.4" || true
cp -f "$LIBS/libwx_ohosu_core-3.3-OHOS.so.4.0.0" "$LIBS/libwx_ohosu_core-3.3-OHOS.so" || true
cp -f "$LIBS/libwx_ohos_graphics.so" "$LIBS/libwx_ohosu_tooltip_stub.so" 2>/dev/null || true

echo "[f-ui-3] pack HAP"
cd "$ROOT/host/fw2-hap/entry"
zip -u "$HAP" \
  libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so.4.0.0 \
  libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so.4 \
  libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so \
  libs/arm64-v8a/libwx_ohosu_aui-3.3-OHOS.so.4.0.0 \
  libs/arm64-v8a/libwx_ohosu_aui-3.3-OHOS.so.4 \
  libs/arm64-v8a/libwx_ohosu_aui-3.3-OHOS.so \
  libs/arm64-v8a/libwx_ohosu_stc-3.3-OHOS.so.4.0.0 \
  libs/arm64-v8a/libwx_ohosu_stc-3.3-OHOS.so.4 \
  libs/arm64-v8a/libwx_ohosu_stc-3.3-OHOS.so \
  libs/arm64-v8a/libwx_ohosu_xrc-3.3-OHOS.so.4.0.0 \
  libs/arm64-v8a/libwx_ohosu_xrc-3.3-OHOS.so.4 \
  libs/arm64-v8a/libwx_ohosu_xrc-3.3-OHOS.so \
  libs/arm64-v8a/libwx_ohos_graphics.so \
  libs/arm64-v8a/libwx_ohosu_tooltip_stub.so \
  libs/arm64-v8a/libcodelite_app.so

echo "[f-ui-3] cold install on $TARGET"
hdc -t "$TARGET" uninstall "$BUNDLE" 2>/dev/null || true
hdc -t "$TARGET" install "$HAP"
hdc -t "$TARGET" shell hilog -r
hdc -t "$TARGET" shell aa start -a EntryAbility -b "$BUNDLE"

echo "[f-ui-3] collect hilog (25s)"
sleep 25
hdc -t "$TARGET" shell hilog -x 2>/dev/null | tee "$LOG" | grep -E \
  "FUI_FRAME|FUI_AUI|FUI_BOOK|FUI_EDITOR|FUI_XRC|FUI_MENU|LoadMenuBar|MenuBar handler created|CreateGUIControls|aui Update|SetMenuBar|THREAD_BLOCK|cppcrash|SIGSEGV|FULL_UI" \
  || true

echo "[f-ui-3] log saved: $LOG"
echo "[f-ui-3] hang categories:"
echo "  A: stops after MenuBar, before/at SetMenuBar return"
echo "  B: stops at AUI Update enter (no return)"
echo "  C: stops at SetStatusBar / wxStatusBar Create (no return)"
echo "  D: stops at Notebook Create enter (no return)"
echo "  E: stops at Scintilla create enter (no further progress)"
echo "  F: other wx node (see last FUI_* tag before THREAD_BLOCK)"
