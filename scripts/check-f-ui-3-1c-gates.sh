#!/usr/bin/env bash
# F-UI-3.1c flow gates: CreateGUIControls slice between LoadMenuBar and SetMenuBar.
set -euo pipefail
LOG="${1:?usage: check-f-ui-3-1c-gates.sh <hilog.txt>}"

has() { grep -qF "$1" "$LOG"; }

if has '[FUI_XRC] LoadMenuBar return'; then
  echo "[f-ui-3.1c-gate] PASS LoadMenuBar return"
else
  echo "[f-ui-3.1c-gate] FAIL missing LoadMenuBar return"
fi

if has '[FUI_FRAME_FLOW] after LoadMenuBar'; then
  echo "[f-ui-3.1c-gate] PASS after LoadMenuBar"
else
  echo "[f-ui-3.1c-gate] FAIL missing after LoadMenuBar"
fi

if has '[FUI_FRAME_FLOW] before clConfig Read' || has '[FUI_FRAME_FLOW] before clConfig' || has '[FUI_FRAME_FLOW] before Harmony menu'; then
  echo "[f-ui-3.1c-gate] PASS before clConfig Read"
else
  echo "[f-ui-3.1c-gate] pending before clConfig Read"
fi

if has '[FUI_FRAME_FLOW] before SetMenuBar'; then
  echo "[f-ui-3.1c-gate] PASS before SetMenuBar"
else
  echo "[f-ui-3.1c-gate] pending before SetMenuBar"
fi

if has '[FUI_FRAME] SetMenuBar enter'; then
  echo "[f-ui-3.1c-gate] PASS SetMenuBar enter"
else
  echo "[f-ui-3.1c-gate] pending SetMenuBar enter"
fi

if grep -qE 'SIGSEGV|cppcrash-com\.codelite' "$LOG"; then
  echo "[f-ui-3.1c-gate] FAIL crash"
  exit 1
fi

if has '[FUI_FRAME_FLOW] before SetMenuBar' && has '[FUI_FRAME] SetMenuBar enter'; then
  echo "[f-ui-3.1c-gate] Case 1 — flow reached SetMenuBar (wxFrame layer next)"
  exit 0
fi
if has '[FUI_FRAME_FLOW] after LoadMenuBar' \
   && ! has '[FUI_FRAME_FLOW] before clConfig Read' \
   && ! has '[FUI_FRAME_FLOW] before clConfig' \
   && ! has '[FUI_FRAME_FLOW] before Harmony menu'; then
  echo "[f-ui-3.1c-gate] Case 2 — hang in wxString cleanup (0x5da484-0x5da4a0) or clConfig::Get"
  exit 2
fi
if has '[FUI_FRAME_FLOW] before clConfig Read' || has '[FUI_FRAME_FLOW] before clConfig' || has '[FUI_FRAME_FLOW] before Harmony menu'; then
  if ! has '[FUI_FRAME_FLOW] before SetMenuBar'; then
    echo "[f-ui-3.1c-gate] Case 2b — hang in clConfig::Read / showMenuBar branch"
    exit 3
  fi
fi
if has '[FUI_XRC] LoadMenuBar return' \
   && ! has '[FUI_FRAME_FLOW] after LoadMenuBar'; then
  echo "[f-ui-3.1c-gate] Case 3 — hang on first line after LoadMenuBar return"
  exit 4
fi

echo "[f-ui-3.1c-gate] inconclusive — inspect log ordering"
exit 5
