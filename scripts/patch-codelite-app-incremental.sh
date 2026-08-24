#!/usr/bin/env bash
# Incremental relink of libcodelite_app.so — ONLY whitelisted .o files may differ
# from the frozen baseline. Never recompile frame.cpp.o here.
#
# Usage:
#   host/fw2-hap/scripts/patch-codelite-app-incremental.sh
#   PATCH_OBJECTS="pluginmanager.cpp.o" host/fw2-hap/scripts/patch-codelite-app-incremental.sh
#
# After relink:
#   host/fw2-hap/scripts/stage-wx-libs.sh
#   repack/install HAP and verify [F-4.path] / [F-4.scan] + MV-1 / B-7
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD="${ROOT}/build-codelite-ohos"
cd "$BUILD"

APP_O="LiteEditor/CMakeFiles/codelite_app.dir"
PRE_F4="lib/libcodelite_app.so.pre-f4.bak"
ALLOW="${PATCH_OBJECTS:-pluginmanager.cpp.o f4_ohos_link_stubs.cpp.o}"
BOOT="${ROOT}/codelite/PCH/ohos_wx_bootstrap.h"

# --- Guard: frame.cpp.o must not be newer than pre-F4 link ---
FRAME_O="$APP_O/frame.cpp.o"
if [[ ! -f "$FRAME_O" ]]; then
  echo "[patch] ERROR: missing $FRAME_O"
  echo "[patch] Restore from baseline: restore-ohos-baseline.sh"
  exit 1
fi
if [[ -f "$PRE_F4" ]]; then
  frame_mtime=$(stat -f %m "$FRAME_O" 2>/dev/null || stat -c %Y "$FRAME_O")
  pre_mtime=$(stat -f %m "$PRE_F4" 2>/dev/null || stat -c %Y "$PRE_F4")
  if [[ "$frame_mtime" -gt "$pre_mtime" ]]; then
    echo "[patch] ERROR: frame.cpp.o is NEWER than pre-F4 lib ($PRE_F4)."
    echo "[patch] Recompiled frame breaks ABI (clMainFrame / wxToolTip SIGSEGV)."
    echo "[patch] Restore Aug-9 08:55 frame.cpp.o from baseline backup."
    exit 1
  fi
fi

for o in $ALLOW; do
  if [[ ! -f "$APP_O/$o" ]]; then
    echo "[patch] missing allowed object: $APP_O/$o"
    if [[ "$o" == "pluginmanager.cpp.o" ]]; then
      echo "[patch] compile with: -include $BOOT"
    fi
    exit 1
  fi
done

if [[ ! -f "$PRE_F4" ]]; then
  echo "[patch] WARN: no $PRE_F4 — link may not match last verified boot"
fi

cp -n "$PRE_F4" "${PRE_F4}.autobak" 2>/dev/null || true
cp -p "lib/libcodelite_app.so" "lib/libcodelite_app.so.pre-patch.bak" 2>/dev/null || true

ninja -C . -t commands lib/libcodelite_app.so | tail -1 > /tmp/link_codelite_app.cmd
python3 <<'PY'
import glob, os, subprocess, sys
cmd=open('/tmp/link_codelite_app.cmd').read().strip()
first=cmd.split(' -o lib/libcodelite_app.so ',1)
rest=first[1]
idx=len(rest)
for marker in [' -L/Users',' lib/libplugin.so']:
    p=rest.find(marker)
    if p!=-1: idx=min(idx,p)
libs=rest[idx:]
objs=sorted(glob.glob('LiteEditor/CMakeFiles/codelite_app.dir/*.o'))
objs=[o for o in objs if not o.endswith('.new-f4')]
open('/tmp/codelite_app_objs.rsp','w').write(' '.join(objs))
newcmd=first[0]+' -o lib/libcodelite_app.so @/tmp/codelite_app_objs.rsp '+libs
print('[patch] linking', len(objs), 'objects')
r=subprocess.run(newcmd, shell=True)
sys.exit(r.returncode)
PY

echo "[patch] probe strings:"
strings lib/libcodelite_app.so | rg '\[F-4\.(path|scan)\]' || true
echo "[patch] next: host/fw2-hap/scripts/stage-wx-libs.sh"
