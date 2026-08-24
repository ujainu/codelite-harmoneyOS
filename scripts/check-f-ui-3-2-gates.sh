#!/usr/bin/env bash
# F-UI-3.2 gates: wxFrame::SetMenuBar enter/return (libwx_ohosu_core only).
set -euo pipefail
LOG="${1:?usage: check-f-ui-3-2-gates.sh <hilog.txt>}"

has() { grep -qF "$1" "$LOG"; }

if has '[FUI_XRC] LoadMenuBar return'; then
  echo "[f-ui-3.2-gate] PASS LoadMenuBar return"
else
  echo "[f-ui-3.2-gate] FAIL missing LoadMenuBar return"
fi

for probe in 'SetMenuBar enter' 'SetMenuBar return'; do
  if has "[FUI_FRAME] $probe"; then
    echo "[f-ui-3.2-gate] PASS $probe"
  else
    echo "[f-ui-3.2-gate] FAIL missing $probe"
  fi
done

if has '[FUI_FRAME_FLOW] before SetMenuBar'; then
  echo "[f-ui-3.2-gate] PASS before SetMenuBar (optional caller probe)"
fi

if grep -qE 'SIGSEGV|cppcrash-com\.codelite' "$LOG"; then
  echo "[f-ui-3.2-gate] FAIL crash"
  exit 1
fi

# Case B: wxFrame SetMenuBar completed
if has '[FUI_FRAME] SetMenuBar enter' && has '[FUI_FRAME] SetMenuBar return'; then
  echo "[f-ui-3.2-gate] Case B — SetMenuBar enter + return (F-UI-3.3 StatusBar/AUI next)"
  exit 0
fi

# Case A: entered SetMenuBar, hung inside
if has '[FUI_FRAME] SetMenuBar enter' && ! has '[FUI_FRAME] SetMenuBar return'; then
  if grep -qE 'THREAD_BLOCK' "$LOG"; then
    echo "[f-ui-3.2-gate] Case A — SetMenuBar enter, no return + THREAD_BLOCK"
  else
    echo "[f-ui-3.2-gate] Case A — SetMenuBar enter, no return"
  fi
  exit 10
fi

# Case C: caller reached menu path but wxFrame::SetMenuBar never entered
if has '[FUI_XRC] LoadMenuBar return' && ! has '[FUI_FRAME] SetMenuBar enter'; then
  if has '[FUI_FRAME_FLOW] before SetMenuBar'; then
    echo "[f-ui-3.2-gate] Case C — before SetMenuBar, no SetMenuBar enter (vtable/dispatch)"
  else
    echo "[f-ui-3.2-gate] Case C — LoadMenuBar return, no SetMenuBar enter (vtable/dispatch)"
  fi
  exit 20
fi

echo "[f-ui-3.2-gate] inconclusive — inspect log ordering"
exit 15
