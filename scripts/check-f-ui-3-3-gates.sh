#!/usr/bin/env bash
# F-UI-3.3 gates: wxFrameBase::SetMenuBar internal path.
set -euo pipefail
export LC_ALL=C
LOG="${1:?usage: check-f-ui-3-3-gates.sh <hilog.txt>}"

has() { grep -qF "$1" "$LOG"; }

if has '[FUI_XRC] LoadMenuBar return'; then
  echo "[f-ui-3.3-gate] PASS LoadMenuBar return"
else
  echo "[f-ui-3.3-gate] FAIL missing LoadMenuBar return"
fi

for probe in \
  'SetMenuBar enter' \
  'post DetachMenuBar' \
  'AttachMenuBar Attach blr' \
  'AttachMenuBar return'; do
  if has "$probe"; then
    echo "[f-ui-3.3-gate] PASS $probe"
  else
    echo "[f-ui-3.3-gate] FAIL missing $probe"
  fi
done

if grep -qE 'SIGSEGV|cppcrash-com\.codelite' "$LOG"; then
  echo "[f-ui-3.3-gate] FAIL crash"
  exit 1
fi

if has '[FUI_FRAMEBASE] AttachMenuBar return' && has '[FUI_MENUBAR] Attach enter'; then
  if grep -qE 'THREAD_BLOCK' "$LOG" && ! has '[FUI_FRAMEBASE] AttachMenuBar return'; then
    :
  fi
fi

# Case D: full framebase path complete
if has '[FUI_FRAMEBASE] AttachMenuBar return'; then
  echo "[f-ui-3.3-gate] Case D — wxFrameBase SetMenuBar path complete (F-UI-3.4 post-menu next)"
  exit 0
fi

# Case C: hung inside wxMenuBar::Attach (after AttachMenuBar blr)
if has '[FUI_FRAMEBASE] AttachMenuBar Attach blr' && ! has '[FUI_FRAMEBASE] AttachMenuBar return'; then
  echo "[f-ui-3.3-gate] Case C — hang inside wxMenuBar::Attach (OHOS menu bridge)"
  exit 13
fi

# Case B: hung in AttachMenuBar before MenuBar Attach blr
if has '[FUI_FRAMEBASE] post DetachMenuBar' \
   && ! has '[FUI_FRAMEBASE] AttachMenuBar Attach blr'; then
  echo "[f-ui-3.3-gate] Case B — hang inside AttachMenuBar (pre wxMenuBar::Attach)"
  exit 12
fi

# Case A2: hung in DetachMenuBar
if has '[FUI_FRAMEBASE] SetMenuBar enter' \
   && ! has '[FUI_FRAMEBASE] post DetachMenuBar'; then
  if grep -qE 'THREAD_BLOCK' "$LOG"; then
    echo "[f-ui-3.3-gate] Case A — hang inside DetachMenuBar/GetMenuBar + THREAD_BLOCK"
  else
    echo "[f-ui-3.3-gate] Case A — hang before post-DetachMenuBar"
  fi
  exit 10
fi

# Case A3: between Detach and Attach
if has '[FUI_FRAMEBASE] post DetachMenuBar' \
   && ! has '[FUI_FRAMEBASE] AttachMenuBar enter'; then
  echo "[f-ui-3.3-gate] Case B2 — hang between DetachMenuBar and AttachMenuBar tail-call"
  exit 11
fi

if has '[FUI_VTABLE] slot='; then
  echo "[f-ui-3.3-gate] note: vtable slice present in same log"
fi

echo "[f-ui-3.3-gate] inconclusive — inspect log ordering"
exit 15
