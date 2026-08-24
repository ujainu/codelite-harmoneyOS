#!/usr/bin/env bash
# F-UI-2: rebuild libwx_ohosu_xrc with [FUI_XRC]/[FUI_MENU] diagnostics
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
source "$ROOT/toolchain/env.sh"

BUILD="$ROOT/build-wx-ohos-gui"
HAP_LIBS="$ROOT/host/fw2-hap/entry/libs/arm64-v8a"
SYSROOT="/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native/sysroot"
OUT="$HAP_LIBS/libwx_ohosu_xrc-3.3-OHOS.so.4.0.0"

rebuild_o() {
  local target="$1"
  local src="$2"
  local obj="$BUILD/$target"
  if [[ ! -f "$obj" ]] || [[ "$src" -nt "$obj" ]]; then
    echo "[f-ui-2] building $(basename "$obj")"
    rm -f "$obj"
    ninja -C "$BUILD" -j1 "$target"
  fi
}

rebuild_o "libs/xrc/CMakeFiles/wxxrc.dir/__/__/__/__/src/xrc/xmlres.cpp.o" \
  "$ROOT/third_party/wxWidgets/src/xrc/xmlres.cpp"
rebuild_o "libs/xrc/CMakeFiles/wxxrc.dir/__/__/__/__/src/xrc/xh_menu.cpp.o" \
  "$ROOT/third_party/wxWidgets/src/xrc/xh_menu.cpp"

XRC_OBJS=("$BUILD"/libs/xrc/CMakeFiles/wxxrc.dir/__/__/__/__/src/xrc/*.o)
if [[ ! -e "${XRC_OBJS[0]}" ]]; then
  echo "[f-ui-2] no xrc object files"
  exit 1
fi

echo "[f-ui-2] linking libwx_ohosu_xrc (${#XRC_OBJS[@]} objects)"
"$CXX" --target=aarch64-linux-ohos --sysroot="$SYSROOT" \
  -shared -fPIC \
  -Wl,-soname,libwx_ohosu_xrc-3.3-OHOS.so.4 \
  -o "$OUT" "${XRC_OBJS[@]}" \
  -L"$HAP_LIBS" \
  -lwx_ohosu_core-3.3-OHOS \
  -lwx_baseu-3.3-OHOS \
  -lwx_baseu_xml-3.3-OHOS \
  -lwx_ohosu_html-3.3-OHOS \
  -lpthread -lm \
  -lc++_shared

cp -f "$OUT" "$HAP_LIBS/libwx_ohosu_xrc-3.3-OHOS.so.4" || true
cp -f "$OUT" "$HAP_LIBS/libwx_ohosu_xrc-3.3-OHOS.so" || true
echo "[f-ui-2] staged $OUT"
