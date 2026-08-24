#!/usr/bin/env bash
# F-4: Relink libcodelite_app.so after pluginmanager.cpp.o rebuild ONLY.
# Does NOT run ninja (avoids liblibcodelite full rebuild).
#
# CRITICAL: Do NOT recompile frame.cpp.o — Aug-9 recompiles crash in clMainFrame
# (wxToolTip SIGSEGV). Keep the pre-F4 PIC frame.cpp.o from the Aug-9 08:55 link.
# Backup: lib/libcodelite_app.so.pre-f4.bak (boots on device; no [F-4.path] yet).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD="${ROOT}/build-codelite-ohos"
cd "$BUILD"

PM_O="LiteEditor/CMakeFiles/codelite_app.dir/pluginmanager.cpp.o"
FRAME_O="LiteEditor/CMakeFiles/codelite_app.dir/frame.cpp.o"
STUB_O="LiteEditor/CMakeFiles/codelite_app.dir/f4_ohos_link_stubs.cpp.o"
PRE_F4="lib/libcodelite_app.so.pre-f4.bak"
BOOT="${ROOT}/codelite/PCH/ohos_wx_bootstrap.h"

if [[ ! -f "$PM_O" ]]; then
  echo "[F-4] missing $PM_O — compile with:"
  echo "  -include $BOOT (see docs/logs/f4-pluginmanager-compile*.log)"
  exit 1
fi
if [[ ! -f "$FRAME_O" ]]; then
  echo "[F-4] missing $FRAME_O — restore from pre-F4 backup; do NOT recompile frame.cpp"
  echo "  pre-F4 lib: $PRE_F4"
  exit 1
fi
if [[ ! -f "$STUB_O" ]]; then
  echo "[F-4] missing $STUB_O — compile f4_ohos_link_stubs.cpp (wxClipboard::Get stub)"
  exit 1
fi

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
open('/tmp/codelite_app_objs.rsp','w').write(' '.join(objs))
newcmd=first[0]+' -o lib/libcodelite_app.so @/tmp/codelite_app_objs.rsp '+libs
print('[F-4] linking', len(objs), 'objects...')
r=subprocess.run(newcmd, shell=True)
sys.exit(r.returncode)
PY

strings lib/libcodelite_app.so | rg '\[F-4\.(path|scan)\]' || true
echo "[F-4] Stage: host/fw2-hap/scripts/stage-wx-libs.sh"
