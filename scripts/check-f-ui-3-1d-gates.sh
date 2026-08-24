#!/usr/bin/env bash
# F-UI-3.1d flow gates: wxString cleanup slice 0x5da484–0x5da4a0.
set -euo pipefail
LOG="${1:?usage: check-f-ui-3-1d-gates.sh <hilog.txt>}"

has() { grep -qF "$1" "$LOG"; }

if has '[FUI_XRC] LoadMenuBar return'; then
  echo "[f-ui-3.1d-gate] PASS LoadMenuBar return"
else
  echo "[f-ui-3.1d-gate] FAIL missing LoadMenuBar return"
fi

for probe in \
  'after LoadMenuBar cleanup begin' \
  'after m_mainMenuBar store' \
  'before wxString dtor' \
  'cleanup finished'; do
  if has "[FUI_FRAME_FLOW] $probe"; then
    echo "[f-ui-3.1d-gate] PASS $probe"
  else
    echo "[f-ui-3.1d-gate] FAIL missing $probe"
  fi
done

if grep -qE 'SIGSEGV|cppcrash-com\.codelite' "$LOG"; then
  echo "[f-ui-3.1d-gate] FAIL crash (probe may have corrupted stack frame)"
  exit 1
fi

# Case A: reached before dtor, hang inside ~wxString
if has 'after m_mainMenuBar store' \
   && has 'before wxString dtor' \
   && ! has 'cleanup finished'; then
  echo "[f-ui-3.1d-gate] Case A — hang inside wxString::~wxString (0x5da490 bl)"
  exit 10
fi

# Case B: store ok, never reached dtor probe
if has 'after m_mainMenuBar store' \
   && ! has 'before wxString dtor'; then
  echo "[f-ui-3.1d-gate] Case B — hang between store and dtor (0x5da488–0x5da48c)"
  exit 11
fi

# Case B2: cleanup begin only
if has 'after LoadMenuBar cleanup begin' \
   && ! has 'after m_mainMenuBar store'; then
  echo "[f-ui-3.1d-gate] Case B2 — hang at ldr/store (0x5da484–0x5da488)"
  exit 12
fi

# Case C: cleanup window done, clConfig not reached (3.1c territory)
if has 'cleanup finished' \
   && ! has '[FUI_FRAME_FLOW] before clConfig'; then
  echo "[f-ui-3.1d-gate] Case C — cleanup done; blocker after 0x5da4a0 (clConfig path)"
  exit 13
fi

if has 'cleanup finished'; then
  echo "[f-ui-3.1d-gate] PASS cleanup window complete"
  exit 0
fi

if has '[FUI_XRC] LoadMenuBar return' \
   && ! has 'after LoadMenuBar cleanup begin'; then
  echo "[f-ui-3.1d-gate] Case 0 — hang before cleanup slice (LoadMenuBar return path)"
  exit 14
fi

echo "[f-ui-3.1d-gate] inconclusive — inspect log ordering"
exit 15
