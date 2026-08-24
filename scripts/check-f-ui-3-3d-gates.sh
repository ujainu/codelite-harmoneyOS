#!/usr/bin/env bash
# F-UI-3.3d gates: DetachMenuBar / AttachMenuBar / wxMenuBar::Attach (no SetMenuBar body).
set -euo pipefail
export LC_ALL=C
LOG="${1:?usage: check-f-ui-3-3d-gates.sh <hilog.txt>}"

has() { grep -qF "$1" "$LOG"; }

if has 'MenuBar handler created' || has '[FUI_XRC] LoadMenuBar return'; then
  echo "[f-ui-3.3d-gate] PASS LoadMenuBar complete"
else
  echo "[f-ui-3.3d-gate] FAIL missing LoadMenuBar complete"
fi

for probe in \
  'DetachMenuBar enter' \
  'DetachMenuBar pre virtual blr' \
  'post DetachMenuBar' \
  'AttachMenuBar enter' \
  'AttachMenuBar Attach blr' \
  'AttachMenuBar return' \
  'Attach enter'; do
  if has "$probe"; then
    echo "[f-ui-3.3d-gate] PASS $probe"
  else
    echo "[f-ui-3.3d-gate] FAIL missing $probe"
  fi
done

if grep -qE 'SIGSEGV|cppcrash-com\.codelite' "$LOG"; then
  echo "[f-ui-3.3d-gate] FAIL crash"
  exit 1
fi

if has '[FUI_FRAMEBASE] AttachMenuBar return' && has '[FUI_MENUBAR] Attach enter'; then
  echo "[f-ui-3.3d-gate] Case F — AttachMenuBar path complete (F-UI-3.4 post-menu next)"
  exit 0
fi

if has '[FUI_FRAMEBASE] AttachMenuBar Attach blr' \
   && ! has '[FUI_FRAMEBASE] AttachMenuBar return'; then
  echo "[f-ui-3.3d-gate] Case E — hang inside wxMenuBar::Attach (OHOS menu bridge)"
  exit 15
fi

if has '[FUI_FRAMEBASE] post DetachMenuBar' \
   && ! has '[FUI_FRAMEBASE] AttachMenuBar Attach blr'; then
  echo "[f-ui-3.3d-gate] Case D — hang inside AttachMenuBar (pre wxMenuBar::Attach)"
  exit 14
fi

if has '[FUI_FRAMEBASE] DetachMenuBar enter' \
   && ! has '[FUI_FRAMEBASE] post DetachMenuBar'; then
  if has 'DetachMenuBar pre virtual blr'; then
    echo "[f-ui-3.3d-gate] Case C — hang inside DetachMenuBar after pre-blr ldr"
  else
    echo "[f-ui-3.3d-gate] Case B — hang inside DetachMenuBar (early, pre virtual blr)"
  fi
  exit 12
fi

if ! has '[FUI_FRAMEBASE] DetachMenuBar enter'; then
  if grep -qE 'THREAD_BLOCK' "$LOG"; then
    echo "[f-ui-3.3d-gate] Case A — hang in wxFrameBase::SetMenuBar before DetachMenuBar enter + THREAD_BLOCK"
  else
    echo "[f-ui-3.3d-gate] Case A — hang in SetMenuBar before DetachMenuBar enter (matches F-UI-3.3c-3)"
  fi
  exit 10
fi

echo "[f-ui-3.3d-gate] inconclusive — inspect log ordering"
exit 16
