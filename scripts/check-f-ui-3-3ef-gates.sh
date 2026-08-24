#!/usr/bin/env bash
# F-UI-3.3ef gates: SetMenuBar + MenuBar Attach boot path.
set -euo pipefail
export LC_ALL=C
LOG="${1:?usage: check-f-ui-3-3ef-gates.sh <hilog.txt>}"

has() { grep -qF "$1" "$LOG"; }

if has 'MenuBar handler created' || has '[FUI_XRC] LoadMenuBar return'; then
  echo "[f-ui-3.3ef-gate] PASS LoadMenuBar complete"
else
  echo "[f-ui-3.3ef-gate] FAIL missing LoadMenuBar complete"
fi

if has '[R-4] MenuBar Attach frame='; then
  echo "[f-ui-3.3ef-gate] PASS wxMenuBar::Attach"
else
  echo "[f-ui-3.3ef-gate] FAIL missing wxMenuBar::Attach"
fi

if grep -qE 'SIGSEGV|cppcrash-com\.codelite' "$LOG"; then
  if has '[R-4] MenuBar Create ok' && ! has '[R-4] MenuBar Attach frame='; then
    echo "[f-ui-3.3ef-gate] Case C — crash in Attach after Create (need more 3.4 fixes)"
    exit 12
  fi
  echo "[f-ui-3.3ef-gate] FAIL crash"
  exit 1
fi

if has '[R-4] MenuBar Attach frame='; then
  if grep -qF '[WS-4] OK FileViewTree Create' "$LOG"; then
    echo "[f-ui-3.3ef-gate] Case A — past SideBar/Workspace tree"
    exit 0
  fi
  if grep -qE 'THREAD_BLOCK' "$LOG" && ! grep -qF 'MainBook' "$LOG" && ! grep -qF 'CreateRecentlyOpenedWorkspacesMenu' "$LOG"; then
    # Ignore stale THREAD_BLOCK from a prior run in the same hilog buffer
    last_attach_line=$(grep -nF '[R-4] MenuBar Attach frame=' "$LOG" | tail -1 | cut -d: -f1 || true)
    last_block_line=$(grep -nE 'THREAD_BLOCK' "$LOG" | tail -1 | cut -d: -f1 || true)
    if [[ -n "$last_attach_line" && -n "$last_block_line" && "$last_block_line" -lt "$last_attach_line" ]]; then
      echo "[f-ui-3.3ef-gate] Case A — MenuBar Attach OK (stale THREAD_BLOCK ignored)"
      exit 0
    fi
    echo "[f-ui-3.3ef-gate] Case B — Attach OK, hang after menu (statusbar/AUI next)"
    exit 11
  fi
  if grep -qF 'aui Update' "$LOG" || grep -qF 'CreateGUIControls' "$LOG"; then
    echo "[f-ui-3.3ef-gate] Case A — menu path unblocked, boot progressed"
    exit 0
  fi
  echo "[f-ui-3.3ef-gate] Case A — MenuBar Attach OK"
  exit 0
fi

if grep -qE 'THREAD_BLOCK' "$LOG"; then
  echo "[f-ui-3.3ef-gate] Case D — THREAD_BLOCK before Attach"
  exit 10
fi

echo "[f-ui-3.3ef-gate] inconclusive"
exit 15
