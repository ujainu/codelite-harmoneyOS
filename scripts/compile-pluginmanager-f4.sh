#!/usr/bin/env bash
# Compile ONLY pluginmanager.cpp.o for F-4 incremental patch (OHOS wx bootstrap).
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
BUILD="$ROOT/build-codelite-ohos"
BOOT="$ROOT/codelite/PCH/ohos_wx_bootstrap.h"
OUT="$BUILD/LiteEditor/CMakeFiles/codelite_app.dir/pluginmanager.cpp.o"
LOG="$ROOT/docs/logs/f4-pluginmanager-compile.log"

cd "$BUILD"
mkdir -p "$(dirname "$OUT")"

python3 <<PY
import json, subprocess, os
boot="-include $BOOT "
with open("compile_commands.json") as f:
    for e in json.load(f):
        if e.get("output","").endswith("codelite_app.dir/pluginmanager.cpp.o"):
            cmd=e["command"].replace("-o LiteEditor/", boot+"-o LiteEditor/")
            r=subprocess.run(cmd, shell=True, capture_output=True, text=True)
            open("$LOG","w").write((r.stdout or "")+(r.stderr or ""))
            print("exit", r.returncode)
            raise SystemExit(r.returncode)
print("compile command not found")
raise SystemExit(1)
PY

cp -p "$OUT" "${OUT}.f4.bak"
ls -la "$OUT"
strings "$OUT" | rg '\[F-4\.(path|scan)\]' || true
echo "[F-4] run: host/fw2-hap/scripts/patch-codelite-app-incremental.sh"
