#!/usr/bin/env python3
"""F-UI-3.3a-2: wxFrameBase::GetMenuBar weak copy in libcodelite_app @ 0x4659E8."""
from __future__ import annotations

import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
CL = ROOT / "host/fw2-hap/entry/libs/arm64-v8a/libcodelite_app.so"

VA_OH_LOG = 0x762900
CAVE = 0x773A00
MARKER = b"FUI33GMC\x00"
NOP = b"\x1f\x20\x03\xd5"

TRACES: list[tuple[int, int, str]] = [
    (0x4659E8, 0xF9415400, "[FUI_GETMB] app GetMenuBar enter"),
    (0x4659EC, 0xD65F03C0, "[FUI_GETMB] app GetMenuBar return"),
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


def is_bl_to_cave(insn: int, site: int) -> bool:
    if (insn & 0xFC000000) != 0x94000000:
        return False
    off = insn & 0x03FFFFFF
    if off & 0x02000000:
        off |= ~0x03FFFFFF
    return CAVE <= site + off * 4 < CAVE + 0x200


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

    def save_scratch(self) -> None:
        self.emit(0xAA0003F8)
        self.emit(0xAA0103F9)
        self.emit(0xAA0203FA)
        self.emit(0xAA0303FB)
        self.emit(0xAA0803F7)

    def restore_scratch(self) -> None:
        self.emit(0xAA1803E0)
        self.emit(0xAA1903E1)
        self.emit(0xAA1A03E2)
        self.emit(0xAA1B03E3)
        self.emit(0xAA1703E8)

    def bytes(self) -> bytes:
        return b"".join(struct.pack("<I", i) for i in self.insns)


def pack_strings(items: list[str]) -> tuple[bytes, list[int]]:
    blob = bytearray()
    offs: list[int] = []
    for s in items:
        offs.append(len(blob))
        blob.extend(s.encode() + b"\x00")
        while len(blob) % 8:
            blob.append(0)
    return bytes(blob), offs


def build_stub(stub_va: int, orig: int, resume_va: int, msg_va: int, fmt_va: int, tag_va: int) -> bytes:
    a = A64(stub_va)
    a.save_scratch()
    a.emit(0x52800000)
    a.emit(0x52800081)
    a.emit(0x529E0062)  # domain F003
    a.adrp(3, tag_va)
    a.add_lo12(3, 3, tag_va)
    a.adrp(4, fmt_va)
    a.add_lo12(4, 4, fmt_va)
    a.adrp(5, msg_va)
    a.add_lo12(5, 5, msg_va)
    a.bl(VA_OH_LOG)
    a.restore_scratch()
    a.emit(orig)
    if orig != 0xD65F03C0:
        a.b(resume_va)
    code = bytearray(a.bytes())
    while len(code) < 0x70:
        code.extend(NOP)
    return bytes(code)


def patch_cl(path: Path) -> None:
    if not path.exists():
        raise SystemExit(f"missing {path}")
    data = bytearray(path.read_bytes())
    if MARKER in data:
        print(f"[f-ui-3.3a-2] already patched app GetMenuBar in {path}")
        return
    blob, idx = pack_strings(["%{public}s", "CodeLiteBoot"] + [m for _, _, m in TRACES])
    str_va = CAVE + 0x180
    str_off = va_to_offset(data, str_va)
    data[str_off : str_off + len(blob)] = blob
    fmt_va = str_va + idx[0]
    tag_va = str_va + idx[1]
    msg_vas = [str_va + idx[2 + i] for i in range(len(TRACES))]

    stub_va = CAVE
    for i, (site_va, orig, msg) in enumerate(TRACES):
        site_off = va_to_offset(data, site_va)
        cur = struct.unpack_from("<I", data, site_off)[0]
        if is_bl_to_cave(cur, site_va):
            stub_va += 0x80
            continue
        if cur != orig:
            raise SystemExit(f"[f-ui-3.3a-2] unexpected @ {site_va:#x}: {cur:#x}")
        resume = site_va + 4
        stub = build_stub(stub_va, orig, resume, msg_vas[i], fmt_va, tag_va)
        so = va_to_offset(data, stub_va)
        data[so : so + len(stub)] = stub
        struct.pack_into(
            "<I",
            data,
            site_off,
            0x94000000 | (((stub_va - site_va) // 4) & 0x03FFFFFF),
        )
        print(f"[f-ui-3.3a-2] {msg} @ {site_va:#x} -> {stub_va:#x}")
        stub_va += 0x80

    marker_off = va_to_offset(data, CAVE + 0x1F0)
    data[marker_off : marker_off + len(MARKER)] = MARKER
    path.write_bytes(data)
    print(f"[f-ui-3.3a-2] patched {path}")


def main() -> None:
    patch_cl(CL)


if __name__ == "__main__":
    main()
