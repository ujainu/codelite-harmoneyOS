#!/usr/bin/env bash
# F-UI-3.1e flow gates: clConfig::Get/Read slice in CreateGUIControls.
set -euo pipefail
LOG="${1:?usage: check-f-ui-3-1e-gates.sh <hilog.txt>}"

has() { grep -qF "$1" "$LOG"; }

if has '[FUI_XRC] LoadMenuBar return'; then
  echo "[f-ui-3.1e-gate] PASS LoadMenuBar return"
else
  echo "[f-ui-3.1e-gate] FAIL missing LoadMenuBar return"
fi

for probe in \
  'clConfig begin' \
  'Get return' \
  'Read enter' \
  'Read return' \
  'before SetMenuBar'; do
  key="[FUI_CONFIG] $probe"
  alt="[FUI_FRAME_FLOW] $probe"
  if has "$key" || { [[ "$probe" == "before SetMenuBar" ]] && has "$alt"; }; then
    echo "[f-ui-3.1e-gate] PASS $probe"
  else
    echo "[f-ui-3.1e-gate] FAIL missing $probe"
  fi
done

if grep -qE 'SIGSEGV|cppcrash-com\.codelite' "$LOG"; then
  echo "[f-ui-3.1e-gate] FAIL crash"
  exit 1
fi

# Case C: config path complete, at SetMenuBar doorstep
if has '[FUI_CONFIG] Read return' \
   && { has '[FUI_FRAME_FLOW] before SetMenuBar' || has '[FUI_CONFIG] before SetMenuBar'; }; then
  if has '[FUI_FRAME] SetMenuBar enter'; then
    echo "[f-ui-3.1e-gate] Case C+ — Read return + before SetMenuBar + SetMenuBar enter"
    exit 0
  fi
  echo "[f-ui-3.1e-gate] Case C — Read return + before SetMenuBar (F-UI-3.2 wxFrame next)"
  exit 20
fi

# Case B: Get ok, Read hangs
if has '[FUI_CONFIG] Get return' \
   && has '[FUI_CONFIG] Read enter' \
   && ! has '[FUI_CONFIG] Read return'; then
  echo "[f-ui-3.1e-gate] Case B — hang inside clConfig::Read (liblibcodelite)"
  exit 11
fi

# Case B2: between Get return and Read enter (wxString key setup)
if has '[FUI_CONFIG] Get return' \
   && ! has '[FUI_CONFIG] Read enter'; then
  echo "[f-ui-3.1e-gate] Case B2 — hang between Get return and Read enter (0x5da4a8–0x5da4c8)"
  exit 12
fi

# Case A: Get hangs (liblibcodelite singleton)
if has '[FUI_CONFIG] clConfig begin' \
   && ! has '[FUI_CONFIG] Get return'; then
  echo "[f-ui-3.1e-gate] Case A — hang inside clConfig::Get"
  exit 10
fi

if has '[FUI_CONFIG] Read return' \
   && ! has '[FUI_FRAME_FLOW] before SetMenuBar'; then
  echo "[f-ui-3.1e-gate] Case B3 — hang after Read return, before SetMenuBar branch"
  exit 13
fi

if has '[FUI_XRC] LoadMenuBar return' \
   && ! has '[FUI_CONFIG] clConfig begin'; then
  echo "[f-ui-3.1e-gate] Case 0 — hang before clConfig (cleanup or pre-0x5da4a4)"
  exit 14
fi

echo "[f-ui-3.1e-gate] inconclusive — inspect log ordering"
exit 15
