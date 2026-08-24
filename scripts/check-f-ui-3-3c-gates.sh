#!/usr/bin/env bash
# F-UI-3.3c gates: GetMenuBar vtable offset fix should unblock SetMenuBar slice.
set -euo pipefail
export LC_ALL=C
LOG="${1:?usage: check-f-ui-3-3c-gates.sh <hilog.txt>}"

has() { grep -qF "$1" "$LOG"; }

if has '[FUI_XRC] LoadMenuBar return'; then
  echo "[f-ui-3.3c-gate] PASS LoadMenuBar return"
else
  echo "[f-ui-3.3c-gate] FAIL missing LoadMenuBar return"
fi

for probe in \
  'post GetMenuBar cmp' \
  'post DetachMenuBar'; do
  if has "$probe"; then
    echo "[f-ui-3.3c-gate] PASS $probe"
  else
    echo "[f-ui-3.3c-gate] FAIL missing $probe"
  fi
done

if grep -qE 'SIGSEGV|cppcrash-com\.codelite' "$LOG"; then
  echo "[f-ui-3.3c-gate] FAIL crash"
  exit 1
fi

if has '[FUI_FRAMEBASE] post DetachMenuBar'; then
  echo "[f-ui-3.3c-gate] Case B — fix OK, post-Detach reached (F-UI-3.3b AttachMenuBar next)"
  exit 0
fi

if has '[FUI_FRAMEBASE] post GetMenuBar cmp' && ! has '[FUI_FRAMEBASE] post DetachMenuBar'; then
  echo "[f-ui-3.3c-gate] Case A2 — GetMenuBar passed, hang in DetachMenuBar"
  exit 11
fi

if has '[FUI_VTABLE] slot=' && ! has '[FUI_FRAMEBASE] post GetMenuBar cmp'; then
  echo "[f-ui-3.3c-gate] Case A1 — fix insufficient, still hang in GetMenuBar"
  exit 10
fi

echo "[f-ui-3.3c-gate] inconclusive"
exit 15
