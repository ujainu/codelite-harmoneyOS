#!/usr/bin/env bash
# Recompile frame.cpp.o (flow traces only) + relink libcodelite_app.so for OHOS HAP.
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
CL="$ROOT/build-codelite-ohos"
OBJ_DIR="$CL/LiteEditor/CMakeFiles/codelite_app.dir"
FRAME_O="$OBJ_DIR/frame.cpp.o"
OUT="$CL/lib/libcodelite_app.so"
HAP_LIB="$ROOT/host/fw2-hap/entry/libs/arm64-v8a/libcodelite_app.so"
RSP="$CL/lib/fui31c-relink.rsp"
LLVM_BIN="/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native/llvm/bin"
CLANG="$LLVM_BIN/clang++"
CC_JSON="$CL/compile_commands.json"

if [[ ! -f "$CC_JSON" ]]; then
  echo "[fui31c-frame] missing $CC_JSON" >&2
  exit 1
fi

if [[ -f "$FRAME_O" ]]; then
  cp -p "$FRAME_O" "${FRAME_O}.bak-fui31c-pre"
  echo "[fui31c-frame] backed up frame.cpp.o → ${FRAME_O}.bak-fui31c-pre"
fi

echo "[fui31c-frame] compile frame.cpp (FUI_FRAME_FLOW traces)"
python3 - "$CC_JSON" "$FRAME_O" <<'PY'
import json, shlex, subprocess, sys

cc_json, frame_o = sys.argv[1:3]
entries = json.load(open(cc_json))
cmd = None
for e in entries:
    if not e["file"].endswith("/LiteEditor/frame.cpp"):
        continue
    if "codelite_app.dir" in e.get("output", ""):
        cmd = e["command"]
        break
if not cmd:
    raise SystemExit("no codelite_app frame.cpp compile command in compile_commands.json")

args = shlex.split(cmd)
out, skip = [], False
for a in args:
    if a == "-o":
        out.extend(["-o", frame_o])
        skip = True
        continue
    if skip:
        skip = False
        continue
    out.append(a)
print("[fui31c-frame] clang++ frame.cpp …")
subprocess.check_call(out)
PY

echo "[fui31c-frame] relink libcodelite_app.so"
python3 - "$CL" "$RSP" <<'PY'
import glob, subprocess, sys

cl, rsp = sys.argv[1:3]
cmd = subprocess.check_output(
    ["ninja", "-C", cl, "-t", "commands", "lib/libcodelite_app.so"],
    text=True,
    errors="replace",
)
lines = [l for l in cmd.splitlines() if "-shared" in l and "libcodelite_app.so" in l and "-o" in l]
if not lines:
    raise SystemExit("no link command")
link = lines[-1]
if link.startswith(": && "):
    link = link[4:]
if link.endswith(" && :"):
    link = link[:-5]
import shlex
args = shlex.split(link)[1:]
open(rsp, "w").write("\n".join(args) + "\n")
print(f"[fui31c-frame] rsp args={len(args)}")
PY

(cd "$CL" && "$CLANG" "@lib/fui31c-relink.rsp")

strings "$OUT" | grep -F '[FUI_FRAME_FLOW]' | head -5 || {
  echo "[fui31c-frame] WARN: FUI_FRAME_FLOW strings not found in linked lib" >&2
}

cp -f "$OUT" "$HAP_LIB"
echo "[fui31c-frame] staged $HAP_LIB"
