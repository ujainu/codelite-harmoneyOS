#!/usr/bin/env bash
# F-UI-3.3b-2 gates: DetachMenuBar enter vs SetMenuBar pre-Detach hang.
set -euo pipefail
export LC_ALL=C
LOG="${1:?usage: check-f-ui-3-3b2-gates.sh <hilog.txt>}"

has() { grep -qF "$1" "$LOG"; }

if has '[FUI_XRC] LoadMenuBar return' || has 'MenuBar handler created'; then
  echo "[f-ui-3.3b2-gate] PASS LoadMenuBar complete"
else
  echo "[f-ui-3.3b2-gate] FAIL missing LoadMenuBar complete"
fi

if has '[FUI_FRAMEBASE] DetachMenuBar enter'; then
  echo "[f-ui-3.3b2-gate] PASS DetachMenuBar enter"
else
  echo "[f-ui-3.3b2-gate] FAIL missing DetachMenuBar enter"
fi

if grep -qE 'SIGSEGV|cppcrash-com\.codelite' "$LOG"; then
  echo "[f-ui-3.3b2-gate] FAIL crash"
  exit 1
fi

# Case B: entered DetachMenuBar but did not return (no post-Detach in full 3.3b runs)
if has '[FUI_FRAMEBASE] DetachMenuBar enter' \
   && ! has '[FUI_FRAMEBASE] post DetachMenuBar'; then
  if grep -qE 'THREAD_BLOCK' "$LOG"; then
    echo "[f-ui-3.3b2-gate] Case B — hang inside DetachMenuBar + THREAD_BLOCK"
  else
    echo "[f-ui-3.3b2-gate] Case B — hang inside DetachMenuBar"
  fi
  exit 12
fi

# Case A: SetMenuBar never reached DetachMenuBar enter
if ! has '[FUI_FRAMEBASE] DetachMenuBar enter'; then
  if grep -qE 'THREAD_BLOCK' "$LOG"; then
    echo "[f-ui-3.3b2-gate] Case A — hang in SetMenuBar before DetachMenuBar enter + THREAD_BLOCK"
  else
    echo "[f-ui-3.3b2-gate] Case A — hang in SetMenuBar before DetachMenuBar enter"
  fi
  exit 10
fi

if has '[FUI_FRAMEBASE] post DetachMenuBar'; then
  echo "[f-ui-3.3b2-gate] Case C — DetachMenuBar returned (run full F-UI-3.3b Attach slice)"
  exit 0
fi

echo "[f-ui-3.3b2-gate] inconclusive — inspect log ordering"
exit 15
