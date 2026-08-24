#!/bin/zsh
# F-8.3: Project Config probe — build.json drives compile flags + output name.
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
INSTALL_DIR="/data/storage/el2/base/haps/entry/files/share/codelite"
PROJECT_DIR="/storage/Users/currentUser/.codelite/projects/HelloWorld"
WORKDIR="$(mktemp -d /tmp/f83-config.XXXXXX)"
trap 'rm -rf "$WORKDIR"' EXIT

T="$("$HDC" list targets 2>/dev/null | head -1 | tr -d '\r\n')"
if [[ -z "$T" || "$T" == "[Empty]" ]]; then
  echo "[F-8.3] FAIL no hdc target"
  exit 1
fi
echo "[F-8.3] hdc target=$T"

if [[ ! -f "$ENTRY" ]]; then
  echo "[F-8.3] FAIL missing libentry.so — run ninja libentry.so first"
  exit 1
fi

# build.json with -std=c++17 (F-8.3 schema)
cat > "$WORKDIR/build.json" <<'EOF'
{
  "profile": "remote-ohos-aarch64",
  "compiler": "RemoteCompiler",
  "runner": "RemoteRunner",
  "mode": "release",
  "target": "arm64",
  "source": "main.cpp",
  "output": "main",
  "flags": ["-std=c++17"],
  "remoteWorkspace": "build/remote",
  "runWorkspace": "build/run"
}
EOF

cat > "$WORKDIR/main.cpp" <<'EOF'
#include <iostream>
int main() { std::cout << "Hello HarmonyCodeLite" << std::endl; return 0; }
EOF

echo "[F-8.3] PC compile: clang++ main.cpp -std=c++17 -O2 -o main"
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
"$HDC" -t "$T" file send "$WORKDIR/build.json" "$PC_STAGE/build.json"
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
echo "[F-8.3] waiting 35s for boot + project probe..."
sleep 35

STAMP=$(date +%Y%m%d-%H%M%S)
RAW="$ROOT/docs/logs/f83-raw-$STAMP.txt"
EVIDENCE="$ROOT/docs/logs/f83-gate-$STAMP.txt"
"$HDC" -t "$T" shell "hilog 2>&1 | head -12000" > "$RAW"
grep -E '\[F-8\.3\]|\[F-8\.1\]|\[F-5\.6\]|clang\+\+ main\.cpp|Hello HarmonyCodeLite' \
  "$RAW" | tee "$EVIDENCE" | tail -40

PASS_A=0 PASS_B=0 PASS_C=0 PASS_D=0
grep -q '\[F-8\.3\] loaded build.json' "$RAW" 2>/dev/null && PASS_A=1
grep -q '\[F-8\.3\] config loaded' "$RAW" 2>/dev/null && PASS_B=1
grep -q 'clang++ main.cpp.*-std=c++17.*-o main' "$RAW" 2>/dev/null && PASS_C=1
grep -q '\[F-5\.6\] compile success=1' "$RAW" 2>/dev/null \
  && grep -q 'Hello HarmonyCodeLite' "$RAW" 2>/dev/null && PASS_D=1

echo "[F-8.3-A] build.json loaded: $([[ $PASS_A -eq 1 ]] && echo PASS || echo CHECK)"
echo "[F-8.3-B] config in compile log: $([[ $PASS_B -eq 1 ]] && echo PASS || echo CHECK)"
echo "[F-8.3-C] flags in command: $([[ $PASS_C -eq 1 ]] && echo PASS || echo CHECK)"
echo "[F-8.3-D] build+run: $([[ $PASS_D -eq 1 ]] && echo PASS || echo CHECK)"
echo "[F-8.3] evidence: $EVIDENCE"

"$HDC" -t "$T" shell "cat '$PROJECT_DIR/build.json' 2>/dev/null | head -12" || true

if [[ $PASS_A -eq 1 && $PASS_B -eq 1 && $PASS_C -eq 1 && $PASS_D -eq 1 ]]; then
  echo "[F-8.3] gate PASS"
else
  echo "[F-8.3] gate CHECK (see evidence)"
  exit 1
fi
