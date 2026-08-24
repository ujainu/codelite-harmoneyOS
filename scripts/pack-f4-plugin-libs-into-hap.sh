#!/bin/zsh
# Patch built HAP: ensure F-4 plugin ELFs are in libs/arm64-v8a (dlopen from bundle el1).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
HAP="$ROOT/host/fw2-hap/entry/build/default/outputs/default/entry-default-unsigned.hap"
LIBS="$ROOT/host/fw2-hap/entry/libs/arm64-v8a"
CLLIB="$ROOT/build-codelite-ohos/lib"
TMP="$(mktemp -d)"

[[ -f "$HAP" ]] || { echo "[!] missing HAP: $HAP"; exit 1; }

mkdir -p "$TMP/libs/arm64-v8a"
for p in HelpPlugin EditorConfigPlugin; do
  for ext in dll so; do
    src="$LIBS/${p}.${ext}"
    [[ -f "$src" ]] || src="$CLLIB/${p}.so"
    [[ -f "$src" ]] && cp "$src" "$TMP/libs/arm64-v8a/${p}.${ext}"
  done
done

(cd "$TMP" && zip -q -u "$HAP" libs/arm64-v8a/HelpPlugin.dll libs/arm64-v8a/HelpPlugin.so \
  libs/arm64-v8a/EditorConfigPlugin.dll libs/arm64-v8a/EditorConfigPlugin.so)

rm -rf "$TMP"
echo "[*] F-4 plugin libs patched into $HAP"
unzip -l "$HAP" | rg "libs/arm64-v8a/(Help|Editor)" || true
