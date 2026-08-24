#!/usr/bin/env bash
# Restore HAP libs from F-UI-2 golden snapshot only — no stattext/xrc rebuild, no binary patches.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
LIBS="$ROOT/host/fw2-hap/entry/libs/arm64-v8a"
SNAP="${F_UI2_GOLDEN_SNAP:-$ROOT/host/fw2-hap/snapshots/f-ui-2-pass-20260811-155014}"

if [[ ! -d "$SNAP" ]]; then
  echo "[golden-restore] snapshot missing: $SNAP" >&2
  echo "[golden-restore] run: bash host/fw2-hap/scripts/create-f-ui-2-golden-snapshot.sh" >&2
  exit 1
fi

verify_manifest() {
  local manifest="$SNAP/manifest.txt"
  [[ -f "$manifest" ]] || return 0
  while read -r line; do
    [[ "$line" =~ ^[0-9a-f]{64}[[:space:]]+[0-9]+[[:space:]]+(.+)$ ]] || continue
    local expect_hash="${BASH_REMATCH[0]%% *}"
    expect_hash=$(echo "$line" | awk '{print $1}')
    local expect_sz=$(echo "$line" | awk '{print $2}')
    local fname=$(echo "$line" | awk '{print $3}')
    local path="$SNAP/$fname"
    if [[ ! -f "$path" ]]; then
      echo "[golden-restore] FAIL manifest file missing: $fname" >&2
      exit 1
    fi
    local got_sz=$(stat -f%z "$path" 2>/dev/null || stat -c%s "$path")
    local got_hash=$(shasum -a 256 "$path" | awk '{print $1}')
    if [[ "$got_sz" != "$expect_sz" || "$got_hash" != "$expect_hash" ]]; then
      echo "[golden-restore] FAIL manifest mismatch: $fname" >&2
      echo "  expect sha256=$expect_hash size=$expect_sz" >&2
      echo "  got    sha256=$got_hash size=$got_sz" >&2
      exit 1
    fi
  done < "$manifest"
  echo "[golden-restore] manifest sha256 OK"
}

copy_one() {
  local name="$1"
  local src="$SNAP/$name"
  if [[ ! -f "$src" ]]; then
    echo "[golden-restore] FAIL missing $src" >&2
    exit 1
  fi
  cp -f "$src" "$LIBS/$name"
}

verify_manifest
copy_one "libwx_ohosu_core-3.3-OHOS.so.4.0.0"
copy_one "libwx_ohosu_aui-3.3-OHOS.so.4.0.0"
copy_one "libwx_ohosu_xrc-3.3-OHOS.so.4.0.0"
copy_one "libwx_baseu-3.3-OHOS.so.4.0.0"
copy_one "libcodelite_app.so"
copy_one "libwx_ohosu_tooltip_stub.so"

# Symlinks expected by loader / HAP layout
for base in libwx_ohosu_core libwx_ohosu_aui libwx_ohosu_xrc libwx_baseu; do
  cp -f "$LIBS/${base}-3.3-OHOS.so.4.0.0" "$LIBS/${base}-3.3-OHOS.so.4" 2>/dev/null || true
  cp -f "$LIBS/${base}-3.3-OHOS.so.4.0.0" "$LIBS/${base}-3.3-OHOS.so" 2>/dev/null || true
done
cp -f "$LIBS/libwx_ohosu_tooltip_stub.so" "$LIBS/libwx_ohos_graphics.so" 2>/dev/null || true

echo "[golden-restore] staged golden libs into $LIBS"
