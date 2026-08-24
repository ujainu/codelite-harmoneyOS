#!/usr/bin/env python3
"""F-UI-3.2b: vtable dispatch slice @ CreateGUIControls SetMenuBar call (trace only).

Sites (golden libcodelite_app.so):
  0x5da4f0  ldr x8, [x19,#0]     log x19 = frame this
  0x5da4f8  ldr x8, [x8,#0x6e0]  log x8 = vtable (pre-slot)
  0x5da4fc  blr x8               log x8 = slot target, then blr

Domain F003 / CodeLiteBoot. Never adjust SP; never touch x19/x20.
"""
from __future__ import annotations

import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
CL = ROOT / "host/fw2-hap/entry/libs/arm64-v8a/libcodelite_app.so"

VA_OH_LOG = 0x762900
CAVE = 0x772930
CAVE_FILE_BYTES = 0x1000
MARKER = b"FUI32BVT\x00"

# (site_va, orig_insn, fmt_string, value_reg, resume_va or 0)
TRACES: list[tuple[int, int, str, int, int]] = [
    (0x5DA4F0, 0xF9400268, "[FUI_VTABLE] this=%{public}llx", 19, 0),
    (0x5DA4F8, 0xF9437108, "[FUI_VTABLE] vtable=%{public}llx", 8, 0),
    (0x5DA4FC, 0xD63F0100, "[FUI_VTABLE] slot=%{public}llx", 8, 0x5DA500),
]

NOP = b"\x1f\x20\x03\xd5"


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


