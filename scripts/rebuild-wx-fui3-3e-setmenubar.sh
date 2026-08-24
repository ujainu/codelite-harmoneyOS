#!/usr/bin/env bash
# F-UI-3.3e: deploy SetMenuBar fix (source relink if possible, else binary patch).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
# shellcheck source=/dev/null
source "$ROOT/toolchain/env.sh"

BUILD="$ROOT/build-wx-ohos-gui"
HAP_LIBS="$ROOT/host/fw2-hap/entry/libs/arm64-v8a"
HAP="$ROOT/host/fw2-hap/entry/build/default/outputs/default/entry-default-unsigned.hap"
FRAME_O="$BUILD/libs/core/CMakeFiles/wxcore.dir/__/__/__/__/src/ohos/frame.cpp.o"
BUILD_LIB="$BUILD/lib/libwx_ohosu_core-3.3-OHOS.so.4.0.0"

deploy_binary_patch() {
  echo "[f-ui-3.3e] binary patch wxFrameBase::SetMenuBar direct-call fix"
  python3 "$ROOT/host/fw2-hap/scripts/patch-wx-fui3-3e-setmenubar-direct-call.py"
}

try_source_relink() {
  if [[ ! -f "$FRAME_O" ]]; then
    echo "[f-ui-3.3e] compile frame.cpp.o"
    ninja -C "$BUILD" \
      "libs/core/CMakeFiles/wxcore.dir/__/__/__/__/src/ohos/frame.cpp.o"
  fi
  local link_cmd
  link_cmd=$(ninja -C "$BUILD" -t commands lib/libwx_ohosu_core-3.3-OHOS.so.4.0.0 2>/dev/null | tail -1)
  if [[ -z "$link_cmd" ]]; then
    return 1
  fi
  echo "[f-ui-3.3e] try link-only relink"
  (
    cd "$BUILD"
    # shellcheck disable=SC2086
    eval "${link_cmd#: && }"
  )
}

echo "[f-ui-3.3e] deploy SetMenuBar fix"
if try_source_relink 2>/dev/null; then
  sz=$(stat -f%z "$BUILD_LIB" 2>/dev/null || stat -c%s "$BUILD_LIB")
  if [[ "$sz" -gt 7000000 ]]; then
    echo "[f-ui-3.3e] source relink OK ($sz bytes)"
    cp -f "$BUILD_LIB" "$HAP_LIBS/"
  else
    echo "[f-ui-3.3e] relink produced tiny lib — fall back to binary patch"
    deploy_binary_patch
  fi
else
  echo "[f-ui-3.3e] link-only relink failed — binary patch"
  deploy_binary_patch
fi

ln -sf libwx_ohosu_core-3.3-OHOS.so.4.0.0 "$HAP_LIBS/libwx_ohosu_core-3.3-OHOS.so.4"
ln -sf libwx_ohosu_core-3.3-OHOS.so.4 "$HAP_LIBS/libwx_ohosu_core-3.3-OHOS.so"

cd "$ROOT/host/fw2-hap/entry"
zip -u "$HAP" \
  libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so.4.0.0 \
  libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so.4 \
  libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so

echo "[f-ui-3.3e] wx core staged ($(stat -f%z "$HAP_LIBS/libwx_ohosu_core-3.3-OHOS.so.4.0.0" 2>/dev/null || stat -c%s "$HAP_LIBS/libwx_ohosu_core-3.3-OHOS.so.4.0.0") bytes)"
