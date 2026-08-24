#!/usr/bin/env bash
# F-UI-3.1: SetMenuBar probe — ONLY after F-UI-2 golden gates PASS.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
HAP="$ROOT/host/fw2-hap/entry/build/default/outputs/default/entry-default-unsigned.hap"
LIBS="$ROOT/host/fw2-hap/entry/libs/arm64-v8a"
TARGET="${HDC_TARGET:-127.0.0.1:5555}"
BUNDLE=com.codelite.fw2.host
LOG="$ROOT/docs/logs/f-ui-3-1-verify-$(date +%Y%m%d-%H%M%S).txt"
GOLDEN_LOG="$ROOT/docs/logs/f-ui-2-golden-precheck-$(date +%Y%m%d-%H%M%S).txt"

run_golden_gate_poll() {
  local out_log="$1"
  : > "$out_log"
  local deadline=$(($(date +%s) + ${F_UI2_GATE_TIMEOUT:-45}))
  while [[ $(date +%s) -lt "$deadline" ]]; do
    hdc -t "$TARGET" shell hilog -x 2>/dev/null >> "$out_log" || true
    if bash "$ROOT/host/fw2-hap/scripts/check-f-ui-2-golden-gates.sh" "$out_log" 2>/dev/null; then
      return 0
    fi
    sleep 2
  done
  hdc -t "$TARGET" shell hilog -x 2>/dev/null >> "$out_log" || true
  bash "$ROOT/host/fw2-hap/scripts/check-f-ui-2-golden-gates.sh" "$out_log"
}

pack_golden_hap() {
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
}

if [[ "${F_UI3_SKIP_GOLDEN_GATE:-0}" != "1" ]]; then
  echo "[f-ui-3.1] Step 0 — F-UI-2 golden gate (required before SetMenuBar probe)"
  step0_ok=0
  for attempt in 1 2 3; do
    echo "[f-ui-3.1] Step 0 attempt $attempt/3"
    bash "$ROOT/host/fw2-hap/scripts/restore-f-ui-2-golden-snapshot.sh"
    pack_golden_hap
    hdc -t "$TARGET" uninstall "$BUNDLE" 2>/dev/null || true
    hdc -t "$TARGET" install "$HAP"
    hdc -t "$TARGET" shell hilog -r
    hdc -t "$TARGET" shell aa start -a EntryAbility -b "$BUNDLE"
    GOLDEN_LOG="$ROOT/docs/logs/f-ui-2-golden-precheck-$(date +%Y%m%d-%H%M%S)-a${attempt}.txt"
    if run_golden_gate_poll "$GOLDEN_LOG"; then
      step0_ok=1
      echo "[f-ui-3.1] Step 0 PASS — precheck log: $GOLDEN_LOG"
      break
    fi
    sleep 2
  done
  if [[ "$step0_ok" -ne 1 ]]; then
    echo "[f-ui-3.1] ABORT — F-UI-2 golden gate failed after 3 attempts" >&2
    exit 1
  fi
fi

echo "[f-ui-3.1] Step 1 — restore golden core (single patch base)"
bash "$ROOT/host/fw2-hap/scripts/restore-f-ui-2-golden-snapshot.sh"

echo "[f-ui-3.1] Step 2 — single SetMenuBar trace only"
python3 "$ROOT/host/fw2-hap/scripts/patch-wx-fui3-1-setmenubar-trace.py"

cp -f "$LIBS/libwx_ohosu_core-3.3-OHOS.so.4.0.0" "$LIBS/libwx_ohosu_core-3.3-OHOS.so.4" || true
cp -f "$LIBS/libwx_ohosu_core-3.3-OHOS.so.4.0.0" "$LIBS/libwx_ohosu_core-3.3-OHOS.so" || true

echo "[f-ui-3.1] pack HAP (patched core only; other libs unchanged golden)"
pack_golden_hap

echo "[f-ui-3.1] cold install on $TARGET"
hdc -t "$TARGET" uninstall "$BUNDLE" 2>/dev/null || true
hdc -t "$TARGET" install "$HAP"
hdc -t "$TARGET" shell hilog -r
hdc -t "$TARGET" shell aa start -a EntryAbility -b "$BUNDLE"

echo "[f-ui-3.1] poll hilog for SetMenuBar trace (up to ${F_UI3_POLL_TIMEOUT:-45}s)"
: > "$LOG"
deadline=$(($(date +%s) + ${F_UI3_POLL_TIMEOUT:-45}))
while [[ $(date +%s) -lt "$deadline" ]]; do
  hdc -t "$TARGET" shell hilog -x 2>/dev/null >> "$LOG" || true
  if grep -qE '\[FUI_FRAME\] SetMenuBar return' "$LOG"; then
    break
  fi
  if grep -qE 'THREAD_BLOCK|cppcrash-com\.codelite|SIGSEGV' "$LOG" \
     && grep -qE '\[FUI_MENU\] MenuBar handler created' "$LOG"; then
    break
  fi
  sleep 2
done
hdc -t "$TARGET" shell hilog -x 2>/dev/null >> "$LOG" || true

grep -E "FUI_FRAME|FUI_XRC|FUI_MENU|MenuBar handler|CreateResource return class=wxMenuBar|THREAD_BLOCK|cppcrash|SIGSEGV" "$LOG" \
  | tail -50 || true

echo "[f-ui-3.1] log saved: $LOG"
echo "[f-ui-3.1] classification:"
if grep -qE '\[FUI_FRAME\] SetMenuBar enter' "$LOG" \
   && grep -qE '\[FUI_FRAME\] SetMenuBar return' "$LOG"; then
  echo "  → Case A: SetMenuBar enter + return (hang is after attach → F-UI-3.2 AUI/layout)"
elif grep -qE '\[FUI_FRAME\] SetMenuBar enter' "$LOG"; then
  echo "  → Case B: SetMenuBar enter only (hang inside wxFrame::SetMenuBar)"
else
  echo "  → Case C: no SetMenuBar enter (CreateGUIControls not yet at frame attach)"
fi
