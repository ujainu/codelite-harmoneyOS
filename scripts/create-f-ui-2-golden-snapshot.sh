#!/usr/bin/env bash
# Freeze F-UI-2 PASS libs via full pipeline, then snapshot bytes + manifest.
#
# Rule: never snapshot bak-pen-patch alone — pipeline output drifts from backups.
# Rule: never use *.pre-fui3-patch for aui (ELF corrupt); use build-wx-ohos-gui clean.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
LIBS="$ROOT/host/fw2-hap/entry/libs/arm64-v8a"
SNAP="${F_UI2_GOLDEN_SNAP:-$ROOT/host/fw2-hap/snapshots/f-ui-2-pass-20260811-155014}"
REF_LOG="${F_UI2_REF_LOG:-docs/logs/f-ui-2-verify-20260811-155014.txt}"

echo "[golden] build fresh F-UI-2 pipeline (tooltip → stattext → renderer → pen → xrc)"
F_UI2_FORCE_LEGACY_RESTORE=1 bash "$ROOT/host/fw2-hap/scripts/restore-f-ui2-baseline.sh"
python3 "$ROOT/host/fw2-hap/scripts/patch-wx-stattext-inplace.py"
bash "$ROOT/host/fw2-hap/scripts/merge-wx-xrc-fui2.sh"

echo "[golden] create snapshot at $SNAP"
mkdir -p "$SNAP"

stage() {
  local name="$1"
  cp -f "$LIBS/$name" "$SNAP/$name"
}

for f in \
  libwx_ohosu_core-3.3-OHOS.so.4.0.0 \
  libwx_ohosu_aui-3.3-OHOS.so.4.0.0 \
  libwx_ohosu_xrc-3.3-OHOS.so.4.0.0 \
  libwx_baseu-3.3-OHOS.so.4.0.0 \
  libcodelite_app.so \
  libwx_ohosu_tooltip_stub.so
do
  stage "$f"
done

MANIFEST="$SNAP/manifest.txt"
{
  echo "# F-UI-2 golden snapshot (pipeline-frozen bytes)"
  echo "# reference_log: $REF_LOG"
  echo "# created: $(date -u +%Y-%m-%dT%H:%M:%SZ)"
  echo "# FORBIDDEN: *.pre-fui3-patch (aui corrupt)"
  echo "# FORBIDDEN: bak-pen-patch as restore source (pipeline drift)"
  echo "# restore: build-wx-ohos-gui aui OR explicit manifest hash"
  echo ""
  for f in \
    libwx_ohosu_core-3.3-OHOS.so.4.0.0 \
    libwx_ohosu_aui-3.3-OHOS.so.4.0.0 \
    libwx_ohosu_xrc-3.3-OHOS.so.4.0.0 \
    libwx_baseu-3.3-OHOS.so.4.0.0 \
    libcodelite_app.so \
    libwx_ohosu_tooltip_stub.so
  do
    path="$SNAP/$f"
    sz=$(stat -f%z "$path" 2>/dev/null || stat -c%s "$path")
    hash=$(shasum -a 256 "$path" | awk '{print $1}')
    echo "$hash  $sz  $f"
  done
} > "$MANIFEST"

echo "[golden] manifest:"
cat "$MANIFEST"
echo "[golden] done — restore with restore-f-ui-2-golden-snapshot.sh"
