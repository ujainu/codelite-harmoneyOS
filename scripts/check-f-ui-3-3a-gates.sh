#!/usr/bin/env bash
# F-UI-3.3a gates: GetMenuBar vs DetachMenuBar hang inside wxFrameBase::SetMenuBar.
set -euo pipefail
export LC_ALL=C
LOG="${1:?usage: check-f-ui-3-3a-gates.sh <hilog.txt>}"

has() { grep -qF "$1" "$LOG"; }

if has '[FUI_XRC] LoadMenuBar return'; then
  echo "[f-ui-3.3a-gate] PASS LoadMenuBar return"
else
  echo "[f-ui-3.3a-gate] FAIL missing LoadMenuBar return"
fi

for probe in \
  'post GetMenuBar cmp' \
  'post DetachMenuBar'; do
  if has "$probe"; then
    echo "[f-ui-3.3a-gate] PASS $probe"
  else
    echo "[f-ui-3.3a-gate] FAIL missing $probe"
  fi
done

if grep -qE 'SIGSEGV|cppcrash-com\.codelite' "$LOG"; then
  echo "[f-ui-3.3a-gate] FAIL crash"
  exit 1
fi

# Case B: passed Detach — continue F-UI-3.3 AttachMenuBar slice
if has '[FUI_FRAMEBASE] post DetachMenuBar'; then
  echo "[f-ui-3.3a-gate] Case B — post-Detach reached (AttachMenuBar next, run F-UI-3.3b+)"
  exit 0
fi

# Case A2: GetMenuBar returned but hung inside DetachMenuBar
if has '[FUI_FRAMEBASE] post GetMenuBar cmp' && ! has '[FUI_FRAMEBASE] post DetachMenuBar'; then
  if grep -qE 'THREAD_BLOCK' "$LOG"; then
    echo "[f-ui-3.3a-gate] Case A2 — hang inside DetachMenuBar + THREAD_BLOCK"
  else
    echo "[f-ui-3.3a-gate] Case A2 — hang inside DetachMenuBar (post-GetMenuBar seen)"
  fi
  exit 11
fi

# Case A1: hung inside GetMenuBar (never reached post-return cmp)
if has '[FUI_VTABLE] slot=' && ! has '[FUI_FRAMEBASE] post GetMenuBar cmp'; then
  if grep -qE 'THREAD_BLOCK' "$LOG"; then
    echo "[f-ui-3.3a-gate] Case A1 — hang inside GetMenuBar + THREAD_BLOCK"
  else
    echo "[f-ui-3.3a-gate] Case A1 — hang inside GetMenuBar (no post-return cmp)"
  fi
  exit 10
fi

if has '[FUI_VTABLE] slot='; then
  echo "[f-ui-3.3a-gate] note: vtable slice present"
fi

echo "[f-ui-3.3a-gate] inconclusive — inspect log ordering"
exit 15
