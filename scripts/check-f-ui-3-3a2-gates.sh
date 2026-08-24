#!/usr/bin/env bash
# F-UI-3.3a-2 gates: GetMenuBar target + thunk entry.
set -euo pipefail
export LC_ALL=C
LOG="${1:?usage: check-f-ui-3-3a2-gates.sh <hilog.txt>}"

has() { grep -qF "$1" "$LOG"; }

for probe in \
  'SetMenuBar body' \
  'target=' \
  'post GetMenuBar cmp'; do
  if has "$probe"; then
    echo "[f-ui-3.3a2-gate] PASS $probe"
  else
    echo "[f-ui-3.3a2-gate] FAIL missing $probe"
  fi
done

if has '[FUI_GETMB] GetMenuBar enter'; then
  echo "[f-ui-3.3a2-gate] PASS core GetMenuBar enter (optional probe)"
fi
if has '[FUI_GETMB] app GetMenuBar enter'; then
  echo "[f-ui-3.3a2-gate] PASS app GetMenuBar enter (optional probe)"
fi

if grep -qE 'SIGSEGV|cppcrash-com\.codelite' "$LOG"; then
  echo "[f-ui-3.3a2-gate] FAIL crash"
  exit 1
fi

if has '[FUI_GETMB] post GetMenuBar cmp'; then
  echo "[f-ui-3.3a2-gate] Case B — GetMenuBar returned (DetachMenuBar next)"
  exit 0
fi

if has '[FUI_GETMB] GetMenuBar enter' || has '[FUI_GETMB] app GetMenuBar enter'; then
  echo "[f-ui-3.3a2-gate] note: GetMenuBar thunk entered (global probe; ldr+ret should instant)"
fi

if has '[FUI_GETMB] target='; then
  target_line=$(grep -F '[FUI_GETMB] target=' "$LOG" | head -1 || true)
  echo "[f-ui-3.3a2-gate] Case A1 — blr target logged, hang inside virtual callee"
  echo "[f-ui-3.3a2-gate] note: $target_line"
  exit 10
fi

if has '[FUI_GETMB] SetMenuBar body'; then
  echo "[f-ui-3.3a2-gate] Case A0b — SetMenuBar body reached, hang before GetMenuBar blr"
  exit 9
fi

if has '[FUI_VTABLE] slot='; then
  echo "[f-ui-3.3a2-gate] Case A0 — vtable ok, hang before SetMenuBar body"
  exit 8
fi

echo "[f-ui-3.3a2-gate] inconclusive"
exit 15
