#!/usr/bin/env python3
"""F-UI-3: binary [FUI_FRAME] hooks on wxFrame::SetMenuBar + wxStatusBar::Create."""
from __future__ import annotations

import struct
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
LIB = ROOT / "host/fw2-hap/entry/libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so.4.0.0"
BUILD_LIB = ROOT / "build-wx-ohos-gui/lib/libwx_ohosu_core-3.3-OHOS.so.4.0.0"

CAVE = 0x772930
NOP = b"\x1f\x20\x03\xd5"

HOOKS: list[tuple[str, str]] = [
    ("_ZN7wxFrame10SetMenuBarEP9wxMenuBar", "[FUI_FRAME] SetMenuBar enter"),
    ("_ZN11wxStatusBar6CreateEP8wxWindowilRK8wxString", "[FUI_FRAME] wxStatusBar Create enter"),
    ("_ZN7wxFrame12SetStatusBarEP11wxStatusBar", "[FUI_FRAME] SetStatusBar enter"),
]


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
    out = subprocess.check_output(["llvm-nm", "-D", str(path)], text=True)
    for line in out.splitlines():
        if sym in line and " T " in line:
            return int(line.split()[0], 16)
    raise SystemExit(f"{sym} not found in {path}")


def nm_ohlog_plt(path: Path) -> int:
    out = subprocess.check_output(["llvm-readelf", "-r", str(path)], text=True)
    for line in out.splitlines():
        if "OH_LOG_Print" in line and "JUMP_SLOT" in line:
            return int(line.split()[0], 16)
    raise SystemExit(f"OH_LOG_Print PLT not found in {path}")


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
        self.emit(0xD10083FF)
        self.emit(0xA9017BF3)

    def restore_caller_saved(self) -> None:
        self.emit(0xA9417BF3)
        self.emit(0x910083FF)

    def bytes(self) -> bytes:
        return b"".join(struct.pack("<I", i) for i in self.insns)


def pack_strings(items: list[str]) -> bytes:
    blob = bytearray()
    for s in items:
        blob.extend(s.encode() + b"\x00")
        while len(blob) % 8:
            blob.append(0)
    return bytes(blob)


def build_enter_stub(stub_va: int, ohlog_va: int, orig: int, resume_va: int, msg_va: int, fmt_va: int, tag_va: int) -> bytes:
    a = A64(stub_va)
    a.save_caller_saved()
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
    a.restore_caller_saved()
    a.emit(orig)
    a.b(resume_va)
    code = bytearray(a.bytes())
    while len(code) < 0x60:
        code.extend(NOP)
    return bytes(code)


def patch_core(path: Path) -> None:
    ohlog = nm_ohlog_plt(path)
    data = bytearray(path.read_bytes())

    str_base = CAVE + 0x600
    fmt_va = str_base
    tag_va = str_base + 0x20
    blob = pack_strings(["%{public}s", "wxOHOS"])
    data[va_to_offset(data, str_base) : va_to_offset(data, str_base) + len(blob)] = blob

    stub_va = CAVE
    for sym, msg in HOOKS:
        try:
            site = nm_sym(path, sym)
        except SystemExit:
            print(f"[f-ui-3-core] skip {sym} (not in {path.name})")
            continue
        msg_va = str_base + 0x40 + HOOKS.index((sym, msg)) * 0x40
        msg_blob = pack_strings([msg])
        moff = va_to_offset(data, msg_va)
        data[moff : moff + len(msg_blob)] = msg_blob

        orig = struct.unpack_from("<I", data, va_to_offset(data, site))[0]
        stub = build_enter_stub(stub_va, ohlog, orig, site + 4, msg_va, fmt_va, tag_va)
        soff = va_to_offset(data, stub_va)
        data[soff : soff + len(stub)] = stub
        bl = 0x94000000 | (((stub_va - site) // 4) & 0x03FFFFFF)
        data[va_to_offset(data, site) : va_to_offset(data, site) + 4] = struct.pack("<I", bl)
        print(f"[f-ui-3-core] {msg} @ 0x{site:x} -> stub 0x{stub_va:x}")
        stub_va += (len(stub) + 15) & ~15

    path.write_bytes(data)
    print(f"[f-ui-3-core] patched {path}")


def main() -> None:
    for p in (LIB, BUILD_LIB):
        if p.exists():
            patch_core(p)


if __name__ == "__main__":
    main()
