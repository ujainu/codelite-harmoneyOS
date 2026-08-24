#!/usr/bin/env bash
# F-UI-3.3e: verify wxFrame::SetMenuBar fix (source relink or binary direct-call patch).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
HAP="$ROOT/host/fw2-hap/entry/build/default/outputs/default/entry-default-unsigned.hap"
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
  echo "[f-ui-3.3e] ABORT: no hdc device" >&2
  exit 1
}

force_restart_app() {
  "$HDC" -t "$TARGET" shell aa force-stop "$BUNDLE" 2>/dev/null || true
  "$HDC" -t "$TARGET" shell aa start -a EntryAbility -b "$BUNDLE"
}

echo "[f-ui-3.3e] Step 1 — rebuild + stage wx core (SetMenuBar fix)"
bash "$ROOT/host/fw2-hap/scripts/rebuild-wx-fui3-3e-setmenubar.sh"

echo "[f-ui-3.3e] Step 2 — cold install (golden libcodelite, new wx core only)"
ensure_device
"$HDC" -t "$TARGET" uninstall "$BUNDLE" 2>/dev/null || true
"$HDC" -t "$TARGET" install "$HAP"
"$HDC" -t "$TARGET" shell hilog -r
"$HDC" -t "$TARGET" shell aa start -a EntryAbility -b "$BUNDLE"

echo "[f-ui-3.3e] Step 3 — poll SetMenuBar / menu path (${F_UI33E_ATTEMPTS:-3} attempts)"
LOG=""
for attempt in $(seq 1 ${F_UI33E_ATTEMPTS:-3}); do
  LOG="$ROOT/docs/logs/f-ui-3-3e-verify-$(date +%Y%m%d-%H%M%S)-a${attempt}.txt"
  if [[ "$attempt" -gt 1 ]]; then
    "$HDC" -t "$TARGET" shell hilog -r 2>/dev/null || true
    force_restart_app
    sleep 2
  fi
  : > "$LOG"
  deadline=$(($(date +%s) + ${F_UI33E_POLL_TIMEOUT:-90}))
  while [[ $(date +%s) -lt "$deadline" ]]; do
    "$HDC" -t "$TARGET" shell hilog -x 2>/dev/null >> "$LOG" || true
    if grep -qF '[FUI_FRAME] SetMenuBar menubar=' "$LOG" \
       || grep -qF '[R-4] MenuBar Attach frame=' "$LOG"; then
      echo "[f-ui-3.3e] attempt $attempt: SetMenuBar path OK"
      break
    fi
    if grep -qE 'SIGSEGV' "$LOG" && grep -qF 'MenuBar handler created' "$LOG"; then
      echo "[f-ui-3.3e] attempt $attempt: SIGSEGV after LoadMenuBar"
      break
    fi
    if grep -qE 'THREAD_BLOCK' "$LOG" \
       && grep -qF 'MenuBar handler created' "$LOG"; then
      echo "[f-ui-3.3e] attempt $attempt: THREAD_BLOCK after LoadMenuBar"
      break
    fi
    sleep 2
  done
  "$HDC" -t "$TARGET" shell hilog -x 2>/dev/null >> "$LOG" || true
  if grep -qF '[FUI_FRAME] SetMenuBar menubar=' "$LOG" \
     || grep -qF '[R-4] MenuBar Attach frame=' "$LOG"; then
    break
  fi
  echo "[f-ui-3.3e] attempt $attempt: SetMenuBar path not confirmed yet"
done

grep -E 'FUI_FRAME|FUI_XRC|MenuBar handler|R-4.*MenuBar|THREAD_BLOCK|SIGSEGV|CreateGUI|aui Update' "$LOG" | tail -50 || true
echo "[f-ui-3.3e] log saved: $LOG"
echo "[f-ui-3.3e] gates:"
set +e
bash "$ROOT/host/fw2-hap/scripts/check-f-ui-3-3e-gates.sh" "$LOG"
exit $?
