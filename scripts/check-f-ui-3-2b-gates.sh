#!/usr/bin/env bash
# F-UI-3.2b gates: vtable dispatch at SetMenuBar virtual call.
set -euo pipefail
export LC_ALL=C
LOG="${1:?usage: check-f-ui-3-2b-gates.sh <hilog.txt>}"

# Golden libwx_ohosu_core symbol offsets (file VA, pre-ASLR)
SYM_WXFRAME_SETMENUBAR=0x6569b4
SYM_WXFRAMEBASE_SETMENUBAR=0x43fe78

has() { grep -qF "$1" "$LOG"; }

extract_hex() {
  local key="$1"
  grep -F "[FUI_VTABLE] ${key}" "$LOG" 2>/dev/null | tail -1 \
    | sed -n "s/.*\[FUI_VTABLE\] ${key}\([0-9a-fA-F][0-9a-fA-F]*\).*/0x\1/p"
}

if has '[FUI_XRC] LoadMenuBar return'; then
  echo "[f-ui-3.2b-gate] PASS LoadMenuBar return"
else
  echo "[f-ui-3.2b-gate] FAIL missing LoadMenuBar return"
fi

for key in this vtable slot; do
  if grep -qF "[FUI_VTABLE] ${key}=" "$LOG"; then
    echo "[f-ui-3.2b-gate] PASS vtable ${key}="
  else
    echo "[f-ui-3.2b-gate] FAIL missing vtable ${key}="
  fi
done

THIS=$(extract_hex 'this=' || true)
VTABLE=$(extract_hex 'vtable=' || true)
SLOT=$(extract_hex 'slot=' || true)

[[ -n "$THIS" ]] && echo "[f-ui-3.2b-gate] this=$THIS"
[[ -n "$VTABLE" ]] && echo "[f-ui-3.2b-gate] vtable=$VTABLE"
[[ -n "$SLOT" ]] && echo "[f-ui-3.2b-gate] slot=$SLOT"

if grep -qE 'SIGSEGV|cppcrash-com\.codelite' "$LOG"; then
  echo "[f-ui-3.2b-gate] FAIL crash"
  exit 1
fi

slot_suffix() {
  local v="${1#0x}"
  echo "${v: -5}"
}

if [[ -z "$SLOT" ]]; then
  if [[ -n "$THIS" ]] && [[ -n "$VTABLE" ]]; then
    echo "[f-ui-3.2b-gate] inconclusive — slot log missing"
    exit 15
  fi
  echo "[f-ui-3.2b-gate] inconclusive — vtable slice not reached"
  exit 15
fi

suf=$(slot_suffix "$SLOT")

if [[ "$SLOT" == "0x0" ]] || [[ "$suf" == "00000" ]]; then
  echo "[f-ui-3.2b-gate] Case 3 — invalid slot ($SLOT)"
  exit 30
fi

# File VAs: wxFrame::SetMenuBar=0x6569b4 wxFrameBase::SetMenuBar=0x43fe78
if [[ "$suf" == "569b4" ]] || [[ "$SLOT" == *"6569b4" ]]; then
  if has '[FUI_FRAME] SetMenuBar enter'; then
    echo "[f-ui-3.2b-gate] Case 1+ — slot=wxFrame::SetMenuBar + SetMenuBar enter logged"
    exit 0
  fi
  echo "[f-ui-3.2b-gate] Case 1 — slot=wxFrame::SetMenuBar (0x6569b4) but no [FUI_FRAME] enter (hook/thunk)"
  exit 11
fi

if [[ "$suf" == "3fe78" ]] || [[ "$suf" == "7fe78" ]] || [[ "$SLOT" == *"43fe78" ]]; then
  echo "[f-ui-3.2b-gate] Case 2 — slot=wxFrameBase::SetMenuBar (0x43fe78, not wxFrame override)"
  exit 20
fi

echo "[f-ui-3.2b-gate] Case 2b — slot=$SLOT suffix=$suf (unknown override)"
exit 21
