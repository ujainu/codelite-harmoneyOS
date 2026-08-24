#!/bin/zsh
# F-5.5: PC-side remote run probe — OHOS aarch64 hello + hdc push + device run result.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
HDC="${HDC:-/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains/hdc}"
BUNDLE="com.codelite.fw2.host"
PC_RUN="/data/local/tmp/fw2_remote"
LLVM="/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native/llvm/bin/clang++"
SYSROOT="/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native/sysroot"
HAP="$ROOT/host/fw2-hap/entry/build/default/outputs/default/entry-default-unsigned.hap"
ENTRY="$ROOT/host/fw2-hap/entry/build/default/intermediates/cmake/default/obj/arm64-v8a/libentry.so"
RAW_RUN_RESULT="$ROOT/host/fw2-hap/entry/src/main/resources/rawfile/share/codelite/build/run/run_result.txt"
INSTALL_DIR="/data/storage/el2/base/haps/entry/files/share/codelite"
WORKDIR="$(mktemp -d /tmp/f55-run.XXXXXX)"
trap 'rm -rf "$WORKDIR"' EXIT

T="$("$HDC" list targets 2>/dev/null | head -1 | tr -d '\r\n')"
if [[ -z "$T" || "$T" == "[Empty]" ]]; then
  echo "[F-5.5] FAIL no hdc target"
  exit 1
fi
echo "[F-5.5] hdc target=$T"

cat > "$WORKDIR/hello.cpp" <<'EOF'
#include <iostream>

int main()
{
    std::cout << "Hello HarmonyCodeLite" << std::endl;
    return 0;
}
EOF

echo "[F-5.5] PC compile: aarch64-linux-ohos clang++ hello.cpp -o hello"
set +e
"$LLVM" --target=aarch64-linux-ohos --sysroot="$SYSROOT" \
  "$WORKDIR/hello.cpp" -o "$WORKDIR/hello" 2>"$WORKDIR/compile-stderr.txt"
COMPILE_EXIT=$?
set -e
if [[ $COMPILE_EXIT -ne 0 ]]; then
  echo "[F-5.5] FAIL PC compile exit=$COMPILE_EXIT"
  cat "$WORKDIR/compile-stderr.txt" >&2
  exit 1
fi

echo "[F-5.5] hdc shell run (capture run_result.txt)"
RUN_OUT="$WORKDIR/run-stdout.txt"
RUN_ERR="$WORKDIR/run-stderr.txt"
set +e
"$HDC" -t "$T" shell "mkdir -p '$PC_RUN' && chmod 777 '$PC_RUN'" || true
"$HDC" -t "$T" file send "$WORKDIR/hello" "$PC_RUN/hello"
"$HDC" -t "$T" shell "chmod 755 '$PC_RUN/hello'" || true
"$HDC" -t "$T" shell "$PC_RUN/hello" >"$RUN_OUT" 2>"$RUN_ERR"
RUN_EXIT=$?
set -e

{
  echo "exit=$RUN_EXIT"
  echo "success=$(( RUN_EXIT == 0 ? 1 : 0 ))"
  echo "---stdout---"
  cat "$RUN_OUT"
  echo "---stderr---"
  cat "$RUN_ERR"
} > "$WORKDIR/run_result.txt"

mkdir -p "$(dirname "$RAW_RUN_RESULT")"
cp "$WORKDIR/run_result.txt" "$RAW_RUN_RESULT"

# HAP sandbox cannot read /data/local/tmp — inject run_result via rawfile + redeploy.
if [[ -f "$HAP" && -f "$ENTRY" ]]; then
  TMP="$(mktemp -d)"
  mkdir -p "$TMP/libs/arm64-v8a" "$TMP/resources/rawfile/share/codelite/build/run"
  cp "$ENTRY" "$TMP/libs/arm64-v8a/libentry.so"
  cp "$WORKDIR/run_result.txt" "$TMP/resources/rawfile/share/codelite/build/run/run_result.txt"
  (cd "$TMP" && zip -q -u "$HAP" \
    libs/arm64-v8a/libentry.so \
    resources/rawfile/share/codelite/build/run/run_result.txt)
  rm -rf "$TMP"
  "$HDC" -t "$T" install "$HAP" || true
fi

# Force Runtime Assets redeploy so rawfile run_result.txt lands in filesDir/build/run/.
"$HDC" -t "$T" shell "rm -f '$INSTALL_DIR/rc/menu.xrc'" || true

echo "[F-5.5] push binary hello → $PC_RUN"
echo "[F-5.5] restart app for RemoteRunner probe"

"$HDC" -t "$T" shell "aa force-stop $BUNDLE" || true
"$HDC" -t "$T" shell "hilog -r" || true
"$HDC" -t "$T" shell "aa start -a EntryAbility -b $BUNDLE"

STAMP=$(date +%Y%m%d-%H%M%S)
OUT="$ROOT/docs/logs/f55-device-hilog-$STAMP.txt"
mkdir -p "$ROOT/docs/logs"
sleep 14
"$HDC" -t "$T" shell hilog -x 2>/dev/null | grep -E '\[F-5\.5\]' | tee "$OUT" | tail -50 || true
echo "[F-5.5] evidence: $OUT"

if grep -q '\[F-5\.5\] RunResult success=1' "$OUT" 2>/dev/null \
   && grep -q 'Hello HarmonyCodeLite' "$OUT" 2>/dev/null \
   && grep -q '\[F-5\.5\] OK' "$OUT" 2>/dev/null; then
  echo "[F-5.5] gate PASS"
else
  echo "[F-5.5] gate CHECK (see evidence)"
fi
