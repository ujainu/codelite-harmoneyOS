#!/usr/bin/env bash
# wxToolTip boot fix: build stub (optional) + in-place patch libwx_ohosu_core in HAP libs.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
source "$ROOT/toolchain/env.sh"

WXLIB="$ROOT/build-wx-ohos-gui/lib"
HAP_LIBS="$ROOT/host/fw2-hap/entry/libs/arm64-v8a"
TOOLTIP="$ROOT/build-wx-ohos-gui/libs/core/CMakeFiles/wxcore.dir/__/__/__/__/src/ohos/tooltip.cpp.o"
STUB="$WXLIB/libwx_ohosu_tooltip_stub.so"
SYSROOT="/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native/sysroot"

# 1) Ensure tooltip.cpp.o exists (for future full wxcore relink)
if [[ ! -f "$TOOLTIP" ]]; then
  echo "[tooltip-fix] building tooltip.cpp.o"
  ninja -C "$ROOT/build-wx-ohos-gui" \
    "libs/core/CMakeFiles/wxcore.dir/__/__/__/__/src/ohos/tooltip.cpp.o"
fi

# 2) Optional standalone stub (libentry dlopen — superseded by in-place patch)
"$CXX" --target=aarch64-linux-ohos --sysroot="$SYSROOT" \
  -shared -Wl,-soname,libwx_ohosu_tooltip_stub.so \
  -o "$STUB" "$TOOLTIP" \
  -L"$WXLIB" -lwx_baseu-3.3-OHOS -lc++_shared
cp "$STUB" "$HAP_LIBS/"

# 3) In-place NOP patch on all full-size libwx_ohosu_core copies (fixes broken ctor in .so.4)
python3 "$ROOT/host/fw2-hap/scripts/patch-wx-tooltip-inplace.py"
PATCHED="$HAP_LIBS/libwx_ohosu_core-3.3-OHOS.so.4.0.0"
for f in "$HAP_LIBS"/libwx_ohosu_core-3.3-OHOS.so*; do
  [[ -f "$f" ]] || continue
  sz=$(stat -f%z "$f" 2>/dev/null || stat -c%s "$f")
  if [[ "$sz" -gt 1000000 ]]; then
    cp "$PATCHED" "$f"
  fi
done
cd "$HAP_LIBS"
ln -sf libwx_ohosu_core-3.3-OHOS.so.4.0.0 libwx_ohosu_core-3.3-OHOS.so.4
ln -sf libwx_ohosu_core-3.3-OHOS.so.4 libwx_ohosu_core-3.3-OHOS.so

echo "[tooltip-fix] patched wx core + staged stub"
echo "[tooltip-fix] rebuild libentry, then repack HAP updating:"
echo "  libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so{,.4,.4.0.0} libentry.so"
