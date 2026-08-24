#!/usr/bin/env python3
"""F-8.2b/c/d: libentry boot-path patches (no DevEco rebuild required).

1) OpenEditorFile @ 0x56740 → return false (MainBook not wired).
2) Fw2_ProbeHarmonyBuildMenu @ 0x53e0c → ret (defer second LoadMenuBar pre-idle).
3) Fw2_ProbeHarmonyNewProject @ 0x57748 → ret.
4) Fw2_RunProjectProbe @ 0x5e664 → ret.
5) Fw2_ProbeCompilerBackend @ 0x503cc → b remote-dtor (skip LocalCompiler::Build + local dtors).

Do NOT write ASCII markers into .text (PLT-tail rule); stub bytes are the idempotency key.
"""
from __future__ import annotations

import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
HAP = ROOT / "host/fw2-hap/entry/build/default/outputs/default/entry-default-unsigned.hap"
HAP_LIB = ROOT / "host/fw2-hap/entry/libs/arm64-v8a/libentry.so"
PRISTINE = (
    ROOT
    / "host/fw2-hap/entry/build/default/intermediates/cmake/default/obj/arm64-v8a/libentry.so.bak-graphics-needed"
)
INTERMEDIATE = (
    ROOT
    / "host/fw2-hap/entry/build/default/intermediates/cmake/default/obj/arm64-v8a/libentry.so"
)

# libentry .text: vaddr = file_offset + 0x1000 (see llvm-readelf -S .text).
VA2OFF = lambda va: va - 0x1000

# ret; nop
STUB_RET = struct.pack("<II", 0xD65F03C0, 0xD503201F)
# mov w0, #0; ret
STUB_FALSE = struct.pack("<II", 0x52800000, 0xD65F03C0)

# (vaddr, stub, label) — converted to file offset before write.
PATCHES: list[tuple[int, bytes, str]] = [
    (0x56740, STUB_FALSE, "OpenEditorFile"),
    (0x53E0C, STUB_RET, "Fw2_ProbeHarmonyBuildMenu"),
    (0x57748, STUB_RET, "Fw2_ProbeHarmonyNewProject"),
    (0x5E664, STUB_RET, "Fw2_RunProjectProbe"),
]

# (vaddr, word, label) — single-instruction patches.
WORD_PATCHES: list[tuple[int, int, str]] = [
    # After LogBuildResult(remote): skip LocalCompiler path + local dtors → remote dtor @ 0x505f4
    (0x503CC, 0x1400008A, "Fw2_ProbeCompilerBackend skip LocalCompiler::Build"),
]

CORRUPT_MARKERS = (b"FUI34OEF", b"FUI34PBM", b"FUI34PNP", b"FUI34PRP")

LLVM_NM = Path(
    "/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native/llvm/bin/llvm-nm"
)


def is_cmake_libentry(path: Path) -> bool:
    """CMake/ninja libentry uses source stubs; legacy VA patches do not apply."""
    if not path.is_file():
        return False
    if path.stat().st_size < 2_000_000:
        return True
    if not LLVM_NM.is_file():
        return False
    import subprocess

    out = subprocess.check_output([str(LLVM_NM), "-C", str(path)], text=True, stderr=subprocess.DEVNULL)
    return "Fw2_EnsureMainMenuBarAttached" in out


def resolve_libentry() -> Path:
    for candidate in (HAP_LIB, INTERMEDIATE, PRISTINE):
        if candidate.is_file():
            return candidate
    if HAP.is_file():
        import subprocess
        import tempfile

        tmp = Path(tempfile.mkdtemp())
        subprocess.check_call(
            ["unzip", "-q", "-j", str(HAP), "libs/arm64-v8a/libentry.so", "-d", str(tmp)]
        )
        extracted = tmp / "libentry.so"
        if extracted.is_file():
            HAP_LIB.parent.mkdir(parents=True, exist_ok=True)
            HAP_LIB.write_bytes(extracted.read_bytes())
            return HAP_LIB
    raise SystemExit("[F-8.2b] libentry.so not found — build entry native target in DevEco first")


def is_correctly_patched(data: bytes) -> bool:
    for va, stub, _ in PATCHES:
        off = VA2OFF(va)
        if bytes(data[off : off + len(stub)]) != stub:
            return False
    for va, word, _ in WORD_PATCHES:
        off = VA2OFF(va)
        if struct.unpack_from("<I", data, off)[0] != word:
            return False
    return True


def patch(path: Path) -> None:
    if is_cmake_libentry(path):
        print(f"[F-8.2b] cmake libentry (source stubs) — skip legacy VA patches: {path}")
        return

    data = bytearray(path.read_bytes())
    if is_correctly_patched(data):
        print(f"[F-8.2b] libentry already patched: {path}")
        return

    if any(marker in data for marker in CORRUPT_MARKERS) or not PRISTINE.is_file():
        if not PRISTINE.is_file():
            raise SystemExit("[F-8.2b] libentry not patched and no pristine backup")
    print(f"[F-8.2b] restoring pristine libentry from {PRISTINE.name}")
    data = bytearray(PRISTINE.read_bytes())

    changed = False
    for va, stub, label in PATCHES:
        off = VA2OFF(va)
        cur = bytes(data[off : off + len(stub)])
        if cur == stub:
            print(f"[F-8.2b] skip {label} @ va 0x{va:X} (already stubbed)")
            continue
        print(f"[F-8.2b] {label} @ va 0x{va:X} file 0x{off:X}: was {cur.hex()}")
        data[off : off + len(stub)] = stub
        changed = True
    for va, word, label in WORD_PATCHES:
        off = VA2OFF(va)
        cur = struct.unpack_from("<I", data, off)[0]
        if cur == word:
            print(f"[F-8.2b] skip {label} @ va 0x{va:X} (already patched)")
            continue
        print(f"[F-8.2b] {label} @ va 0x{va:X} file 0x{off:X}: was {cur:#010x}")
        struct.pack_into("<I", data, off, word)
        changed = True
    if not changed:
        print(f"[F-8.2b] libentry already patched: {path}")
    path.write_bytes(data)
    print(f"[F-8.2b] libentry boot defer patches applied: {path}")


def main() -> None:
    # Prefer freshly built cmake output when present (run-on-emu copies after rebuild-wx).
    cmake_out = INTERMEDIATE
    if cmake_out.is_file() and is_cmake_libentry(cmake_out):
        path = cmake_out
        HAP_LIB.parent.mkdir(parents=True, exist_ok=True)
        HAP_LIB.write_bytes(cmake_out.read_bytes())
        print(f"[F-8.2b] staged cmake libentry: {cmake_out} -> {HAP_LIB}")
    else:
        path = resolve_libentry()
    patch(path)
    staged = HAP_LIB
    staged.parent.mkdir(parents=True, exist_ok=True)
    if path != staged:
        staged.write_bytes(path.read_bytes())
    if HAP.is_file():
        import subprocess

        entry_dir = ROOT / "host/fw2-hap/entry"
        rc = subprocess.call(["zip", "-u", str(HAP), "libs/arm64-v8a/libentry.so"], cwd=str(entry_dir))
        if rc not in (0, 12):
            raise SystemExit(f"[F-8.2b] zip libentry failed rc={rc}")
        print(f"[F-8.2b] HAP libentry updated: {HAP}")


if __name__ == "__main__":
    main()
