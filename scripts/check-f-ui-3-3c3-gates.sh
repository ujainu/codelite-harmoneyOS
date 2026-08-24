#!/usr/bin/env bash
# F-UI-3.3c-3 gates: caller chain + DetachMenuBar enter (SetMenuBar body not traceable).
set -euo pipefail
export LC_ALL=C
LOG="${1:?usage: check-f-ui-3-3c3-gates.sh <hilog.txt>}"

has() { grep -qF "$1" "$LOG"; }

if has 'MenuBar handler created' || has '[FUI_XRC] LoadMenuBar return'; then
  echo "[f-ui-3.3c3-gate] PASS LoadMenuBar complete"
else
  echo "[f-ui-3.3c3-gate] FAIL missing LoadMenuBar complete"
fi

for probe in \
  'after LoadMenuBar' \
  'before clConfig Read' \
  'before SetMenuBar' \
  'DetachMenuBar enter'; do
  if has "$probe"; then
    echo "[f-ui-3.3c3-gate] PASS $probe"
  else
    echo "[f-ui-3.3c3-gate] FAIL missing $probe"
  fi
done

if grep -qE 'SIGSEGV|cppcrash-com\.codelite' "$LOG"; then
  echo "[f-ui-3.3c3-gate] FAIL crash"
  exit 1
fi

if has '[FUI_FRAMEBASE] DetachMenuBar enter'; then
  echo "[f-ui-3.3c3-gate] Case D — DetachMenuBar entered (run F-UI-3.3b Attach slice)"
  exit 0
fi

if has '[FUI_FRAME_FLOW] before SetMenuBar' \
   && ! has '[FUI_FRAMEBASE] DetachMenuBar enter'; then
  if grep -qE 'THREAD_BLOCK' "$LOG"; then
    echo "[f-ui-3.3c3-gate] Case C — SetMenuBar virtual reached, hang inside wx SetMenuBar + THREAD_BLOCK"
  else
    echo "[f-ui-3.3c3-gate] Case C — SetMenuBar virtual reached, hang inside wx SetMenuBar"
  fi
  exit 13
fi

if has '[FUI_FRAME_FLOW] after LoadMenuBar' \
   && ! has '[FUI_FRAME_FLOW] before SetMenuBar'; then
  echo "[f-ui-3.3c3-gate] Case B — hang between after LoadMenuBar and before SetMenuBar (clConfig/showMenuBar path)"
  exit 12
fi

if ! has '[FUI_FRAME_FLOW] after LoadMenuBar'; then
  if grep -qE 'THREAD_BLOCK' "$LOG"; then
    echo "[f-ui-3.3c3-gate] Case A — hang before after-LoadMenuBar (LoadMenuBar return / caller) + THREAD_BLOCK"
  else
    echo "[f-ui-3.3c3-gate] Case A — hang before after-LoadMenuBar"
  fi
  exit 10
fi

echo "[f-ui-3.3c3-gate] inconclusive — inspect log ordering"
exit 15
