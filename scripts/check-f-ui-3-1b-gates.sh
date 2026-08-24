#!/usr/bin/env bash
# F-UI-3.1b gates: slice LoadMenuBar return → SetMenuBar enter.
set -euo pipefail
LOG="${1:?usage: check-f-ui-3-1b-gates.sh <hilog.txt>}"

G1='\[FUI_XRC\] LoadMenuBar return obj='
G2='\[FUI_FRAME\] SetMenuBar enter'

for g in "$G1" "$G2"; do
  if grep -qE "$g" "$LOG"; then
    echo "[f-ui-3.1b-gate] PASS $g"
  else
    echo "[f-ui-3.1b-gate] FAIL missing $g"
  fi
done

if grep -qE 'SIGSEGV|cppcrash-com\.codelite' "$LOG"; then
  echo "[f-ui-3.1b-gate] FAIL crash detected"
  exit 1
fi

if grep -qE "$G1" "$LOG" && grep -qE "$G2" "$LOG"; then
  echo "[f-ui-3.1b-gate] SLICE PASS — safe for F-UI-3.2 (SetMenuBar attach confirmed reachable)"
  exit 0
fi

if grep -qE "$G1" "$LOG"; then
  echo "[f-ui-3.1b-gate] SLICE PARTIAL — LoadMenuBar returned; hang before SetMenuBar (caller gap)"
  exit 2
fi

echo "[f-ui-3.1b-gate] SLICE FAIL — no LoadMenuBar return (XRC return path or earlier hang)"
exit 1
