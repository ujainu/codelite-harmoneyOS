#!/usr/bin/env python3
"""F-UI-3.1: single [FUI_FRAME] probe on wxFrame::SetMenuBar (enter + return only).

Patches HAP libwx_ohosu_core only (never build-wx copy). Uses NOP tail of the
renderer RX cave (0x6F7D00+) — NOT 0x772930 (past RX segment on this build).
"""
from __future__ import annotations

import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
LIB = ROOT / "host/fw2-hap/entry/libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so.4.0.0"
LLVM = Path(
    "/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native/llvm/bin"
)

SYM = "_ZN7wxFrame10SetMenuBarEP9wxMenuBar"
ENTER_MSG = "[FUI_FRAME] SetMenuBar enter"
RETURN_MSG = "[FUI_FRAME] SetMenuBar return"

# Renderer trampoline lives at 0x6F7BC0; stubs/strings in the remaining 4K tail.
CAVE = 0x6F7D00
STR_BASE = 0x6F7E80
RETURNInsn_OFFSET = 0x34  # ldp epilogue before tail br x1

NOP = b"\x1f\x20\x03\xd5"
MARKER = b"FUI31SMB\x00"


def va_to_offset(data: bytes, va: int) -> int:
    e_phoff = struct.unpack_from("<Q", data, 0x20)[0]
    e_phentsize = struct.unpack_from("<H", data, 0x36)[0]
    e_phnum = struct.unpack_from("<H", data, 0x38)[0]
    for i in range(e_phnum):
        off = e_phoff + i * e_phentsize
        p_type, _p_flags, p_offset, p_vaddr, _p_paddr, p_filesz, _p_memsz, _p_align = struct.unpack_from(
            "<IIQQQQQQ", data, off
        )
        if p_type != 1:
            continue
        if p_vaddr <= va < p_vaddr + p_filesz:
            return p_offset + (va - p_vaddr)
    raise SystemExit(f"VA 0x{va:x} not in PT_LOAD")


def nm_sym(path: Path, sym: str) -> int:
    nm = LLVM / "llvm-nm"
    out = subprocess.check_output([str(nm), "-D", str(path)], text=True)
    for line in out.splitlines():
        if sym in line and " T " in line:
            return int(line.split()[0], 16)
    raise SystemExit(f"{sym} not found in {path}")


def nm_ohlog_plt(path: Path) -> int:
    readelf = LLVM / "llvm-readelf"
    out = subprocess.check_output([str(readelf), "-r", str(path)], text=True)
    for line in out.splitlines():
        if "OH_LOG_Print" in line and "JUMP_SLOT" in line:
            return int(line.split()[0], 16)
    raise SystemExit(f"OH_LOG_Print PLT not found in {path}")


def is_bl_to_cave(insn: int, site: int) -> bool:
    if (insn & 0xFC000000) != 0x94000000:
        return False
    off = insn & 0x03FFFFFF
    if off & 0x02000000:
        off |= ~0x03FFFFFF
    target = site + off * 4
    return CAVE <= target < CAVE + 0x400


class A64:
    def __init__(self, pc: int) -> None:
        self.pc = pc
        self.insns: list[int] = []

    @property
    def here(self) -> int:
        return self.pc + len(self.insns) * 4

    def emit(self, w: int) -> None:
        self.insns.append(w & 0xFFFFFFFF)

    def bl(self, target: int) -> None:
        off = (target - self.here) // 4
        self.emit(0x94000000 | (off & 0x03FFFFFF))

    def b(self, target: int) -> None:
        off = (target - self.here) // 4
        self.emit(0x14000000 | (off & 0x03FFFFFF))

    def adrp(self, rd: int, va: int) -> None:
        page = va & ~0xFFF
        imm = ((page >> 12) - (self.here >> 12)) & 0x1FFFFFF
        immlo = (imm & 0x3) << 29
        immhi = (imm >> 2) << 5
        self.emit(0x90000000 | immlo | immhi | rd)

    def add_lo12(self, rd: int, rn: int, va: int) -> None:
        off = va & 0xFFF
        self.emit(0x91000000 | (off << 10) | (rn << 5) | rd)

    def save_caller_saved(self) -> None:
        self.emit(0xD10083FF)  # sub sp, sp, #32
        self.emit(0xA9017BF3)  # stp x19, x30, [sp, #16]

    def restore_caller_saved(self) -> None:
        self.emit(0xA9417BF3)  # ldp x19, x30, [sp, #16]
        self.emit(0x910083FF)  # add sp, sp, #32

    def bytes(self) -> bytes:
        return b"".join(struct.pack("<I", i) for i in self.insns)


def pack_strings(items: list[str]) -> bytes:
    blob = bytearray()
    for s in items:
        blob.extend(s.encode() + b"\x00")
        while len(blob) % 8:
            blob.append(0)
    return bytes(blob)


