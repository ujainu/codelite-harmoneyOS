#!/bin/zsh
# Stage wxOHOS + CodeLiteApp + runtime deps into HAP libs/arm64-v8a (Minimal Boot).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
HOST="$ROOT/host/fw2-hap"
DEST="$HOST/entry/libs/arm64-v8a"
WXLIB="$ROOT/build-wx-ohos-gui/lib"
CLLIB="$ROOT/build-codelite-ohos/lib"
SQLIB="$ROOT/build-sqlite-ohos/install/lib"
CLAPP_CANDIDATES=(
  "$ROOT/build-codelite-ohos/lib/libcodelite_app.so"
  "$ROOT/build-codelite-ohos/LiteEditor/libcodelite_app.so"
)
CXXLIB="/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native/llvm/lib/aarch64-linux-ohos/libc++_shared.so"

mkdir -p "$DEST"
cp -L "$WXLIB"/libwx_ohosu_*.so* "$DEST/" 2>/dev/null || true
cp -L "$WXLIB"/libwx_baseu*.so* "$DEST/" 2>/dev/null || true
cp -L "$CXXLIB" "$DEST/"
if [[ -f "$ROOT/docs/logs/libz-ohos/libz.so" ]]; then
  cp "$ROOT/docs/logs/libz-ohos/libz.so" "$DEST/"
fi

# libcodelite_app / liblibcodelite / libplugin / libwxsqlite3 all NEEDED libsqlite3.so
if [[ -f "$SQLIB/libsqlite3.so" ]]; then
  cp -L "$SQLIB/libsqlite3.so" "$DEST/"
  echo "[*] staged libsqlite3.so from $SQLIB"
else
  echo "[!] missing $SQLIB/libsqlite3.so — B-1 native load will fail"
  exit 1
fi

# Official CodeLiteApp + core libs for EmbeddedStart
for f in liblibcodelite.so libplugin.so libwxsqlite3.so libdapcxx.so; do
  [[ -f "$CLLIB/$f" ]] && cp -L "$CLLIB/$f" "$DEST/"
done
# F-4: bundle plugin modules beside libcodelite_app (seeded to share/codelite/plugins at boot)
for p in HelpPlugin EditorConfigPlugin; do
  if [[ -f "$CLLIB/${p}.so" ]]; then
    cp -L "$CLLIB/${p}.so" "$DEST/${p}.dll"
    cp -L "$CLLIB/${p}.so" "$DEST/${p}.so"
    echo "[*] staged plugin ${p}.dll/.so for F-4 seed"
  fi
done
for c in "${CLAPP_CANDIDATES[@]}"; do
  if [[ -f "$c" ]]; then
    cp -L "$c" "$DEST/libcodelite_app.so"
    echo "[*] staged CodeLiteApp from $c"
    break
  fi
done

echo "[*] staged into $DEST"
ls -la "$DEST" | head -40
