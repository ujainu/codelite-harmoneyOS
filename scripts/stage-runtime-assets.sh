#!/bin/zsh
# B4-002: Stage official CodeLite Runtime Assets into HAP layout.
# Mirrors LiteEditor/CMakeLists.txt → CL_RESOURCES_DIR (share/codelite).
# Deployment only — does not touch Resource Loader / XML / PluginManager.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
CL="$ROOT/codelite"
CLLIB="$ROOT/build-codelite-ohos/lib"
HOST="$ROOT/host/fw2-hap"
STAGE="$HOST/runtime-assets/share/codelite"
RAWFILE="$HOST/entry/src/main/resources/rawfile/share/codelite"

rm -rf "$HOST/runtime-assets"
rm -rf "$HOST/entry/src/main/resources/rawfile/share"
mkdir -p "$STAGE"

copy_dir() {
  local src="$1" dest="$2"
  mkdir -p "$dest"
  # Prefer rsync if present; else cp -a
  if command -v rsync >/dev/null 2>&1; then
    rsync -a --exclude '.git' --exclude '.svn' --exclude '*.windows' "$src/" "$dest/"
  else
    cp -a "$src"/. "$dest"/
    find "$dest" -name '*.windows' -delete 2>/dev/null || true
  fi
}

# Directories (same DESTINATION set as upstream install(DIRECTORY ...))
copy_dir "$CL/Runtime/rc" "$STAGE/rc"
copy_dir "$CL/Runtime/lexers" "$STAGE/lexers"
copy_dir "$CL/Runtime/templates" "$STAGE/templates"
copy_dir "$CL/Runtime/gdb_printers" "$STAGE/gdb_printers"
copy_dir "$CL/Runtime/plugins/resources" "$STAGE/resources"
# Runtime/src/ trailing-slash semantics → contents into share/codelite/src/
mkdir -p "$STAGE/src"
if [[ -d "$CL/Runtime/src" ]]; then
  cp -a "$CL/Runtime/src"/. "$STAGE/src"/ 2>/dev/null || true
fi

# config with official renames (GTK defaults — OHOS is unix-like)
mkdir -p "$STAGE/config"
cp "$CL/Runtime/config/build_settings.xml.default" "$STAGE/config/"
cp "$CL/Runtime/config/codelite.xml.default.gtk" "$STAGE/config/codelite.xml.default"
cp "$CL/Runtime/config/debuggers.xml.gtk" "$STAGE/config/debuggers.xml.default"
[[ -f "$CL/Runtime/config/plugins.xml.default" ]] && \
  cp "$CL/Runtime/config/plugins.xml.default" "$STAGE/config/"
[[ -f "$CL/Runtime/config/codelite.layout.default" ]] && \
  cp "$CL/Runtime/config/codelite.layout.default" "$STAGE/config/"

# images (splash)
mkdir -p "$STAGE/images"
if [[ -f "$CL/art/splashscreen@2x.png" ]]; then
  cp "$CL/art/splashscreen@2x.png" "$STAGE/images/splashscreen.png"
  cp "$CL/art/splashscreen@2x.png" "$STAGE/images/splashscreen@2x.png"
fi

[[ -f "$CL/LICENSE" ]] && cp "$CL/LICENSE" "$STAGE/LICENSE"

# F-4: stage minimal plugin set into share/codelite/plugins/
# Current OHOS binary scans *.dll (same as MSW fallback); also ship .so for future builds.
mkdir -p "$STAGE/plugins"
for p in HelpPlugin EditorConfigPlugin; do
  if [[ -f "$CLLIB/${p}.so" ]]; then
    cp "$CLLIB/${p}.so" "$STAGE/plugins/${p}.dll"
    cp "$CLLIB/${p}.so" "$STAGE/plugins/${p}.so"
    echo "[*] staged plugin ${p} (.dll + .so)"
  else
    echo "[!] missing plugin build artifact: $CLLIB/${p}.so"
  fi
done

# Mirror into HAP rawfile (same tree)
mkdir -p "$(dirname "$RAWFILE")"
rm -rf "$RAWFILE"
cp -a "$STAGE" "$RAWFILE"

# Sanity
MENU="$STAGE/rc/menu.xrc"
if [[ ! -f "$MENU" ]]; then
  echo "[!] missing $MENU — official Runtime incomplete"
  exit 1
fi

echo "[*] staged Runtime Assets → $STAGE"
echo "[*] HAP rawfile           → $RAWFILE"
echo "[*] menu.xrc              → OK ($(wc -c < "$MENU") bytes)"
du -sh "$STAGE" "$RAWFILE"
