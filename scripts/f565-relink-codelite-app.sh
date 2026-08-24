#!/bin/zsh
# F-5.6.5: link-only relink libcodelite_app.so using the exact ninja link line
# (frame.cpp.o already built). Uses @response file to avoid ARG_MAX.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/../../.." && pwd)"
CL="$ROOT/build-codelite-ohos"
HAP_LIB="$ROOT/host/fw2-hap/entry/libs/arm64-v8a/libcodelite_app.so"
BASELINE="$ROOT/host/fw2-hap/baseline/20260809-f565-before-menu/libcodelite_app.so"
OBJ_DIR="$CL/LiteEditor/CMakeFiles/codelite_app.dir"
STUB_SRC="$ROOT/codelite/LiteEditor/f4_ohos_link_stubs.cpp"
STUB_O="$OBJ_DIR/f4_ohos_link_stubs.cpp.o"
OUT="$CL/lib/libcodelite_app.so"
RSP="$CL/lib/f565-relink.rsp"
LINK_CMD_FILE="$ROOT/docs/logs/f565-ninja-link-cmd.txt"
LLVM_BIN="/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native/llvm/bin"
CLANG="$LLVM_BIN/clang++"

if [[ ! -f "$OBJ_DIR/frame.cpp.o" ]]; then
  echo "[F-5.6.5] FAIL missing $OBJ_DIR/frame.cpp.o"
  exit 1
fi

echo "[F-5.6.5] backup baseline (if missing)"
mkdir -p "$(dirname "$BASELINE")"
if [[ ! -f "$BASELINE" ]]; then
  cp "$HAP_LIB" "$BASELINE"
fi
shasum -a 256 "$BASELINE" | tee "$(dirname "$BASELINE")/SHA256SUMS.txt"

echo "[F-5.6.5] compile OHOS link stubs (wxFileDialog/wxClipboard)"
python3 - "$CL" "$STUB_SRC" "$STUB_O" <<'PY'
import json, shlex, subprocess, sys
cl, stub, stub_o = sys.argv[1:4]
cc = json.load(open(f"{cl}/compile_commands.json"))
cmd = next(e["command"] for e in cc if e["file"].endswith("frame.cpp"))
args = shlex.split(cmd)
out, skip = [], False
for a in args:
    if a == "-o":
        out.extend(["-o", stub_o]); skip = True; continue
    if skip:
        skip = False; continue
    if a.endswith("frame.cpp"):
        out.append(stub); continue
    if a.startswith("-MD") or a.startswith("-MF") or a.startswith("-MT"):
        continue
    out.append(a)
subprocess.check_call(out, cwd=cl)
print(f"[F-5.6.5] stub object OK → {stub_o}")
PY

echo "[F-5.6.5] extract ninja link command"
python3 - "$CL" "$LINK_CMD_FILE" <<'PY'
import subprocess, sys
cl, out_path = sys.argv[1:3]
cmd = subprocess.check_output(["ninja", "-C", cl, "-t", "commands", "lib/libcodelite_app.so"], text=True, errors="replace")
lines = [l for l in cmd.splitlines() if "-shared" in l and "libcodelite_app.so" in l and "-o" in l]
if not lines:
    raise SystemExit("no link command found")
link = lines[-1]
if link.startswith(": && "):
    link = link[4:]
if link.endswith(" && :"):
    link = link[:-5]
open(out_path, "w").write(link)
print(f"[F-5.6.5] link cmd bytes={len(link)}")
PY

echo "[F-5.6.5] build response file (+ f4_ohos_link_stubs.cpp.o)"
python3 - "$LINK_CMD_FILE" "$RSP" "$STUB_O" <<'PY'
import shlex, sys
link = open(sys.argv[1]).read().strip()
stub = sys.argv[3]
args = shlex.split(link)[1:]
out, inserted = [], False
for a in args:
    if not inserted and a.startswith("-L"):
        out.append(stub); inserted = True
    out.append(a)
if not inserted:
    out.append(stub)
open(sys.argv[2], "w").write("\n".join(out) + "\n")
print(f"[F-5.6.5] rsp args={len(out)}")
PY

echo "[F-5.6.5] link libcodelite_app.so (ninja-equivalent, link-only)"
(cd "$CL" && "$CLANG" "@lib/f565-relink.rsp")

echo "[F-5.6.5] verify F565 symbols"
nm -D "$OUT" | grep -E 'F565|OnHarmony|EmbeddedStart' | head -12

echo "[F-5.6.5] install to HAP libs"
cp "$OUT" "$HAP_LIB"
shasum -a 256 "$OUT" "$HAP_LIB" | tee "$ROOT/docs/logs/f565-libcodelite-sha256.txt"
ls -la "$OUT" "$HAP_LIB"

echo "[F-5.6.5] relink OK → $HAP_LIB"
