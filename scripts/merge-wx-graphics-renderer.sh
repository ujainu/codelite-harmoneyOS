#!/usr/bin/env bash
# F-UI-1: build libwx_ohos_graphics.so (minimal wxGraphicsRenderer, no Cairo).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
source "$ROOT/toolchain/env.sh"

WXLIB="$ROOT/build-wx-ohos-gui/lib"
HAP_LIBS="$ROOT/host/fw2-hap/entry/libs/arm64-v8a"
GRAPHICS_O="$ROOT/build-wx-ohos-gui/libs/core/CMakeFiles/wxcore.dir/__/__/__/__/src/ohos/graphics.cpp.o"
TEXTEXT_O="$ROOT/build-wx-ohos-gui/libs/core/CMakeFiles/wxcore.dir/__/__/__/__/src/ohos/textextent.cpp.o"
EXPORTS="$ROOT/host/fw2-hap/scripts/graphics.exports"
OUT="$HAP_LIBS/libwx_ohos_graphics.so"
SYSROOT="/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native/sysroot"

if [[ ! -f "$GRAPHICS_O" ]] || [[ "$ROOT/third_party/wxWidgets/src/ohos/graphics.cpp" -nt "$GRAPHICS_O" ]]; then
  echo "[graphics-renderer] building graphics.cpp.o"
  ninja -C "$ROOT/build-wx-ohos-gui" \
    "libs/core/CMakeFiles/wxcore.dir/__/__/__/__/src/ohos/graphics.cpp.o"
fi

if [[ ! -f "$TEXTEXT_O" ]]; then
  echo "[graphics-renderer] building textextent.cpp.o"
  ninja -C "$ROOT/build-wx-ohos-gui" \
    "libs/core/CMakeFiles/wxcore.dir/__/__/__/__/src/ohos/textextent.cpp.o"
fi

"$CXX" --target=aarch64-linux-ohos --sysroot="$SYSROOT" \
  -shared -fPIC \
  -Wl,-soname,libwx_ohos_graphics.so \
  -Wl,--version-script="$EXPORTS" \
  -o "$OUT" "$GRAPHICS_O" "$TEXTEXT_O" \
  -L"$HAP_LIBS" -lwx_ohosu_core-3.3-OHOS \
  -lc++_shared

echo "[graphics-renderer] built $OUT"
llvm-nm -D "$OUT" 2>/dev/null | rg "wxOhosGetGraphicsRenderer" || true
