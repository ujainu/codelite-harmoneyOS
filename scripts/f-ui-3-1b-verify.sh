#!/usr/bin/env bash
# F-UI-3.1b: LoadMenuBar return trace (xrc rebuild) + SetMenuBar trace (core patch only).
# Observation only — no CodeLite / wxFrame logic changes.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
HAP="$ROOT/host/fw2-hap/entry/build/default/outputs/default/entry-default-unsigned.hap"
LIBS="$ROOT/host/fw2-hap/entry/libs/arm64-v8a"
TARGET="${HDC_TARGET:-127.0.0.1:5555}"
BUNDLE=com.codelite.fw2.host
LOG="$ROOT/docs/logs/f-ui-3-1b-verify-$(date +%Y%m%d-%H%M%S).txt"

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
  echo "[f-ui-3.1b] Step 0 — F-UI-2 golden gate"
  step0_ok=0
  for attempt in 1 2 3; do
    echo "[f-ui-3.1b] Step 0 attempt $attempt/3"
    bash "$ROOT/host/fw2-hap/scripts/restore-f-ui-2-golden-snapshot.sh"
    pack_hap
    hdc -t "$TARGET" uninstall "$BUNDLE" 2>/dev/null || true
    hdc -t "$TARGET" install "$HAP"
    hdc -t "$TARGET" shell hilog -r
    hdc -t "$TARGET" shell aa start -a EntryAbility -b "$BUNDLE"
    prelog="$ROOT/docs/logs/f-ui-2-golden-precheck-$(date +%Y%m%d-%H%M%S)-3b-a${attempt}.txt"
    if run_golden_gate_poll "$prelog"; then
      step0_ok=1
      echo "[f-ui-3.1b] Step 0 PASS — $prelog"
      break
    fi
    sleep 2
  done
  if [[ "$step0_ok" -ne 1 ]]; then
    echo "[f-ui-3.1b] ABORT — F-UI-2 gate failed" >&2
    exit 1
  fi
fi

echo "[f-ui-3.1b] Step 1 — golden restore + xrc rebuild (LoadMenuBar return trace)"
bash "$ROOT/host/fw2-hap/scripts/restore-f-ui-2-golden-snapshot.sh"
bash "$ROOT/host/fw2-hap/scripts/merge-wx-xrc-fui2.sh"

echo "[f-ui-3.1b] Step 2 — SetMenuBar trace on core (F-UI-3.1 probe, unchanged)"
python3 "$ROOT/host/fw2-hap/scripts/patch-wx-fui3-1-setmenubar-trace.py"
cp -f "$LIBS/libwx_ohosu_core-3.3-OHOS.so.4.0.0" "$LIBS/libwx_ohosu_core-3.3-OHOS.so.4" 2>/dev/null || true
cp -f "$LIBS/libwx_ohosu_core-3.3-OHOS.so.4.0.0" "$LIBS/libwx_ohosu_core-3.3-OHOS.so" 2>/dev/null || true
for base in libwx_ohosu_xrc; do
  cp -f "$LIBS/${base}-3.3-OHOS.so.4.0.0" "$LIBS/${base}-3.3-OHOS.so.4" 2>/dev/null || true
  cp -f "$LIBS/${base}-3.3-OHOS.so.4.0.0" "$LIBS/${base}-3.3-OHOS.so" 2>/dev/null || true
done

echo "[f-ui-3.1b] Step 3 — cold install"
pack_hap
hdc -t "$TARGET" uninstall "$BUNDLE" 2>/dev/null || true
hdc -t "$TARGET" install "$HAP"
hdc -t "$TARGET" shell hilog -r
hdc -t "$TARGET" shell aa start -a EntryAbility -b "$BUNDLE"

echo "[f-ui-3.1b] Step 4 — poll slice gates (up to ${F_UI3_POLL_TIMEOUT:-45}s)"
: > "$LOG"
deadline=$(($(date +%s) + ${F_UI3_POLL_TIMEOUT:-45}))
while [[ $(date +%s) -lt "$deadline" ]]; do
  hdc -t "$TARGET" shell hilog -x 2>/dev/null >> "$LOG" || true
  if grep -qE '\[FUI_FRAME\] SetMenuBar enter' "$LOG"; then
    break
  fi
  if grep -qE 'THREAD_BLOCK|cppcrash-com\.codelite|SIGSEGV' "$LOG" \
     && grep -qE '\[FUI_XRC\] LoadMenuBar return' "$LOG"; then
    break
  fi
  sleep 2
done
hdc -t "$TARGET" shell hilog -x 2>/dev/null >> "$LOG" || true

grep -E "FUI_FRAME|FUI_XRC.*LoadMenuBar|MenuBar handler created|CreateResource return class=wxMenuBar|FUI_MENU.*append.*Harmony|THREAD_BLOCK" "$LOG" \
  | tail -60 || true

echo "[f-ui-3.1b] log saved: $LOG"
echo "[f-ui-3.1b] slice gates:"
set +e
bash "$ROOT/host/fw2-hap/scripts/check-f-ui-3-1b-gates.sh" "$LOG"
rc=$?
set -e

echo "[f-ui-3.1b] classification:"
if grep -qE '\[FUI_FRAME\] SetMenuBar enter' "$LOG" \
   && grep -qE '\[FUI_FRAME\] SetMenuBar return' "$LOG"; then
  echo "  → F-UI-3.1 complete + return: proceed F-UI-3.2 (post-SetMenuBar)"
elif grep -qE '\[FUI_FRAME\] SetMenuBar enter' "$LOG"; then
  echo "  → SetMenuBar enter without return: wxFrame layer (F-UI-3.2 wxFrame only)"
elif grep -qE '\[FUI_XRC\] LoadMenuBar return' "$LOG"; then
  echo "  → LoadMenuBar returned, no SetMenuBar: caller gap (CodeLite pre-attach)"
else
  echo "  → No LoadMenuBar return: XRC unwind or earlier hang"
fi

exit "$rc"
