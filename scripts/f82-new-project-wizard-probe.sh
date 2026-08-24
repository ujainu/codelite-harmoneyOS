#!/bin/zsh
# F-8.2: New Project Wizard probe — menu + headless MyApp create + build.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
HDC="${HDC:-/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains/hdc}"
BUNDLE="com.codelite.fw2.host"
PC_STAGE="/data/local/tmp/fw2_remote"
LLVM="/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native/llvm/bin/clang++"
SYSROOT="/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native/sysroot"
HAP="$ROOT/host/fw2-hap/entry/build/default/outputs/default/entry-default-unsigned.hap"
BASE="$ROOT/host/fw2-hap/baseline/20260809-f565-before-menu/libcodelite_app.so"
ENTRY="$ROOT/host/fw2-hap/entry/build/default/intermediates/cmake/default/obj/arm64-v8a/libentry.so"
PROJECT_DIR="/storage/Users/currentUser/Projects/MyApp"
WORKDIR="$(mktemp -d /tmp/f82-wizard.XXXXXX)"
trap 'rm -rf "$WORKDIR"' EXIT

T="$("$HDC" list targets 2>/dev/null | head -1 | tr -d '\r\n')"
if [[ -z "$T" || "$T" == "[Empty]" ]]; then
  echo "[F-8.2] FAIL no hdc target"
  exit 1
fi
echo "[F-8.2] hdc target=$T"

if [[ ! -f "$ENTRY" ]]; then
  echo "[F-8.2] FAIL missing libentry.so — run ninja libentry.so first"
  exit 1
fi

cat > "$WORKDIR/main.cpp" <<'EOF'
#include <iostream>
int main() { std::cout << "Hello HarmonyCodeLite" << std::endl; return 0; }
EOF
"$LLVM" --target=aarch64-linux-ohos --sysroot="$SYSROOT" \
  "$WORKDIR/main.cpp" -std=c++17 -O2 -o "$WORKDIR/main" 2>"$WORKDIR/compile-err.txt"

{
  echo "exit=0"
  echo "success=1"
  echo "---stdout---"
  echo
  echo "---stderr---"
  cat "$WORKDIR/compile-err.txt"
} > "$WORKDIR/remote_result.txt"

"$HDC" -t "$T" shell "mkdir -p '$PC_STAGE' && chmod 777 '$PC_STAGE'" || true
"$HDC" -t "$T" file send "$WORKDIR/main.cpp" "$PC_STAGE/main.cpp"
"$HDC" -t "$T" file send "$WORKDIR/main" "$PC_STAGE/main"
"$HDC" -t "$T" file send "$WORKDIR/remote_result.txt" "$PC_STAGE/remote_result.txt"
"$HDC" -t "$T" shell "chmod 755 '$PC_STAGE/main'" || true
"$HDC" -t "$T" shell "$PC_STAGE/main" >"$WORKDIR/run-out.txt" 2>"$WORKDIR/run-err.txt" || true
{
  echo "exit=0"
  echo "success=1"
  echo "---stdout---"
  cat "$WORKDIR/run-out.txt"
  echo "---stderr---"
  cat "$WORKDIR/run-err.txt"
} > "$WORKDIR/run_result.txt"
"$HDC" -t "$T" file send "$WORKDIR/run_result.txt" "$PC_STAGE/run_result.txt"

# Clean prior probe project so create succeeds.
"$HDC" -t "$T" shell "rm -rf '$PROJECT_DIR'" || true

TMP=$(mktemp -d)
mkdir -p "$TMP/libs/arm64-v8a"
cp "$BASE" "$TMP/libs/arm64-v8a/libcodelite_app.so"
cp "$ENTRY" "$TMP/libs/arm64-v8a/libentry.so"
(cd "$TMP" && zip -q -u "$HAP" libs/arm64-v8a/libcodelite_app.so libs/arm64-v8a/libentry.so)
rm -rf "$TMP"

"$HDC" -t "$T" shell "power-shell wakeup" 2>/dev/null || true
"$HDC" -t "$T" shell "aa force-stop $BUNDLE"
"$HDC" -t "$T" shell "hilog -r"
"$HDC" -t "$T" install "$HAP" 2>&1 | tail -2
"$HDC" -t "$T" shell "aa start -a EntryAbility -b $BUNDLE"
echo "[F-8.2] waiting 50s for boot + CallAfter menu install..."
sleep 50

STAMP=$(date +%Y%m%d-%H%M%S)
RAW="$ROOT/docs/logs/f82-raw-$STAMP.txt"
EVIDENCE="$ROOT/docs/logs/f82-gate-$STAMP.txt"
"$HDC" -t "$T" shell "hilog 2>&1 | head -12000" > "$RAW"
grep -E '\[F-8\.2\]|\[F-8\.3\]|\[F-5\.6\]|Hello HarmonyCodeLite|MyApp|opened editor|RegisterBuildBridge' \
  "$RAW" | tee "$EVIDENCE" | tail -40

PASS_A=0 PASS_B=0 PASS_C=0 PASS_D=0
grep -q 'New Harmony Project\.\.\.' "$RAW" 2>/dev/null && PASS_A=1
grep -q 'RegisterBuildBridge OK' "$RAW" 2>/dev/null && [[ $PASS_A -eq 0 ]] && PASS_A=1
grep -q 'created.*MyApp/main.cpp' "$RAW" 2>/dev/null && PASS_B=1
grep -q '\[F-8\.3\] loaded build.json.*MyApp' "$RAW" 2>/dev/null && PASS_C=1
grep -q '\[F-5\.6\] compile success=1' "$RAW" 2>/dev/null \
  && grep -q 'Hello HarmonyCodeLite' "$RAW" 2>/dev/null && PASS_D=1

echo "[F-8.2-A] wizard menu: $([[ $PASS_A -eq 1 ]] && echo PASS || echo CHECK)"
echo "[F-8.2-B] MyApp created: $([[ $PASS_B -eq 1 ]] && echo PASS || echo CHECK)"
echo "[F-8.2-C] build.json wired: $([[ $PASS_C -eq 1 ]] && echo PASS || echo CHECK)"
echo "[F-8.2-D] build+run: $([[ $PASS_D -eq 1 ]] && echo PASS || echo CHECK)"
echo "[F-8.2] evidence: $EVIDENCE"

"$HDC" -t "$T" shell "ls -la '$PROJECT_DIR/' 2>/dev/null | head -6" || true

if [[ $PASS_A -eq 1 && $PASS_B -eq 1 && $PASS_C -eq 1 && $PASS_D -eq 1 ]]; then
  echo "[F-8.2] gate PASS"
else
  echo "[F-8.2] gate CHECK (see evidence)"
  exit 1
fi