def build_log_stub(stub_va: int, ohlog_va: int, orig: int, resume_va: int, msg_va: int, fmt_va: int, tag_va: int) -> bytes:
    """Enter-style stub: log, run one original insn, branch to resume."""
    a = A64(stub_va)
    a.save_caller_saved()
    a.emit(0x52800000)  # mov w0, #0  LOG_APP
    a.emit(0x52800081)  # mov w1, #4  INFO
    a.emit(0x52800002)  # mov w2, #0
    a.emit(0x72A01E02)  # movk w2, #0xF004, lsl #16
    a.adrp(3, tag_va)
    a.add_lo12(3, 3, tag_va)
    a.adrp(4, fmt_va)
    a.add_lo12(4, 4, fmt_va)
    a.adrp(5, msg_va)
    a.add_lo12(5, 5, msg_va)
    a.bl(ohlog_va)
    a.restore_caller_saved()
    a.emit(orig)
    a.b(resume_va)
    code = bytearray(a.bytes())
    while len(code) < 0x60:
        code.extend(NOP)
    return bytes(code)


def build_return_stub(
    stub_va: int,
    ohlog_va: int,
    msg_va: int,
    fmt_va: int,
    tag_va: int,
    orig_ldp: int,
    orig_br: int,
) -> bytes:
    """Return stub at epilogue: preserve x1 (tail-call target) across OH_LOG."""
    a = A64(stub_va)
    a.emit(0xD10083FF)  # sub sp, sp, #32
    a.emit(0xA90107E1)  # stp x1, x8, [sp, #16]
    a.emit(0xA9007BF3)  # stp x19, x30, [sp]
    a.emit(0x52800000)
    a.emit(0x52800081)
    a.emit(0x52800002)
    a.emit(0x72A01E02)
    a.adrp(3, tag_va)
    a.add_lo12(3, 3, tag_va)
    a.adrp(4, fmt_va)
    a.add_lo12(4, 4, fmt_va)
    a.adrp(5, msg_va)
    a.add_lo12(5, 5, msg_va)
    a.bl(ohlog_va)
    a.emit(0xA9407BF3)  # ldp x19, x30, [sp]
    a.emit(0xA94107E1)  # ldp x1, x8, [sp, #16]
    a.emit(0x910083FF)  # add sp, sp, #32
    a.emit(orig_ldp)
    a.emit(orig_br)
    code = bytearray(a.bytes())
    while len(code) < 0x80:
        code.extend(NOP)
    return bytes(code)


def patch_core(path: Path) -> None:
    if not path.exists():
        raise SystemExit(f"missing {path}")

    site = nm_sym(path, SYM)
    return_site = site + RETURNInsn_OFFSET
    ohlog = nm_ohlog_plt(path)

    data = bytearray(path.read_bytes())
    enter_insn = struct.unpack_from("<I", data, va_to_offset(data, site))[0]
    if is_bl_to_cave(enter_insn, site):
        marker_off = va_to_offset(data, CAVE + 0x180)
        if data[marker_off : marker_off + len(MARKER)] == MARKER:
            print(f"[f-ui-3.1] already patched SetMenuBar @ 0x{site:x} — skip")
            return

    return_off = va_to_offset(data, return_site)
    orig_ldp = struct.unpack_from("<I", data, return_off)[0]
    orig_br = struct.unpack_from("<I", data, return_off + 4)[0]
    if orig_ldp != 0xA8C27BFD or orig_br != 0xD61F0020:
        raise SystemExit(
            f"unexpected SetMenuBar epilogue @ 0x{return_site:x}: "
            f"{orig_ldp:#010x} {orig_br:#010x} (disassemble before patching)"
        )

    fmt_va = STR_BASE
    tag_va = STR_BASE + 0x20
    enter_msg_va = STR_BASE + 0x40
    return_msg_va = STR_BASE + 0x80
    blob = pack_strings(["%{public}s", "wxOHOS", ENTER_MSG, RETURN_MSG])
    str_off = va_to_offset(data, STR_BASE)
    data[str_off : str_off + len(blob)] = blob

    enter_stub_va = CAVE
    return_stub_va = CAVE + 0x80
    marker_va = CAVE + 0x180
    data[va_to_offset(data, marker_va) : va_to_offset(data, marker_va) + len(MARKER)] = MARKER

    enter_stub = build_log_stub(
        enter_stub_va, ohlog, enter_insn, site + 4, enter_msg_va, fmt_va, tag_va
    )
    return_stub = build_return_stub(
        return_stub_va, ohlog, return_msg_va, fmt_va, tag_va, orig_ldp, orig_br
    )

    es_off = va_to_offset(data, enter_stub_va)
    rs_off = va_to_offset(data, return_stub_va)
    data[es_off : es_off + len(enter_stub)] = enter_stub
    data[rs_off : rs_off + len(return_stub)] = return_stub

    bl_enter = 0x94000000 | (((enter_stub_va - site) // 4) & 0x03FFFFFF)
    bl_return = 0x94000000 | (((return_stub_va - return_site) // 4) & 0x03FFFFFF)
    struct.pack_into("<I", data, va_to_offset(data, site), bl_enter)
    struct.pack_into("<I", data, return_off, bl_return)

    path.write_bytes(data)
    print(f"[f-ui-3.1] SetMenuBar enter @ 0x{site:x} -> stub 0x{enter_stub_va:x}")
    print(f"[f-ui-3.1] SetMenuBar return @ 0x{return_site:x} -> stub 0x{return_stub_va:x}")
    print(f"[f-ui-3.1] patched {path} (HAP lib only)")


def main() -> None:
    patch_core(LIB)


if __name__ == "__main__":
    main()
