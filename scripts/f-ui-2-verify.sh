#!/usr/bin/env bash
# F-UI-2: golden snapshot restore + strict MenuBar chain gate (no patch pipeline).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
HAP="$ROOT/host/fw2-hap/entry/build/default/outputs/default/entry-default-unsigned.hap"
LIBS="$ROOT/host/fw2-hap/entry/libs/arm64-v8a"
TARGET="${HDC_TARGET:-127.0.0.1:5555}"
BUNDLE=com.codelite.fw2.host
LOG="$ROOT/docs/logs/f-ui-2-verify-$(date +%Y%m%d-%H%M%S).txt"
SNAP="$ROOT/host/fw2-hap/snapshots/f-ui-2-pass-20260811-155014"

if [[ ! -d "$SNAP" ]]; then
  echo "[f-ui-2] golden snapshot missing — creating once"
  bash "$ROOT/host/fw2-hap/scripts/create-f-ui-2-golden-snapshot.sh"
fi

echo "[f-ui-2] restore golden snapshot (no stattext/xrc rebuild)"
bash "$ROOT/host/fw2-hap/scripts/restore-f-ui-2-golden-snapshot.sh"

echo "[f-ui-2] pack HAP (core + aui + xrc + cl + tooltip — all golden bytes)"
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

echo "[f-ui-2] cold install on $TARGET"
hdc -t "$TARGET" uninstall "$BUNDLE" 2>/dev/null || true
hdc -t "$TARGET" install "$HAP"
hdc -t "$TARGET" shell hilog -r
hdc -t "$TARGET" shell aa start -a EntryAbility -b "$BUNDLE"

echo "[f-ui-2] poll hilog for golden gates (up to ${F_UI2_GATE_TIMEOUT:-45}s)"
: > "$LOG"
deadline=$(($(date +%s) + ${F_UI2_GATE_TIMEOUT:-45}))
while [[ $(date +%s) -lt "$deadline" ]]; do
  hdc -t "$TARGET" shell hilog -x 2>/dev/null >> "$LOG" || true
  if bash "$ROOT/host/fw2-hap/scripts/check-f-ui-2-golden-gates.sh" "$LOG" 2>/dev/null; then
    grep -E "FUI_XRC|FUI_MENU|LoadMenuBar|MenuBar handler|CreateResource return class=wxMenuBar|B-[5-7]|THREAD_BLOCK|cppcrash|SIGSEGV" "$LOG" | tail -40 || true
    echo "[f-ui-2] log saved: $LOG"
    exit 0
  fi
  sleep 2
done

hdc -t "$TARGET" shell hilog -x 2>/dev/null >> "$LOG" || true
grep -E "FUI_XRC|FUI_MENU|LoadMenuBar|MenuBar handler|CreateResource return class=wxMenuBar|B-[5-7]|THREAD_BLOCK|cppcrash|SIGSEGV" "$LOG" | tail -40 || true
echo "[f-ui-2] log saved: $LOG"
echo "[f-ui-2] golden gates:"
bash "$ROOT/host/fw2-hap/scripts/check-f-ui-2-golden-gates.sh" "$LOG"
