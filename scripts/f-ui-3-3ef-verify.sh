#!/usr/bin/env bash
# F-UI-3.3ef: verify SetMenuBar + MenuBar Attach boot path.
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
  [[ -n "$t" && "$t" != "[Empty]" ]]
}

force_restart_app() {
  "$HDC" -t "$TARGET" shell aa force-stop "$BUNDLE" 2>/dev/null || true
  "$HDC" -t "$TARGET" shell aa start -a EntryAbility -b "$BUNDLE"
}

echo "[f-ui-3.3ef] Step 1 — patch + stage wx core"
bash "$ROOT/host/fw2-hap/scripts/rebuild-wx-fui3-3ef-boot-menu.sh"

echo "[f-ui-3.3ef] Step 2 — cold install"
ensure_device || { echo "[f-ui-3.3ef] ABORT: no hdc device" >&2; exit 1; }
"$HDC" -t "$TARGET" uninstall "$BUNDLE" 2>/dev/null || true
"$HDC" -t "$TARGET" install "$HAP"
"$HDC" -t "$TARGET" shell hilog -r
"$HDC" -t "$TARGET" shell aa start -a EntryAbility -b "$BUNDLE"

echo "[f-ui-3.3ef] Step 3 — poll menu boot path (${F_UI33EF_ATTEMPTS:-3} attempts)"
LOG=""
for attempt in $(seq 1 ${F_UI33EF_ATTEMPTS:-3}); do
  LOG="$ROOT/docs/logs/f-ui-3-3ef-verify-$(date +%Y%m%d-%H%M%S)-a${attempt}.txt"
  [[ "$attempt" -gt 1 ]] && { "$HDC" -t "$TARGET" shell hilog -r 2>/dev/null || true; force_restart_app; sleep 2; }
  : > "$LOG"
  deadline=$(($(date +%s) + ${F_UI33EF_POLL_TIMEOUT:-120}))
  while [[ $(date +%s) -lt $deadline ]]; do
    "$HDC" -t "$TARGET" shell hilog -x 2>/dev/null >> "$LOG" || true
    if grep -qF '[R-4] MenuBar Attach frame=' "$LOG"; then
      echo "[f-ui-3.3ef] attempt $attempt: MenuBar Attach OK"
      break
    fi
    if grep -qF 'CreateRecentlyOpenedWorkspacesMenu' "$LOG" || grep -qF 'MainBook' "$LOG"; then
      echo "[f-ui-3.3ef] attempt $attempt: past statusbar/menu slice"
      break
    fi
    if grep -qE 'SIGSEGV' "$LOG" && grep -qF 'MenuBar handler created' "$LOG"; then
      echo "[f-ui-3.3ef] attempt $attempt: SIGSEGV after LoadMenuBar"
      break
    fi
    if grep -qE 'THREAD_BLOCK' "$LOG" && grep -qF 'MenuBar handler created' "$LOG"; then
      echo "[f-ui-3.3ef] attempt $attempt: THREAD_BLOCK"
      break
    fi
    if grep -qF 'CreateGUIControls' "$LOG" && grep -qF 'aui Update' "$LOG"; then
      echo "[f-ui-3.3ef] attempt $attempt: past menu — AUI Update seen"
      break
    fi
    sleep 2
  done
  "$HDC" -t "$TARGET" shell hilog -x 2>/dev/null >> "$LOG" || true
  grep -qF '[R-4] MenuBar Attach frame=' "$LOG" && break
done

grep -E 'FUI_FRAME|FUI_XRC|MenuBar|R-4|THREAD_BLOCK|SIGSEGV|CreateGUI|aui Update|clConfig' "$LOG" | tail -60 || true
echo "[f-ui-3.3ef] log saved: $LOG"
bash "$ROOT/host/fw2-hap/scripts/check-f-ui-3-3ef-gates.sh" "$LOG" || exit $?
