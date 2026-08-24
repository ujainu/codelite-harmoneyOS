#!/usr/bin/env bash
# F-UI-3.3c-3: caller-chain slice after F-UI-3.3c (avoid SetMenuBar body probes — SIGSEGV).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
HAP="$ROOT/host/fw2-hap/entry/build/default/outputs/default/entry-default-unsigned.hap"
LIBS="$ROOT/host/fw2-hap/entry/libs/arm64-v8a"
TARGET="${HDC_TARGET:-127.0.0.1:5555}"
HDC="${HDC:-/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains/hdc}"
BUNDLE=com.codelite.fw2.host

ensure_device() {
  local t
  t=$("$HDC" list targets 2>/dev/null | head -1 | tr -d '\r\n')
  if [[ -n "$t" && "$t" != "[Empty]" ]]; then
    TARGET="$t"
    return 0
  fi
  "$HDC" tconn "$TARGET" 2>/dev/null || true
  t=$("$HDC" list targets 2>/dev/null | head -1 | tr -d '\r\n')
  if [[ -n "$t" && "$t" != "[Empty]" ]]; then
    TARGET="$t"
    return 0
  fi
  echo "[f-ui-3.3c-3] ABORT: no hdc device" >&2
  exit 1
}

pack_hap() {
  cd "$ROOT/host/fw2-hap/entry"
  cp -f "$LIBS/libwx_ohosu_core-3.3-OHOS.so.4.0.0" "$LIBS/libwx_ohosu_core-3.3-OHOS.so.4" 2>/dev/null || true
  cp -f "$LIBS/libwx_ohosu_core-3.3-OHOS.so.4.0.0" "$LIBS/libwx_ohosu_core-3.3-OHOS.so" 2>/dev/null || true
  zip -u "$HAP" \
    libs/arm64-v8a/libcodelite_app.so \
    libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so.4.0.0 \
    libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so.4 \
    libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so \
    libs/arm64-v8a/libwx_ohosu_xrc-3.3-OHOS.so.4.0.0 \
    libs/arm64-v8a/libwx_ohosu_xrc-3.3-OHOS.so.4 \
    libs/arm64-v8a/libwx_ohosu_xrc-3.3-OHOS.so || true
}

force_restart_app() {
  "$HDC" -t "$TARGET" shell aa force-stop "$BUNDLE" 2>/dev/null || true
  "$HDC" -t "$TARGET" shell aa start -a EntryAbility -b "$BUNDLE"
}

echo "[f-ui-3.3c-3] Step 1 — golden + 3.3c fix + frame-flow (app) + Detach enter (core)"
bash "$ROOT/host/fw2-hap/scripts/restore-f-ui-2-golden-snapshot.sh"
python3 "$ROOT/host/fw2-hap/scripts/patch-wx-fui3-3c-getmenubar-vtable-offset-fix.py"
python3 "$ROOT/host/fw2-hap/scripts/patch-f-ui-3-1c-frame-flow-trace.py"
python3 "$ROOT/host/fw2-hap/scripts/patch-wx-fui3-3b2-detach-enter-trace.py"
pack_hap

echo "[f-ui-3.3c-3] Step 2 — cold install"
ensure_device
"$HDC" -t "$TARGET" uninstall "$BUNDLE" 2>/dev/null || true
"$HDC" -t "$TARGET" install "$HAP"
"$HDC" -t "$TARGET" shell hilog -r
"$HDC" -t "$TARGET" shell aa start -a EntryAbility -b "$BUNDLE"

echo "[f-ui-3.3c-3] Step 3 — poll caller chain (${F_UI33C3_ATTEMPTS:-3} attempts)"
LOG=""
for attempt in $(seq 1 ${F_UI33C3_ATTEMPTS:-3}); do
  LOG="$ROOT/docs/logs/f-ui-3-3c3-verify-$(date +%Y%m%d-%H%M%S)-a${attempt}.txt"
  if [[ "$attempt" -gt 1 ]]; then
    "$HDC" -t "$TARGET" shell hilog -r 2>/dev/null || true
    force_restart_app
    sleep 2
  fi
  : > "$LOG"
  deadline=$(($(date +%s) + ${F_UI33C3_POLL_TIMEOUT:-90}))
  while [[ $(date +%s) -lt "$deadline" ]]; do
    "$HDC" -t "$TARGET" shell hilog -x 2>/dev/null >> "$LOG" || true
    if grep -qF '[FUI_FRAMEBASE] DetachMenuBar enter' "$LOG"; then
      echo "[f-ui-3.3c-3] attempt $attempt: DetachMenuBar enter OK"
      break
    fi
    if grep -qF '[FUI_FRAME_FLOW] before SetMenuBar' "$LOG"; then
      echo "[f-ui-3.3c-3] attempt $attempt: before SetMenuBar seen"
      break
    fi
    if grep -qF '[FUI_FRAME_FLOW] after LoadMenuBar' "$LOG"; then
      echo "[f-ui-3.3c-3] attempt $attempt: after LoadMenuBar seen"
      break
    fi
    if grep -qE 'SIGSEGV' "$LOG" && grep -qF 'MenuBar handler created' "$LOG"; then
      echo "[f-ui-3.3c-3] attempt $attempt: SIGSEGV after LoadMenuBar"
      break
    fi
    if grep -qE 'THREAD_BLOCK' "$LOG" \
       && grep -qF 'MenuBar handler created' "$LOG"; then
      echo "[f-ui-3.3c-3] attempt $attempt: THREAD_BLOCK after LoadMenuBar"
      break
    fi
    sleep 2
  done
  "$HDC" -t "$TARGET" shell hilog -x 2>/dev/null >> "$LOG" || true
  if grep -qF '[FUI_FRAME_FLOW] after LoadMenuBar' "$LOG"; then
    break
  fi
  echo "[f-ui-3.3c-3] attempt $attempt: no caller-chain slice yet"
done

grep -E 'FUI_FRAME_FLOW|FUI_FRAMEBASE|FUI_XRC|MenuBar handler|THREAD_BLOCK|SIGSEGV' "$LOG" | tail -50 || true
echo "[f-ui-3.3c-3] log saved: $LOG"
echo "[f-ui-3.3c-3] gates:"
set +e
bash "$ROOT/host/fw2-hap/scripts/check-f-ui-3-3c3-gates.sh" "$LOG"
exit $?
