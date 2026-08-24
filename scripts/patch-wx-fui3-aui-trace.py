#!/usr/bin/env python3
"""F-UI-3: binary trace hooks in libwx_ohosu_aui for wxAuiManager / wxAuiNotebook."""
from __future__ import annotations

import struct
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
LIB = ROOT / "host/fw2-hap/entry/libs/arm64-v8a/libwx_ohosu_aui-3.3-OHOS.so.4.0.0"

# RX tail cave (within last PT_LOAD R+E, below RW at 0xa5ce0)
CAVE = 0xA4200
CAVE_SIZE = 0x800
NOP = b"\x1f\x20\x03\xd5"

ENTER_SITES: list[tuple[int, str]] = [
    (0x4CB60, "FuiAuiCtorEnter"),
    (0x4DDFC, "FuiAuiUpdateEnter"),
    (0x77FD8, "FuiAuiNotebookCreateEnter"),
]

RETURN_SITES: list[tuple[int, str, int]] = [
    (0x4CC7C, "FuiAuiCtorReturn", 0x4CC7C),
    (0x4E730, "FuiAuiUpdateReturn", 0x4E730),
    (0x78098, "FuiAuiNotebookCreateReturn", 0x78098),
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


def nm_sym(path: Path, name: str) -> int:
    out = subprocess.check_output(["llvm-nm", "-D", str(path)], text=True)
    for line in out.splitlines():
        parts = line.split()
        if len(parts) >= 3 and parts[2] == name:
            return int(parts[0], 16)
    raise SystemExit(f"symbol {name} not in {path}")


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

    def save_volatile_regs(self) -> None:
        self.emit(0xD100203F)
        self.emit(0xA90007E0)
        self.emit(0xA9010FE2)
        self.emit(0xA90217E4)
        self.emit(0xA9031FE6)
        self.emit(0xA90427E8)
        self.emit(0xA9052FEA)
        self.emit(0xA90637EC)
        self.emit(0xA9073FEE)

    def restore_volatile_regs(self) -> None:
        self.emit(0xA9473FEE)
        self.emit(0xA94637EC)
        self.emit(0xA9452FEA)
        self.emit(0xA94427E8)
        self.emit(0xA9431FE6)
        self.emit(0xA94217E4)
        self.emit(0xA9410FE2)
        self.emit(0xA94007E0)
        self.emit(0x9100803F)

    def bytes(self) -> bytes:
        return b"".join(struct.pack("<I", i) for i in self.insns)


def build_enter_stub(stub_va: int, helper_va: int, orig: int, resume_va: int) -> bytes:
    a = A64(stub_va)
    a.save_volatile_regs()
    a.bl(helper_va)
    a.restore_volatile_regs()
    a.emit(orig)
    a.b(resume_va)
    code = bytearray(a.bytes())
    while len(code) % 16:
        code.extend(NOP)
    return bytes(code)


def build_return_stub(stub_va: int, helper_va: int, resume_va: int) -> bytes:
    a = A64(stub_va)
    a.save_volatile_regs()
    a.bl(helper_va)
    a.restore_volatile_regs()
    a.b(resume_va)
    code = bytearray(a.bytes())
    while len(code) % 16:
        code.extend(NOP)
    return bytes(code)


def ensure_cave(data: bytearray) -> None:
    """NOP-fill RX cave bytes if section tail is not yet file-backed."""
    try:
        off = va_to_offset(data, CAVE)
    except SystemExit:
        pass
    else:
        end = va_to_offset(data, CAVE + CAVE_SIZE - 1) + 1
        if any(b != 0 for b in data[off:end]):
            return
    # extend RX PT_LOAD to cover cave if needed
    e_phoff = struct.unpack_from("<Q", data, 0x20)[0]
    e_phentsize = struct.unpack_from("<H", data, 0x36)[0]
    e_phnum = struct.unpack_from("<H", data, 0x38)[0]
    rx_idx = None
    for i in range(e_phnum):
        poff = e_phoff + i * e_phentsize
        p_type, p_flags, p_offset, p_vaddr, _p_paddr, p_filesz, p_memsz, _p_align = struct.unpack_from(
            "<IIQQQQQQ", data, poff
        )
        if p_type == 1 and (p_flags & 1):
            rx_idx = i
            rx_end_va = p_vaddr + p_filesz
            if CAVE + CAVE_SIZE <= rx_end_va:
                return
            need_va = CAVE + CAVE_SIZE - rx_end_va
            insert_at = p_offset + p_filesz
            grow = need_va
            data[insert_at:insert_at] = NOP * ((grow + 3) // 4)
            for j in range(e_phnum):
                joff = e_phoff + j * e_phentsize
                pt, pf, po, pv, pp, pfz, pmz, pa = struct.unpack_from("<IIQQQQQQ", data, joff)
                if po >= insert_at:
                    po += grow
                if pt == 1 and j == rx_idx:
                    pfz += grow
                    pmz = max(pmz, pfz)
                struct.pack_into("<IIQQQQQQ", data, joff, pt, pf, po, pv, pp, pfz, pmz, pa)
            return
    raise SystemExit("RX PT_LOAD not found")


def patch(path: Path) -> None:
    data = bytearray(path.read_bytes())
    ensure_cave(data)

    stub_va = CAVE
    for site_va, sym in ENTER_SITES:
        helper = nm_sym(path, sym)
        orig = struct.unpack_from("<I", data, va_to_offset(data, site_va))[0]
        stub = build_enter_stub(stub_va, helper, orig, site_va + 4)
        soff = va_to_offset(data, stub_va)
        data[soff : soff + len(stub)] = stub
        bl = 0x94000000 | (((stub_va - site_va) // 4) & 0x03FFFFFF)
        data[va_to_offset(data, site_va) : va_to_offset(data, site_va) + 4] = struct.pack("<I", bl)
        print(f"[f-ui-3-aui] enter {sym} @ 0x{site_va:x} -> stub 0x{stub_va:x}")
        stub_va += (len(stub) + 15) & ~15

    for patch_va, sym, resume in RETURN_SITES:
        helper = nm_sym(path, sym)
        stub = build_return_stub(stub_va, helper, resume)
        soff = va_to_offset(data, stub_va)
        data[soff : soff + len(stub)] = stub
        bl = 0x94000000 | (((stub_va - patch_va) // 4) & 0x03FFFFFF)
        data[va_to_offset(data, patch_va) : va_to_offset(data, patch_va) + 4] = struct.pack("<I", bl)
        print(f"[f-ui-3-aui] return {sym} @ 0x{patch_va:x} -> stub 0x{stub_va:x}")
        stub_va += (len(stub) + 15) & ~15

    path.write_bytes(data)
    print(f"[f-ui-3-aui] patched {path}")


def main() -> None:
    if not LIB.exists():
        raise SystemExit(f"missing {LIB}")
    patch(LIB)


if __name__ == "__main__":
    main()
