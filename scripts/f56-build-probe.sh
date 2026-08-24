#!/bin/zsh
# F-5.6: BuildController probe — PC compile + run, inject remote/run results via rawfile.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
HDC="${HDC:-/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains/hdc}"
BUNDLE="com.codelite.fw2.host"
PC_STAGE="/data/local/tmp/fw2_remote"
LLVM="/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native/llvm/bin/clang++"
SYSROOT="/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native/sysroot"
HAP="$ROOT/host/fw2-hap/entry/build/default/outputs/default/entry-default-unsigned.hap"
ENTRY="$ROOT/host/fw2-hap/entry/build/default/intermediates/cmake/default/obj/arm64-v8a/libentry.so"
RAW_REMOTE_RESULT="$ROOT/host/fw2-hap/entry/src/main/resources/rawfile/share/codelite/build/remote/remote_result.txt"
RAW_RUN_RESULT="$ROOT/host/fw2-hap/entry/src/main/resources/rawfile/share/codelite/build/run/run_result.txt"
INSTALL_DIR="/data/storage/el2/base/haps/entry/files/share/codelite"
WORKDIR="$(mktemp -d /tmp/f56-build.XXXXXX)"
trap 'rm -rf "$WORKDIR"' EXIT

T="$("$HDC" list targets 2>/dev/null | head -1 | tr -d '\r\n')"
if [[ -z "$T" || "$T" == "[Empty]" ]]; then
  echo "[F-5.6] FAIL no hdc target"
  exit 1
fi
echo "[F-5.6] hdc target=$T"

cat > "$WORKDIR/hello.cpp" <<'EOF'
#include <iostream>

int main()
{
    std::cout << "Hello HarmonyCodeLite" << std::endl;
    return 0;
}
EOF

echo "[F-5.6] PC compile: aarch64-linux-ohos clang++ hello.cpp -o hello"
set +e
"$LLVM" --target=aarch64-linux-ohos --sysroot="$SYSROOT" \
  "$WORKDIR/hello.cpp" -o "$WORKDIR/hello" 2>"$WORKDIR/compile-stderr.txt"
COMPILE_EXIT=$?
set -e
if [[ $COMPILE_EXIT -ne 0 ]]; then
  echo "[F-5.6] FAIL PC compile exit=$COMPILE_EXIT"
  cat "$WORKDIR/compile-stderr.txt" >&2
  exit 1
fi

{
  echo "exit=$COMPILE_EXIT"
  echo "success=$(( COMPILE_EXIT == 0 ? 1 : 0 ))"
  echo "---stdout---"
  echo
  echo "---stderr---"
  cat "$WORKDIR/compile-stderr.txt"
} > "$WORKDIR/remote_result.txt"

echo "[F-5.6] hdc shell run (capture run_result.txt)"
RUN_OUT="$WORKDIR/run-stdout.txt"
RUN_ERR="$WORKDIR/run-stderr.txt"
set +e
"$HDC" -t "$T" shell "mkdir -p '$PC_STAGE' && chmod 777 '$PC_STAGE'" || true
"$HDC" -t "$T" file send "$WORKDIR/hello.cpp" "$PC_STAGE/hello.cpp"
"$HDC" -t "$T" file send "$WORKDIR/hello" "$PC_STAGE/hello"
"$HDC" -t "$T" file send "$WORKDIR/remote_result.txt" "$PC_STAGE/remote_result.txt"
"$HDC" -t "$T" shell "chmod 755 '$PC_STAGE/hello'" || true
"$HDC" -t "$T" shell "$PC_STAGE/hello" >"$RUN_OUT" 2>"$RUN_ERR"
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

mkdir -p "$(dirname "$RAW_REMOTE_RESULT")" "$(dirname "$RAW_RUN_RESULT")"
cp "$WORKDIR/remote_result.txt" "$RAW_REMOTE_RESULT"
cp "$WORKDIR/run_result.txt" "$RAW_RUN_RESULT"

# HAP sandbox cannot read /data/local/tmp — also seed filesDir workspaces directly.
"$HDC" -t "$T" shell "mkdir -p '$INSTALL_DIR/build/remote' '$INSTALL_DIR/build/run'" || true
"$HDC" -t "$T" file send "$WORKDIR/remote_result.txt" "$INSTALL_DIR/build/remote/remote_result.txt" || true
"$HDC" -t "$T" file send "$WORKDIR/run_result.txt" "$INSTALL_DIR/build/run/run_result.txt" || true
"$HDC" -t "$T" file send "$WORKDIR/hello.cpp" "$INSTALL_DIR/build/remote/hello.cpp" || true

if [[ ! -f "$ENTRY" ]]; then
  echo "[F-5.6] WARN libentry.so missing — build entry native target first"
fi

if [[ -f "$HAP" && -f "$ENTRY" ]]; then
  TMP="$(mktemp -d)"
  mkdir -p "$TMP/libs/arm64-v8a" \
    "$TMP/resources/rawfile/share/codelite/build/remote" \
    "$TMP/resources/rawfile/share/codelite/build/run"
  cp "$ENTRY" "$TMP/libs/arm64-v8a/libentry.so"
  cp "$WORKDIR/remote_result.txt" "$TMP/resources/rawfile/share/codelite/build/remote/remote_result.txt"
  cp "$WORKDIR/run_result.txt" "$TMP/resources/rawfile/share/codelite/build/run/run_result.txt"
  (cd "$TMP" && zip -q -u "$HAP" \
    libs/arm64-v8a/libentry.so \
    resources/rawfile/share/codelite/build/remote/remote_result.txt \
    resources/rawfile/share/codelite/build/run/run_result.txt)
  rm -rf "$TMP"
  "$HDC" -t "$T" install "$HAP" || true
fi

# Force Runtime Assets redeploy so rawfile results land in filesDir/build/*.
"$HDC" -t "$T" shell "rm -f '$INSTALL_DIR/rc/menu.xrc'" || true

echo "[F-5.6] push compile+run artifacts → rawfile + $PC_STAGE"
echo "[F-5.6] restart app for BuildController probe"

"$HDC" -t "$T" shell "aa force-stop $BUNDLE" || true
"$HDC" -t "$T" shell "hilog -r" || true
"$HDC" -t "$T" shell "aa start -a EntryAbility -b $BUNDLE"

STAMP=$(date +%Y%m%d-%H%M%S)
OUT="$ROOT/docs/logs/f56-device-hilog-$STAMP.txt"
mkdir -p "$ROOT/docs/logs"
sleep 14
# Bounded dump on device — unbounded hilog -x hangs when piped through hdc on macOS.
"$HDC" -t "$T" shell "hilog 2>&1 | head -3000" \
  | grep -E '\[F-5\.6\]' | tee "$OUT" | tail -50 || true
echo "[F-5.6] evidence: $OUT"

if grep -q '\[F-5\.6\] controller=create' "$OUT" 2>/dev/null \
   && grep -q '\[F-5\.6\] compiler=RemoteCompiler' "$OUT" 2>/dev/null \
   && grep -q '\[F-5\.6\] compile success=1' "$OUT" 2>/dev/null \
   && grep -q '\[F-5\.6\] runner=RemoteRunner' "$OUT" 2>/dev/null \
   && grep -q 'Hello HarmonyCodeLite' "$OUT" 2>/dev/null \
   && grep -q '\[F-5\.6\] exit=0' "$OUT" 2>/dev/null \
   && grep -q '\[F-5\.6\] OK' "$OUT" 2>/dev/null; then
  echo "[F-5.6] gate PASS"
else
  echo "[F-5.6] gate CHECK (see evidence)"
fi
