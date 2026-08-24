#!/bin/zsh
# F-5.6.5: Device gate — boot + menu + Build Remote (after libcodelite relink).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
HDC="${HDC:-/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains/hdc}"
BUNDLE="com.codelite.fw2.host"
HAP="$ROOT/host/fw2-hap/entry/build/default/outputs/default/entry-default-unsigned.hap"
CL_SO="$ROOT/host/fw2-hap/entry/libs/arm64-v8a/libcodelite_app.so"
ENTRY="$ROOT/host/fw2-hap/entry/build/default/intermediates/cmake/default/obj/arm64-v8a/libentry.so"

T="$("$HDC" list targets 2>/dev/null | head -1 | tr -d '\r\n')"
if [[ -z "$T" || "$T" == "[Empty]" ]]; then
  echo "[F-5.6.5] FAIL no hdc target"
  exit 1
fi

"$HDC" -t "$T" shell "power-shell wakeup" 2>/dev/null || true

# Repack HAP with relinked libcodelite + libentry.
TMP="$(mktemp -d)"
mkdir -p "$TMP/libs/arm64-v8a"
cp "$CL_SO" "$TMP/libs/arm64-v8a/libcodelite_app.so"
if [[ -f "$ENTRY" ]]; then
  cp "$ENTRY" "$TMP/libs/arm64-v8a/libentry.so"
fi
(cd "$TMP" && zip -q -u "$HAP" libs/arm64-v8a/libcodelite_app.so libs/arm64-v8a/libentry.so 2>/dev/null || \
  zip -q -u "$HAP" libs/arm64-v8a/libcodelite_app.so)
rm -rf "$TMP"

# Prepare remote compile/run artifacts (F-5.6 bridge).
"$ROOT/host/fw2-hap/scripts/f56-build-probe.sh" 2>&1 | grep -E '^\[F-5\.(6|6\.5)\]|gate|install|start' | tail -15 || true

STAMP=$(date +%Y%m%d-%H%M%S)
GATE="$ROOT/docs/logs/f565-gate-$STAMP.txt"
sleep 3
"$HDC" -t "$T" shell "hilog 2>&1 | head -6000" \
  | grep -E '\[B-6\]|BOOT|alive|\[F-5\.6\.5\]|\[F-5\.6\]|\[MV-3\]|\[B-7\]|MV-1' \
  | tee "$GATE" | tail -60 || true
echo "[F-5.6.5] evidence: $GATE"

PASS_A=0 PASS_B=0 PASS_C=0
grep -qE 'BOOT|alive|\[B-7\]' "$GATE" 2>/dev/null && PASS_A=1
grep -qE 'Harmony menu (created|installed)' "$GATE" 2>/dev/null && PASS_B=1
grep -qE '59001|Build Remote id' "$GATE" 2>/dev/null && PASS_B=1
grep -q '\[F-5.6.5\] command=BUILD_REMOTE' "$GATE" 2>/dev/null \
  && grep -q '\[F-5.6\] compile success=1' "$GATE" 2>/dev/null \
  && grep -q 'Hello HarmonyCodeLite' "$GATE" 2>/dev/null && PASS_C=1

echo "[F-5.6.5-A] boot/MV/B-7: $([[ $PASS_A -eq 1 ]] && echo PASS || echo CHECK)"
echo "[F-5.6.5-B] menu exists: $([[ $PASS_B -eq 1 ]] && echo PASS || echo CHECK)"
echo "[F-5.6.5-C] build remote: $([[ $PASS_C -eq 1 ]] && echo PASS || echo CHECK)"

if [[ $PASS_A -eq 1 && $PASS_B -eq 1 && $PASS_C -eq 1 ]]; then
  echo "[F-5.6.5] gate PASS"
else
  echo "[F-5.6.5] gate CHECK (see evidence; menu click F-5.6.5-C may need manual Harmony→Build Remote)"
fi
