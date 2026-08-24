#!/bin/zsh
# F-5.4.1: PC-side remote compile probe — DevEco clang++ + hdc staging.
# Run BEFORE launching the app (or restart after push) so device reads remote_result.txt.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
HDC="${HDC:-/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/toolchains/hdc}"
BUNDLE="com.codelite.fw2.host"
INSTALL_DIR="/data/storage/el2/base/haps/entry/files/share/codelite"
REMOTE_DIR="$INSTALL_DIR/build/remote"
PC_STAGE="/data/local/tmp/fw2_remote"
LLVM="/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native/llvm/bin/clang++"
SYSROOT="/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native/sysroot"
WORKDIR="$(mktemp -d /tmp/f54-remote.XXXXXX)"
trap 'rm -rf "$WORKDIR"' EXIT

T="$("$HDC" list targets 2>/dev/null | head -1 | tr -d '\r\n')"
if [[ -z "$T" || "$T" == "[Empty]" ]]; then
  echo "[F-5.4] FAIL no hdc target"
  exit 1
fi
HAP="$ROOT/host/fw2-hap/entry/build/default/outputs/default/entry-default-unsigned.hap"
ENTRY="$ROOT/host/fw2-hap/entry/build/default/intermediates/cmake/default/obj/arm64-v8a/libentry.so"
if [[ -f "$ENTRY" && -f "$HAP" ]]; then
  TMP="$(mktemp -d)"
  mkdir -p "$TMP/libs/arm64-v8a"
  cp "$ENTRY" "$TMP/libs/arm64-v8a/libentry.so"
  (cd "$TMP" && zip -q -u "$HAP" libs/arm64-v8a/libentry.so)
  rm -rf "$TMP"
  "$HDC" -t "$T" install "$HAP" || true
fi

echo "[F-5.4] hdc target=$T"

cat > "$WORKDIR/hello.cpp" <<'EOF'
#include <cstdio>
int main() {
    std::puts("HarmonyCodeLite hello");
    return 0;
}
EOF

echo "[F-5.4] PC compile: clang++ hello.cpp -o hello (OHOS aarch64)"
set +e
"$LLVM" --target=aarch64-linux-ohos --sysroot="$SYSROOT" \
  "$WORKDIR/hello.cpp" -o "$WORKDIR/hello" \
  >"$WORKDIR/stdout.txt" 2>"$WORKDIR/stderr.txt"
EXIT=$?
set -e

STDOUT="$(cat "$WORKDIR/stdout.txt")"
STDERR="$(cat "$WORKDIR/stderr.txt")"

{
  echo "exit=$EXIT"
  echo "success=$(( EXIT == 0 ? 1 : 0 ))"
  echo "---stdout---"
  printf '%s' "$STDOUT"
  echo
  echo "---stderr---"
  printf '%s' "$STDERR"
} > "$WORKDIR/remote_result.txt"

"$HDC" -t "$T" shell "mkdir -p '$PC_STAGE'" || true
"$HDC" -t "$T" file send "$WORKDIR/hello.cpp" "$PC_STAGE/hello.cpp"
"$HDC" -t "$T" file send "$WORKDIR/remote_result.txt" "$PC_STAGE/remote_result.txt"
if [[ -f "$WORKDIR/hello" ]]; then
  "$HDC" -t "$T" file send "$WORKDIR/hello" "$PC_STAGE/hello"
fi

echo "[F-5.4] push hello.cpp OK (hdc → $PC_STAGE)"
echo "[F-5.4] remote_result exit=$EXIT"
echo "[F-5.4] restart app to consume BuildResult on device"

"$HDC" -t "$T" shell "aa force-stop $BUNDLE" || true
"$HDC" -t "$T" shell "hilog -r" || true
"$HDC" -t "$T" shell "aa start -a EntryAbility -b $BUNDLE"

STAMP=$(date +%Y%m%d-%H%M%S)
OUT="$ROOT/docs/logs/f54-device-hilog-$STAMP.txt"
mkdir -p "$ROOT/docs/logs"
sleep 12
"$HDC" -t "$T" shell hilog -x 2>/dev/null | grep -E '\[F-5\.4\]|FW2Host' | tee "$OUT" | tail -40 || true
echo "[F-5.4] evidence: $OUT"
