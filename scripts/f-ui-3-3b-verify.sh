#!/usr/bin/env bash
# F-UI-3.3b: AttachMenuBar / wxMenuBar::Attach slice after F-UI-3.3c clMainFrame fix.
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
  echo "[f-ui-3.3b] ABORT: no hdc device (start MateBook Pro emulator first)" >&2
  exit 1
}

pack_wx() {
  cd "$ROOT/host/fw2-hap/entry"
  cp -f "$LIBS/libwx_ohosu_core-3.3-OHOS.so.4.0.0" "$LIBS/libwx_ohosu_core-3.3-OHOS.so.4" 2>/dev/null || true
  cp -f "$LIBS/libwx_ohosu_core-3.3-OHOS.so.4.0.0" "$LIBS/libwx_ohosu_core-3.3-OHOS.so" 2>/dev/null || true
  zip -u "$HAP" \
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

echo "[f-ui-3.3b] Step 1 — golden + F-UI-3.3c fix + AttachMenuBar probes (no 3.2b/3.3a)"
bash "$ROOT/host/fw2-hap/scripts/restore-f-ui-2-golden-snapshot.sh"
python3 "$ROOT/host/fw2-hap/scripts/patch-wx-fui3-3c-getmenubar-vtable-offset-fix.py"
python3 "$ROOT/host/fw2-hap/scripts/patch-wx-fui3-3b-attachmenubar-trace.py"
pack_wx

echo "[f-ui-3.3b] Step 2 — cold install"
ensure_device
"$HDC" -t "$TARGET" uninstall "$BUNDLE" 2>/dev/null || true
"$HDC" -t "$TARGET" install "$HAP"
"$HDC" -t "$TARGET" shell hilog -r
"$HDC" -t "$TARGET" shell aa start -a EntryAbility -b "$BUNDLE"

echo "[f-ui-3.3b] Step 3 — poll AttachMenuBar slice (${F_UI33B_ATTEMPTS:-5} attempts)"
LOG=""
for attempt in $(seq 1 ${F_UI33B_ATTEMPTS:-5}); do
  LOG="$ROOT/docs/logs/f-ui-3-3b-verify-$(date +%Y%m%d-%H%M%S)-a${attempt}.txt"
  if [[ "$attempt" -gt 1 ]]; then
    "$HDC" -t "$TARGET" shell hilog -r 2>/dev/null || true
    if [[ "${F_UI33B_RETRY_REINSTALL:-0}" == "1" ]]; then
      pack_wx
      "$HDC" -t "$TARGET" uninstall "$BUNDLE" 2>/dev/null || true
      "$HDC" -t "$TARGET" install "$HAP"
    fi
    force_restart_app
    sleep 2
  fi
  : > "$LOG"
  deadline=$(($(date +%s) + ${F_UI33B_POLL_TIMEOUT:-90}))
  while [[ $(date +%s) -lt "$deadline" ]]; do
    "$HDC" -t "$TARGET" shell hilog -x 2>/dev/null >> "$LOG" || true
    if grep -qF '[FUI_FRAMEBASE] AttachMenuBar return' "$LOG"; then
      echo "[f-ui-3.3b] attempt $attempt: AttachMenuBar return OK"
      break
    fi
    if grep -qF '[FUI_FRAMEBASE] AttachMenuBar Attach blr' "$LOG" \
       && grep -qE 'THREAD_BLOCK|SIGSEGV' "$LOG"; then
      echo "[f-ui-3.3b] attempt $attempt: hang/crash after Attach blr"
      break
    fi
    if grep -qF '[FUI_FRAMEBASE] post DetachMenuBar' "$LOG"; then
      echo "[f-ui-3.3b] attempt $attempt: post-Detach seen"
      break
    fi
    if grep -qE 'SIGSEGV' "$LOG" && grep -qF '[FUI_XRC] LoadMenuBar enter' "$LOG"; then
      echo "[f-ui-3.3b] attempt $attempt: SIGSEGV after LoadMenuBar"
      break
    fi
    sleep 2
  done
  "$HDC" -t "$TARGET" shell hilog -x 2>/dev/null >> "$LOG" || true
  if grep -qF '[FUI_FRAMEBASE] post DetachMenuBar' "$LOG" \
     || grep -qF '[FUI_FRAMEBASE] AttachMenuBar enter' "$LOG"; then
    break
  fi
  echo "[f-ui-3.3b] attempt $attempt: no AttachMenuBar slice yet"
done

grep -E 'FUI_FRAMEBASE|FUI_MENUBAR|FUI_XRC.*LoadMenuBar return|THREAD_BLOCK|SIGSEGV' "$LOG" | tail -40 || true
echo "[f-ui-3.3b] log saved: $LOG"
echo "[f-ui-3.3b] gates:"
set +e
bash "$ROOT/host/fw2-hap/scripts/check-f-ui-3-3-gates.sh" "$LOG"
exit $?