def extend_rx_cave(data: bytearray) -> None:
    e_phoff = struct.unpack_from("<Q", data, 0x20)[0]
    e_phentsize = struct.unpack_from("<H", data, 0x36)[0]
    e_phnum = struct.unpack_from("<H", data, 0x38)[0]
    rx_idx = None
    rx_off = rx_filesz = 0
    for i in range(e_phnum):
        off = e_phoff + i * e_phentsize
        p_type, p_flags, p_offset, p_vaddr, _p_paddr, p_filesz, p_memsz, _p_align = struct.unpack_from(
            "<IIQQQQQQ", data, off
        )
        if p_type == 1 and (p_flags & 1):
            rx_idx = i
            rx_off = p_offset
            rx_filesz = p_filesz
    if rx_idx is None:
        raise SystemExit("RX PT_LOAD not found")
    if rx_filesz >= 0x370304 + CAVE_FILE_BYTES:
        return
    insert_at = rx_off + rx_filesz
    data[insert_at:insert_at] = NOP * (CAVE_FILE_BYTES // 4)
    for i in range(e_phnum):
        off = e_phoff + i * e_phentsize
        p_type, p_flags, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align = struct.unpack_from(
            "<IIQQQQQQ", data, off
        )
        if p_offset >= insert_at:
            p_offset += CAVE_FILE_BYTES
        if p_type == 1 and i == rx_idx:
            p_filesz += CAVE_FILE_BYTES
            p_memsz += CAVE_FILE_BYTES
        struct.pack_into("<IIQQQQQQ", data, off, p_type, p_flags, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align)
    e_shoff = struct.unpack_from("<Q", data, 0x28)[0]
    e_shentsize = struct.unpack_from("<H", data, 0x3A)[0]
    e_shnum = struct.unpack_from("<H", data, 0x3C)[0]
    if e_shoff >= insert_at:
        struct.pack_into("<Q", data, 0x28, e_shoff + CAVE_FILE_BYTES)
        e_shoff += CAVE_FILE_BYTES
    for i in range(e_shnum):
        soff = e_shoff + i * e_shentsize
        sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size, sh_link, sh_info, sh_addralign, sh_entsize = struct.unpack_from(
            "<IIQQQQIIQQ", data, soff
        )
        if sh_offset >= insert_at:
            sh_offset += CAVE_FILE_BYTES
        struct.pack_into(
            "<IIQQQQIIQQ",
            data,
            soff,
            sh_name,
            sh_type,
            sh_flags,
            sh_addr,
            sh_offset,
            sh_size,
            sh_link,
            sh_info,
            sh_addralign,
            sh_entsize,
        )


def is_trace_stub_bl(insn: int, site_va: int) -> bool:
    if (insn & 0xFC000000) != 0x94000000:
        return False
    off = insn & 0x03FFFFFF
    if off & 0x02000000:
        off -= 1 << 26
    target = site_va + off * 4
    return CAVE <= target < CAVE + CAVE_FILE_BYTES


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

    def adrp(self, rd: int, page_va: int) -> None:
        page = page_va & ~0xFFF
        imm = ((page >> 12) - (self.here >> 12)) & 0x1FFFFFF
        immlo = (imm & 0x3) << 29
        immhi = (imm >> 2) << 5
        self.emit(0x90000000 | immlo | immhi | rd)

    def add_lo12(self, rd: int, rn: int, va: int) -> None:
        off = va & 0xFFF
        self.emit(0x91000000 | (off << 10) | (rn << 5) | rd)

    def mov_reg(self, rd: int, rn: int) -> None:
        self.emit(0xAA0003E0 | (rn << 16) | rd)

    def save_scratch(self) -> None:
        self.emit(0xAA0003F8)  # x24 = x0
        self.emit(0xAA0103F9)  # x25 = x1
        self.emit(0xAA0203FA)  # x26 = x2
        self.emit(0xAA0303FB)  # x27 = x3
        self.emit(0xAA0803F7)  # x23 = x8

    def restore_scratch(self) -> None:
        self.emit(0xAA1803E0)  # x0 = x24
        self.emit(0xAA1903E1)  # x1 = x25
        self.emit(0xAA1A03E2)  # x2 = x26
        self.emit(0xAA1B03E3)  # x3 = x27
        self.emit(0xAA1703E8)  # x8 = x23

    def hilog_hex(self, fmt_va: int, tag_va: int, val_reg: int) -> None:
        self.emit(0x52800000)  # LOG_APP
        self.emit(0x52800081)  # INFO
        self.emit(0x529E0062)  # domain 0xF003
        if val_reg != 5:
            self.mov_reg(5, val_reg)
        self.adrp(3, tag_va)
        self.add_lo12(3, 3, tag_va)
        self.adrp(4, fmt_va)
        self.add_lo12(4, 4, fmt_va)
        self.bl(VA_OH_LOG)

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


def reencode_bl(orig_insn: int, orig_site: int, new_pc: int) -> int:
    off = orig_insn & 0x03FFFFFF
    if off & 0x02000000:
        off -= 1 << 26
    target = orig_site + off * 4
    new_off = (target - new_pc) // 4
    return 0x94000000 | (new_off & 0x03FFFFFF)


def build_stub(
    stub_va: int,
    fmt: str,
    val_reg: int,
    orig: int,
    orig_site: int,
    resume_va: int,
) -> bytes:
    blob, idx = pack_strings([fmt, "CodeLiteBoot"])
    base = stub_va + 0x80
    fmt_va = base + idx[0]
    tag_va = base + idx[1]
    a = A64(stub_va)
    a.save_scratch()
    a.hilog_hex(fmt_va, tag_va, val_reg)
    a.restore_scratch()
    is_blr = (orig & 0xFFFFFC1F) == 0xD63F0000
    if (orig & 0xFC000000) == 0x94000000:
        a.emit(reencode_bl(orig, orig_site, a.here))
    else:
        a.emit(orig)
    if not is_blr:
        a.b(resume_va)
    else:
        a.b(resume_va)
    code = bytearray(a.bytes())
    while len(code) < 0xA0:
        code.extend(NOP)
    code.extend(blob)
    while len(code) % 8:
        code.append(0)
    return bytes(code)


def patch_site(
    data: bytearray,
    stub_va: int,
    site_va: int,
    orig: int,
    fmt: str,
    val_reg: int,
    resume_va: int,
) -> int:
    site_off = va_to_offset(data, site_va)
    cur = struct.unpack_from("<I", data, site_off)[0]
    if is_trace_stub_bl(cur, site_va):
        print(f"[f-ui-3.2b] skip {fmt} @ {site_va:#x}")
        return stub_va
    if cur != orig:
        raise SystemExit(f"[f-ui-3.2b] unexpected @ {site_va:#x}: {cur:#x} expected {orig:#x}")
    resume = resume_va if resume_va else site_va + 4
    stub = build_stub(stub_va, fmt, val_reg, orig, site_va, resume)
    stub_off = va_to_offset(data, stub_va)
    data[stub_off : stub_off + len(stub)] = stub
    bl_insn = 0x94000000 | (((stub_va - site_va) // 4) & 0x03FFFFFF)
    struct.pack_into("<I", data, site_off, bl_insn)
    print(f"[f-ui-3.2b] {fmt.split('=')[0]} @ {site_va:#x} -> {stub_va:#x}")
    return stub_va + ((len(stub) + 15) & ~15)


def main() -> None:
    if not CL.exists():
        raise SystemExit(f"missing {CL}")
    data = bytearray(CL.read_bytes())
    if MARKER in data:
        print(f"[f-ui-3.2b] already patched {CL}")
        return
    for old in (b"FUI31ECL", b"FUI31DWS", b"FUI31CFLOW"):
        if old in data:
            raise SystemExit(f"[f-ui-3.2b] prior marker {old!r} — restore golden first")
    extend_rx_cave(data)
    stub_va = CAVE
    for site_va, orig, fmt, val_reg, resume in TRACES:
        stub_va = patch_site(data, stub_va, site_va, orig, fmt, val_reg, resume)
    marker_off = va_to_offset(data, CAVE + CAVE_FILE_BYTES - 48)
    data[marker_off : marker_off + len(MARKER)] = MARKER
    CL.write_bytes(data)
    print(f"[f-ui-3.2b] patched {CL} ({len(TRACES)} vtable probes)")


if __name__ == "__main__":
    main()
