#!/usr/bin/env bash
# F-UI-3.1d: wxString cleanup slice (libcodelite only) + F-UI-2 xrc + SetMenuBar core trace.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
HAP="$ROOT/host/fw2-hap/entry/build/default/outputs/default/entry-default-unsigned.hap"
LIBS="$ROOT/host/fw2-hap/entry/libs/arm64-v8a"
TARGET="${HDC_TARGET:-127.0.0.1:5555}"
BUNDLE=com.codelite.fw2.host
LOG="$ROOT/docs/logs/f-ui-3-1d-verify-$(date +%Y%m%d-%H%M%S).txt"

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

pack_hap() {
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
  echo "[f-ui-3.1d] Step 0 — F-UI-2 golden gate"
  step0_ok=0
  for attempt in 1 2 3; do
    bash "$ROOT/host/fw2-hap/scripts/restore-f-ui-2-golden-snapshot.sh"
    pack_hap
    hdc -t "$TARGET" uninstall "$BUNDLE" 2>/dev/null || true
    hdc -t "$TARGET" install "$HAP"
    hdc -t "$TARGET" shell hilog -r
    hdc -t "$TARGET" shell aa start -a EntryAbility -b "$BUNDLE"
    prelog="$ROOT/docs/logs/f-ui-2-golden-precheck-$(date +%Y%m%d-%H%M%S)-3d-a${attempt}.txt"
    if run_golden_gate_poll "$prelog"; then
      step0_ok=1
      break
    fi
    sleep 2
  done
  [[ "$step0_ok" -eq 1 ]] || { echo "[f-ui-3.1d] ABORT F-UI-2 gate" >&2; exit 1; }
fi

echo "[f-ui-3.1d] Step 1 — golden + xrc + SetMenuBar + wxString slice (libcodelite only)"
bash "$ROOT/host/fw2-hap/scripts/restore-f-ui-2-golden-snapshot.sh"
bash "$ROOT/host/fw2-hap/scripts/merge-wx-xrc-fui2.sh"
python3 "$ROOT/host/fw2-hap/scripts/patch-wx-fui3-1-setmenubar-trace.py"
python3 "$ROOT/host/fw2-hap/scripts/patch-f-ui-3-1d-wxstring-slice-trace.py"

for base in libwx_ohosu_core libwx_ohosu_xrc; do
  cp -f "$LIBS/${base}-3.3-OHOS.so.4.0.0" "$LIBS/${base}-3.3-OHOS.so.4" 2>/dev/null || true
  cp -f "$LIBS/${base}-3.3-OHOS.so.4.0.0" "$LIBS/${base}-3.3-OHOS.so" 2>/dev/null || true
done

echo "[f-ui-3.1d] Step 2 — cold install"
pack_hap
hdc -t "$TARGET" uninstall "$BUNDLE" 2>/dev/null || true
hdc -t "$TARGET" install "$HAP"
hdc -t "$TARGET" shell hilog -r
hdc -t "$TARGET" shell aa start -a EntryAbility -b "$BUNDLE"

echo "[f-ui-3.1d] Step 3 — poll wxString slice (up to ${F_UI3_POLL_TIMEOUT:-45}s, ${F_UI3_SLICE_ATTEMPTS:-3} attempts)"
slice_ok=0
for slice_attempt in $(seq 1 ${F_UI3_SLICE_ATTEMPTS:-3}); do
  LOG="$ROOT/docs/logs/f-ui-3-1d-verify-$(date +%Y%m%d-%H%M%S)-a${slice_attempt}.txt"
  if [[ "$slice_attempt" -gt 1 ]]; then
    hdc -t "$TARGET" shell hilog -r
    hdc -t "$TARGET" shell aa start -a EntryAbility -b "$BUNDLE"
    sleep 1
  fi
  : > "$LOG"
  deadline=$(($(date +%s) + ${F_UI3_POLL_TIMEOUT:-45}))
  while [[ $(date +%s) -lt "$deadline" ]]; do
    hdc -t "$TARGET" shell hilog -x 2>/dev/null >> "$LOG" || true
    if grep -qF '[FUI_FRAME_FLOW] cleanup finished' "$LOG"; then
      slice_ok=1
      break
    fi
    if grep -qE 'THREAD_BLOCK|cppcrash-com\.codelite|SIGSEGV' "$LOG" \
       && grep -qF '[FUI_XRC] LoadMenuBar return' "$LOG"; then
      slice_ok=1
      break
    fi
    sleep 2
  done
  hdc -t "$TARGET" shell hilog -x 2>/dev/null >> "$LOG" || true
  if grep -qF '[FUI_XRC] LoadMenuBar return' "$LOG"; then
    slice_ok=1
    break
  fi
  echo "[f-ui-3.1d] attempt $slice_attempt: no LoadMenuBar return yet, retry"
done

grep -E 'FUI_FRAME_FLOW|FUI_FRAME|FUI_XRC.*LoadMenuBar return|THREAD_BLOCK|SIGSEGV' "$LOG" | tail -50 || true

echo "[f-ui-3.1d] log saved: $LOG"
echo "[f-ui-3.1d] slice gates:"
set +e
bash "$ROOT/host/fw2-hap/scripts/check-f-ui-3-1d-gates.sh" "$LOG"
exit $?
