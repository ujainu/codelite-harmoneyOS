#!/usr/bin/env bash
# F-UI-3.3: wxFrameBase::SetMenuBar internal slice (libwx_ohosu_core only).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
HAP="$ROOT/host/fw2-hap/entry/build/default/outputs/default/entry-default-unsigned.hap"
LIBS="$ROOT/host/fw2-hap/entry/libs/arm64-v8a"
TARGET="${HDC_TARGET:-127.0.0.1:5555}"
BUNDLE=com.codelite.fw2.host

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

force_restart_app() {
  hdc -t "$TARGET" shell aa force-stop "$BUNDLE" 2>/dev/null || true
  hdc -t "$TARGET" shell aa start -a EntryAbility -b "$BUNDLE"
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

if [[ "${F_UI33_SKIP_GOLDEN_GATE:-0}" != "1" ]]; then
  echo "[f-ui-3.3] Step 0 — F-UI-2 golden gate"
  step0_ok=0
  for attempt in 1 2 3; do
    bash "$ROOT/host/fw2-hap/scripts/restore-f-ui-2-golden-snapshot.sh"
    pack_hap
    hdc -t "$TARGET" uninstall "$BUNDLE" 2>/dev/null || true
    hdc -t "$TARGET" install "$HAP"
    hdc -t "$TARGET" shell hilog -r
    hdc -t "$TARGET" shell aa start -a EntryAbility -b "$BUNDLE"
    prelog="$ROOT/docs/logs/f-ui-2-golden-precheck-$(date +%Y%m%d-%H%M%S)-33-a${attempt}.txt"
    if run_golden_gate_poll "$prelog"; then
      step0_ok=1
      break
    fi
    hdc -t "$TARGET" shell aa force-stop "$BUNDLE" 2>/dev/null || true
    sleep 2
  done
  [[ "$step0_ok" -eq 1 ]] || { echo "[f-ui-3.3] ABORT F-UI-2 gate" >&2; exit 1; }
fi

echo "[f-ui-3.3] Step 1 — golden + vtable (caller) + wxFrameBase path probes"
bash "$ROOT/host/fw2-hap/scripts/restore-f-ui-2-golden-snapshot.sh"
python3 "$ROOT/host/fw2-hap/scripts/patch-f-ui-3-2b-vtable-dispatch-trace.py"
python3 "$ROOT/host/fw2-hap/scripts/patch-wx-fui3-3-framebase-setmenubar-trace.py"

cp -f "$LIBS/libwx_ohosu_core-3.3-OHOS.so.4.0.0" "$LIBS/libwx_ohosu_core-3.3-OHOS.so.4" 2>/dev/null || true
cp -f "$LIBS/libwx_ohosu_core-3.3-OHOS.so.4.0.0" "$LIBS/libwx_ohosu_core-3.3-OHOS.so" 2>/dev/null || true

echo "[f-ui-3.3] Step 2 — cold install"
pack_hap
hdc -t "$TARGET" uninstall "$BUNDLE" 2>/dev/null || true
hdc -t "$TARGET" install "$HAP"
hdc -t "$TARGET" shell hilog -r
hdc -t "$TARGET" shell aa start -a EntryAbility -b "$BUNDLE"

echo "[f-ui-3.3] Step 3 — poll framebase path (${F_UI33_SLICE_ATTEMPTS:-5} attempts)"
LOG=""
for slice_attempt in $(seq 1 ${F_UI33_SLICE_ATTEMPTS:-5}); do
  LOG="$ROOT/docs/logs/f-ui-3-3-verify-$(date +%Y%m%d-%H%M%S)-a${slice_attempt}.txt"
  if [[ "$slice_attempt" -gt 1 ]]; then
    hdc -t "$TARGET" shell hilog -r
    if [[ "${F_UI33_RETRY_REINSTALL:-1}" == "1" ]]; then
      pack_hap
      hdc -t "$TARGET" uninstall "$BUNDLE" 2>/dev/null || true
      hdc -t "$TARGET" install "$HAP"
    fi
    force_restart_app
    sleep 2
  fi
  : > "$LOG"
  deadline=$(($(date +%s) + ${F_UI33_POLL_TIMEOUT:-90}))
  while [[ $(date +%s) -lt "$deadline" ]]; do
    hdc -t "$TARGET" shell hilog -x 2>/dev/null >> "$LOG" || true
    if grep -qF '[FUI_FRAMEBASE] AttachMenuBar return' "$LOG"; then
      break
    fi
    if grep -qF '[FUI_FRAMEBASE] SetMenuBar enter' "$LOG" \
       && grep -qE 'THREAD_BLOCK|SIGSEGV' "$LOG"; then
      break
    fi
    if grep -qF '[FUI_FRAMEBASE] AttachMenuBar Attach blr' "$LOG" \
       && grep -qE 'THREAD_BLOCK' "$LOG"; then
      break
    fi
    sleep 2
  done
  hdc -t "$TARGET" shell hilog -x 2>/dev/null >> "$LOG" || true
    if grep -qF '[FUI_FRAMEBASE] SetMenuBar enter' "$LOG"; then
      break
    fi
    if grep -qF '[FUI_VTABLE] slot=' "$LOG" \
       && grep -qE 'THREAD_BLOCK|SIGSEGV' "$LOG"; then
      break
    fi
  echo "[f-ui-3.3] attempt $slice_attempt: no framebase slice yet"
done

grep -E 'FUI_FRAMEBASE|FUI_MENUBAR|FUI_VTABLE|FUI_XRC.*LoadMenuBar return|THREAD_BLOCK' "$LOG" | tail -35 || true

echo "[f-ui-3.3] log saved: $LOG"
echo "[f-ui-3.3] framebase gates:"
set +e
bash "$ROOT/host/fw2-hap/scripts/check-f-ui-3-3-gates.sh" "$LOG"
exit $?
