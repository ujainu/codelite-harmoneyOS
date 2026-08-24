#!/usr/bin/env bash
# F-UI-3.3e gates: wxFrame::SetMenuBar source fix (OHOS frame.cpp).
set -euo pipefail
export LC_ALL=C
LOG="${1:?usage: check-f-ui-3-3e-gates.sh <hilog.txt>}"

has() { grep -qF "$1" "$LOG"; }

if has 'MenuBar handler created' || has '[FUI_XRC] LoadMenuBar return'; then
  echo "[f-ui-3.3e-gate] PASS LoadMenuBar complete"
else
  echo "[f-ui-3.3e-gate] FAIL missing LoadMenuBar complete"
fi

if has '[FUI_FRAME] SetMenuBar menubar=' || has '[R-4] MenuBar Attach frame='; then
  echo "[f-ui-3.3e-gate] PASS SetMenuBar path (source log or R-4 Attach)"
else
  echo "[f-ui-3.3e-gate] FAIL missing SetMenuBar path confirmation"
fi

if has '[R-4] MenuBar Attach frame='; then
  echo "[f-ui-3.3e-gate] PASS wxMenuBar::Attach"
else
  echo "[f-ui-3.3e-gate] FAIL missing wxMenuBar::Attach"
fi

if grep -qE 'SIGSEGV|cppcrash-com\.codelite' "$LOG"; then
  if has '[R-4] MenuBar Create ok' \
     && grep -qF 'wxFrameBase::SetMenuBar' "$LOG" \
     && ! has '[R-4] MenuBar Attach frame='; then
    echo "[f-ui-3.3e-gate] Case C — SetMenuBar+Attach entered, crash in Attach (F-UI-3.4 next)"
    exit 12
  fi
  echo "[f-ui-3.3e-gate] FAIL crash"
  exit 1
fi

if has '[FUI_FRAME] SetMenuBar menubar=' || has '[R-4] MenuBar Attach frame='; then
  if grep -qE 'THREAD_BLOCK' "$LOG" && ! has 'aui Update after menu'; then
    echo "[f-ui-3.3e-gate] Case B — SetMenuBar+Attach OK, hang after menu (F-UI-3.4 AUI next)"
    exit 11
  fi
  echo "[f-ui-3.3e-gate] Case A — SetMenuBar path unblocked (source fix OK)"
  exit 0
fi

if has '[R-4] MenuBar Create ok' && ! has '[R-4] MenuBar Attach frame='; then
  echo "[f-ui-3.3e-gate] Case C — SetMenuBar entered, hang/crash in AttachMenuBar/Attach"
  exit 12
fi

if grep -qE 'THREAD_BLOCK' "$LOG"; then
  echo "[f-ui-3.3e-gate] Case D — still THREAD_BLOCK before SetMenuBar log"
  exit 10
fi

echo "[f-ui-3.3e-gate] inconclusive"
exit 15
