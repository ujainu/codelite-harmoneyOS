#!/usr/bin/env python3
"""F-UI-3.1c: [FUI_FRAME_FLOW] probes in libcodelite_app CreateGUIControls (trace only).

Sites from golden snapshot libcodelite (no Harmony menu block in this .so):
  0x5da4a4 after wxString cleanup, clConfig::Get entry
  0x5da4b8 before clConfig::Read(showMenuBar)
  0x5da4f0 before SetMenuBar virtual call

Uses appended RX cave @ 0x772930 (same discipline as F-UI-1.2).
"""
from __future__ import annotations

import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
CL = ROOT / "host/fw2-hap/entry/libs/arm64-v8a/libcodelite_app.so"
SNAP_CL = ROOT / "host/fw2-hap/snapshots/f-ui-2-pass-20260811-155014/libcodelite_app.so"

VA_OH_LOG = 0x762900
CAVE = 0x772930
CAVE_FILE_BYTES = 0x1000
MARKER = b"FUI31CFLOW"

TRACES: list[tuple[int, int, str, int]] = [
    (0x5DA4A4, 0x94061FB3, "[FUI_FRAME_FLOW] after LoadMenuBar", 0),
    (0x5DA4B8, 0x94061442, "[FUI_FRAME_FLOW] before clConfig Read", 0),
    (0x5DA4F0, 0xF9400268, "[FUI_FRAME_FLOW] before SetMenuBar", 0),
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
    data[insert_at:insert_at] = b"\x1f\x20\x03\xd5" * (CAVE_FILE_BYTES // 4)
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

    def adrp(self, rd: int, va: int) -> None:
        page = va & ~0xFFF
        imm = ((page >> 12) - (self.here >> 12)) & 0x1FFFFFF
        immlo = (imm & 0x3) << 29
        immhi = (imm >> 2) << 5
        self.emit(0x90000000 | immlo | immhi | rd)

    def add_lo12(self, rd: int, rn: int, va: int) -> None:
        off = va & 0xFFF
        self.emit(0x91000000 | (off << 10) | (rn << 5) | rd)

    def save_x0_x1(self) -> None:
        # Callee-saved scratch — OH_LOG must preserve x19-x28; never adjust sp here
        # (wxString temp lives at [sp,#216] in CreateGUIControls).
        self.emit(0xAA0003FC)  # mov x28, x0
        self.emit(0xAA0103FB)  # mov x27, x1

    def restore_x0_x1(self) -> None:
        self.emit(0xAA1C03E0)  # mov x0, x28
        self.emit(0xAA1B03E1)  # mov x1, x27

    def hilog(self, msg_va: int, fmt_va: int, tag_va: int) -> None:
        self.emit(0x52800000)  # mov w0, #0  LOG_APP type
        self.emit(0x52800081)  # mov w1, #4  LOG_INFO
        self.emit(0x529E0062)  # mov w2, #0xF003  LOG_DOMAIN CodeLiteBoot
        self.adrp(3, tag_va)
        self.add_lo12(3, 3, tag_va)
        self.adrp(4, fmt_va)
        self.add_lo12(4, 4, fmt_va)
        self.adrp(5, msg_va)
        self.add_lo12(5, 5, msg_va)
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
    if (orig_insn & 0xFC000000) != 0x94000000:
        return orig_insn
    off = orig_insn & 0x03FFFFFF
    if off & 0x02000000:
        off -= 1 << 26
    target = orig_site + off * 4
    new_off = (target - new_pc) // 4
    return 0x94000000 | (new_off & 0x03FFFFFF)


def build_stub(stub_va: int, msg: str, orig: int, orig_site: int, resume_va: int) -> bytes:
    blob, idx = pack_strings([msg, "%{public}s", "CodeLiteBoot"])
    base = stub_va + 0x60
    msg_va = base + idx[0]
    fmt_va = base + idx[1]
    tag_va = base + idx[2]
    a = A64(stub_va)
    a.save_x0_x1()
    a.hilog(msg_va, fmt_va, tag_va)
    a.restore_x0_x1()
    a.emit(reencode_bl(orig, orig_site, a.here) if (orig & 0xFC000000) == 0x94000000 else orig)
    a.b(resume_va)
    code = bytearray(a.bytes())
    while len(code) < 0x60:
        code.extend(b"\x1f\x20\x03\xd5")
    code.extend(blob)
    while len(code) % 8:
        code.append(0)
    return bytes(code)


def main() -> None:
    if not CL.exists():
        raise SystemExit(f"missing {CL}")
    data = bytearray(CL.read_bytes())
    if MARKER in data:
        print(f"[f-ui-3.1c] already patched {CL}")
        return
    extend_rx_cave(data)
    stub_va = CAVE
    for site_va, orig, msg, continue_va in TRACES:
        resume = continue_va if continue_va else site_va + 4
        site_off = va_to_offset(data, site_va)
        cur = struct.unpack_from("<I", data, site_off)[0]
        if is_trace_stub_bl(cur, site_va):
            print(f"[f-ui-3.1c] skip {msg} @ {site_va:#x}")
            continue
        if cur != orig:
            raise SystemExit(f"[f-ui-3.1c] unexpected @ {site_va:#x}: {cur:#x} expected {orig:#x}")
        stub = build_stub(stub_va, msg, orig, site_va, resume)
        stub_off = va_to_offset(data, stub_va)
        data[stub_off : stub_off + len(stub)] = stub
        bl_insn = 0x94000000 | (((stub_va - site_va) // 4) & 0x03FFFFFF)
        struct.pack_into("<I", data, site_off, bl_insn)
        print(f"[f-ui-3.1c] {msg} @ {site_va:#x} -> {stub_va:#x}")
        stub_va += (len(stub) + 15) & ~15
    marker_off = va_to_offset(data, CAVE + CAVE_FILE_BYTES - 16)
    data[marker_off : marker_off + len(MARKER)] = MARKER
    CL.write_bytes(data)
    print(f"[f-ui-3.1c] patched {CL}")


if __name__ == "__main__":
    main()
