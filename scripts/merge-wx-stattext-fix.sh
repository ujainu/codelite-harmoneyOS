#!/usr/bin/env bash
# wxStaticText boot fix: compile stub template + in-place patch libwx_ohosu_core.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
source "$ROOT/toolchain/env.sh"

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
HAP_LIBS="$ROOT/host/fw2-hap/entry/libs/arm64-v8a"
STUB_CPP="$SCRIPT_DIR/stattext_stub.cpp"
STUB_O="/tmp/stattext_stub_verify.o"

echo "[stattext-fix] verify stub object compiles"
"$CXX" --target=aarch64-linux-ohos -c "$STUB_CPP" -o "$STUB_O"

echo "[ui-boot-fix] in-place patch wx core + base + libcodelite_app"
python3 "$SCRIPT_DIR/patch-wx-stattext-inplace.py"

PATCHED="$HAP_LIBS/libwx_ohosu_core-3.3-OHOS.so.4.0.0"
BASE="$HAP_LIBS/libwx_baseu-3.3-OHOS.so.4.0.0"
CL="$HAP_LIBS/libcodelite_app.so"
for f in "$HAP_LIBS"/libwx_ohosu_core-3.3-OHOS.so*; do
  [[ -f "$f" ]] || continue
  sz=$(stat -f%z "$f" 2>/dev/null || stat -c%s "$f")
  if [[ "$sz" -gt 1000000 ]]; then
    cp -f "$PATCHED" "$f" || true
  fi
done
for f in "$HAP_LIBS"/libwx_baseu-3.3-OHOS.so*; do
  [[ -f "$f" ]] || continue
  sz=$(stat -f%z "$f" 2>/dev/null || stat -c%s "$f")
  if [[ "$sz" -gt 100000 ]]; then
    cp -f "$BASE" "$f" || true
  fi
done
cd "$HAP_LIBS"
ln -sf libwx_ohosu_core-3.3-OHOS.so.4.0.0 libwx_ohosu_core-3.3-OHOS.so.4
ln -sf libwx_ohosu_core-3.3-OHOS.so.4 libwx_ohosu_core-3.3-OHOS.so

echo "[ui-boot-fix] patched wx core + base in HAP libs"
echo "[ui-boot-fix] libcodelite_app SetIcons patched in-place (frozen baseline)"
echo "[ui-boot-fix] next: repack HAP (wx*.so + libcodelite_app.so), cold install, boot probe"
