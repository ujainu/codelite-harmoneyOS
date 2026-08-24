#!/usr/bin/env bash
# Deploy F-UI-3.3e + 3.4 wx core binary fixes and stage into HAP.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
HAP_LIBS="$ROOT/host/fw2-hap/entry/libs/arm64-v8a"
HAP="$ROOT/host/fw2-hap/entry/build/default/outputs/default/entry-default-unsigned.hap"

echo "[f-ui-3.3ef] stage pristine libcodelite from golden snapshot"
cp "$ROOT/host/fw2-hap/snapshots/f-ui-2-pass-20260811-155014/libcodelite_app.so" \
  "$HAP_LIBS/libcodelite_app.so"

echo "[f-ui-3.3ef] stage pristine wx core + stattext/statusbar boot patches"
cp "$ROOT/build-wx-ohos-gui/lib/libwx_ohosu_core-3.3-OHOS.so.4.0.0" \
  "$HAP_LIBS/libwx_ohosu_core-3.3-OHOS.so.4.0.0"
WX_SKIP_BASELINE_COPY=1 python3 "$ROOT/host/fw2-hap/scripts/patch-wx-stattext-inplace.py"

echo "[f-ui-3.3ef] binary patch SetMenuBar + MenuBar Attach path"
WX_SKIP_PRISTINE_COPY=1 python3 "$ROOT/host/fw2-hap/scripts/patch-wx-fui3-3ef-boot-menu-fix.py"
python3 "$ROOT/host/fw2-hap/scripts/patch-fui34-boot-progress.py"

echo "[f-ui-3.3ef] paint backing: EnsureBackingStore size fallback"
python3 "$ROOT/host/fw2-hap/scripts/patch-wx-paint-backing.py"

echo "[f-ui-3.4o] DC bind fallback (EnsureBackingStore fail → still GetBackingBitmap)"
python3 "$ROOT/host/fw2-hap/scripts/patch-wx-dc-bind-fallback.py"
echo "[f-ui-3.4q] DC bind TLW cache (parent-walk fail → cached main-thread TLW)"
python3 "$ROOT/host/fw2-hap/scripts/patch-wx-dc-bind-tlw-cache.py"
python3 "$ROOT/host/fw2-hap/scripts/patch-cl-completeinit-direct.py"
python3 "$ROOT/host/fw2-hap/scripts/patch-libentry-openeditor.py"

ln -sf libwx_ohosu_core-3.3-OHOS.so.4.0.0 "$HAP_LIBS/libwx_ohosu_core-3.3-OHOS.so.4"
ln -sf libwx_ohosu_core-3.3-OHOS.so.4 "$HAP_LIBS/libwx_ohosu_core-3.3-OHOS.so"

cd "$ROOT/host/fw2-hap/entry"
zip -u "$HAP" \
  libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so.4.0.0 \
  libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so.4 \
  libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so \
  libs/arm64-v8a/libcodelite_app.so \
  libs/arm64-v8a/libplugin.so

echo "[f-ui-3.3ef] wx core staged ($(stat -f%z "$HAP_LIBS/libwx_ohosu_core-3.3-OHOS.so.4.0.0" 2>/dev/null || stat -c%s "$HAP_LIBS/libwx_ohosu_core-3.3-OHOS.so.4.0.0") bytes)"
