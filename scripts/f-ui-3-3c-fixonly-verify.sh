#!/usr/bin/env bash
# F-UI-3.3c fix-only: deploy vtable drift patch without trace probes.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
HAP="$ROOT/host/fw2-hap/entry/build/default/outputs/default/entry-default-unsigned.hap"
LIBS="$ROOT/host/fw2-hap/entry/libs/arm64-v8a"
TARGET="${HDC_TARGET:-127.0.0.1:5555}"
BUNDLE=com.codelite.fw2.host

pack_wx() {
  cd "$ROOT/host/fw2-hap/entry"
  cp -f "$LIBS/libwx_ohosu_core-3.3-OHOS.so.4.0.0" "$LIBS/libwx_ohosu_core-3.3-OHOS.so.4" 2>/dev/null || true
  cp -f "$LIBS/libwx_ohosu_core-3.3-OHOS.so.4.0.0" "$LIBS/libwx_ohosu_core-3.3-OHOS.so" 2>/dev/null || true
  zip -u "$HAP" \
    libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so.4.0.0 \
    libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so.4 \
    libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so || true
}

echo "[f-ui-3.3c-fixonly] restore golden + apply SetMenuBar vtable drift fix"
bash "$ROOT/host/fw2-hap/scripts/restore-f-ui-2-golden-snapshot.sh"
python3 "$ROOT/host/fw2-hap/scripts/patch-wx-fui3-3c-getmenubar-vtable-offset-fix.py"
pack_wx

echo "[f-ui-3.3c-fixonly] cold install"
hdc -t "$TARGET" uninstall "$BUNDLE" 2>/dev/null || true
hdc -t "$TARGET" install "$HAP"
hdc -t "$TARGET" shell hilog -r
hdc -t "$TARGET" shell aa start -a EntryAbility -b "$BUNDLE"

LOG="$ROOT/docs/logs/f-ui-3-3c-fixonly-$(date +%Y%m%d-%H%M%S).txt"
: > "$LOG"
echo "[f-ui-3.3c-fixonly] poll ${F_UI33C_FIXONLY_ATTEMPTS:-3} attempts"
for attempt in $(seq 1 ${F_UI33C_FIXONLY_ATTEMPTS:-3}); do
  if [[ "$attempt" -gt 1 ]]; then
    hdc -t "$TARGET" shell hilog -r 2>/dev/null || true
    hdc -t "$TARGET" shell aa force-stop "$BUNDLE" 2>/dev/null || true
    hdc -t "$TARGET" shell aa start -a EntryAbility -b "$BUNDLE" 2>/dev/null || true
    sleep 2
  fi
  deadline=$(($(date +%s) + ${F_UI33C_FIXONLY_TIMEOUT:-60}))
  while [[ $(date +%s) -lt "$deadline" ]]; do
    hdc -t "$TARGET" shell hilog -x 2>/dev/null >> "$LOG" || true
    if grep -qF '[FUI_XRC] LoadMenuBar return' "$LOG"; then
      break
    fi
    if grep -qE 'SIGSEGV|THREAD_BLOCK_6S' "$LOG"; then
      break
    fi
    sleep 2
  done
  if grep -qF '[FUI_XRC] LoadMenuBar return' "$LOG" && ! grep -qE 'SIGSEGV' "$LOG"; then
    echo "[f-ui-3.3c-fixonly] attempt $attempt: LoadMenuBar return OK"
    break
  fi
  if grep -qE 'SIGSEGV' "$LOG"; then
    echo "[f-ui-3.3c-fixonly] attempt $attempt: SIGSEGV — retry"
  else
    echo "[f-ui-3.3c-fixonly] attempt $attempt: waiting..."
  fi
done

grep -E 'LoadMenuBar|CreateGUI|AUI|Notebook|THREAD_BLOCK|SIGSEGV|MenuBar' "$LOG" | tail -35 || true
echo "[f-ui-3.3c-fixonly] log: $LOG"

if grep -qE 'SIGSEGV|cppcrash-com\.codelite' "$LOG"; then
  echo "[f-ui-3.3c-fixonly] FAIL crash"
  exit 1
fi
if grep -qF '[FUI_XRC] LoadMenuBar return' "$LOG"; then
  echo "[f-ui-3.3c-fixonly] PASS LoadMenuBar return — re-run f-ui-3-3a with F_UI33C_FIX=1 for SetMenuBar slice"
  exit 0
fi
if grep -qE 'THREAD_BLOCK' "$LOG"; then
  echo "[f-ui-3.3c-fixonly] note: THREAD_BLOCK (may be progress vs Case A1 hang)"
  exit 10
fi
echo "[f-ui-3.3c-fixonly] inconclusive"
exit 15
