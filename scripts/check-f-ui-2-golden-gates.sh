#!/usr/bin/env bash
# Gate F-UI-2 PASS: three log lines must all appear.
# With --wait, poll $LOG (refreshed by caller) up to ${F_UI2_GATE_TIMEOUT:-45}s.
set -euo pipefail
LOG="${1:?usage: check-f-ui-2-golden-gates.sh <hilog.txt> [--wait]}"
WAIT=0
[[ "${2:-}" == "--wait" ]] && WAIT=1

G1='\[FUI_MENU\] MenuBar handler enter'
G2='\[FUI_MENU\] MenuBar handler created'
G3='\[FUI_XRC\] CreateResource return class=wxMenuBar'

check_once() {
  local pass=1
  for g in "$G1" "$G2" "$G3"; do
    if grep -qE "$g" "$LOG"; then
      echo "[f-ui-2-gate] PASS $g"
    else
      echo "[f-ui-2-gate] pending $g"
      pass=0
    fi
  done
  if grep -qE 'SIGSEGV|cppcrash-com\.codelite' "$LOG"; then
    echo "[f-ui-2-gate] FAIL crash detected"
    return 2
  fi
  [[ "$pass" -eq 1 ]]
}

if [[ "$WAIT" -eq 1 ]]; then
  deadline=$(($(date +%s) + ${F_UI2_GATE_TIMEOUT:-45}))
  while [[ $(date +%s) -lt "$deadline" ]]; do
    if [[ ! -f "$LOG" ]]; then
      sleep 2
      continue
    fi
    if check_once; then
      echo "[f-ui-2-gate] GOLDEN PASS — safe to proceed to F-UI-3.x"
      exit 0
    fi
    rc=$?
    [[ "$rc" -eq 2 ]] && exit 1
    sleep 2
  done
  echo "[f-ui-2-gate] FAIL timeout waiting for golden gates"
  exit 1
fi

if check_once; then
  echo "[f-ui-2-gate] GOLDEN PASS — safe to proceed to F-UI-3.x"
  exit 0
fi
echo "[f-ui-2-gate] GOLDEN FAIL — do NOT run F-UI-3.x; restore golden snapshot and retry"
exit 1
