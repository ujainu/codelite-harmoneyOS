#!/usr/bin/env python3
"""F-UI-1 dispatch trace: log [FUI_TRACE] at wx text-measurement entry points (no logic change).

wxDC::GetTextExtent is inline in wx/dc.h (m_pimpl->DoGetTextExtent); hook wxGCDCImpl::DoGetTextExtent.
wxCairoContext::GetTextExtent is not exported in this build (GetCairoRenderer returns null); hook
wxSVGGraphicsContext::GetTextExtent as the GraphicsContext virtual GetTextExtent implementation present.
"""
from __future__ import annotations

import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
LIB = ROOT / "host/fw2-hap/entry/libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so.4.0.0"
BUILD_LIB = ROOT / "build-wx-ohos-gui/lib/libwx_ohosu_core-3.3-OHOS.so.4.0.0"

VA_OH_LOG_PLT = 0x6F7770
# Append RX segment tail (VA hole 0x6f7bc0..0x6f8bc0) for trace stubs — must NOT
# overwrite live code (PostScript DoGetTextExtent is used by plugin lexer init).
CAVE = 0x6F7BC0
CAVE_FILE_BYTES = 0x1000
STR_BASE = CAVE + 0x800
NOP = b"\x1f\x20\x03\xd5"

# (entry_va, original_first_insn, log_name)
# Names match user-facing trace labels; wxDC::GetTextExtent is covered by GCDC DoGetTextExtent hook.
TRACE_SITES: list[tuple[int, int, str]] = [
    (0x3F5E38, 0xA9BE7BFD, "wxMemoryDC::SelectObject"),
    (0x44FA70, 0xA9BE7BFD, "wxGraphicsContext::Create(MemoryDC)"),
    (0x3FCAE0, 0xA9BB7BFD, "wxGCDCImpl::ctor(MemoryDC)"),
    (0x3FE0C8, 0xA9BE7BFD, "wxGCDCImpl::SetFont"),
    (0x655EC0, 0xA9BE7BFD, "wxFont::GetNativeFontInfo"),
    (0x40152C, 0xD10203FF, "wxGCDCImpl::DoGetTextExtent"),
    (0x401628, 0xAA0603F4, "wxGCDCImpl::DoGetTextExtent(null_ctx)"),
    (0x652504, 0xA9BB7BFD, "wxOhosDCImpl::DoGetTextExtent"),
    (0x4C5D90, 0xD10143FF, "wxCairoContext::GetTextExtent"),
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
    """Grow RX PT_LOAD to fill VA gap before RW; insert NOP cave bytes in file."""
    e_phoff = struct.unpack_from("<Q", data, 0x20)[0]
    e_phentsize = struct.unpack_from("<H", data, 0x36)[0]
    e_phnum = struct.unpack_from("<H", data, 0x38)[0]
    rx_idx = None
    rx_off = rx_filesz = rx_memsz = 0
    for i in range(e_phnum):
        off = e_phoff + i * e_phentsize
        p_type, p_flags, p_offset, p_vaddr, _p_paddr, p_filesz, p_memsz, _p_align = struct.unpack_from(
            "<IIQQQQQQ", data, off
        )
        if p_type == 1 and (p_flags & 1):
            rx_idx = i
            rx_off = p_offset
            rx_filesz = p_filesz
            rx_memsz = p_memsz
    if rx_idx is None:
        raise SystemExit("RX PT_LOAD not found")
    insert_at = rx_off + rx_filesz
    if len(data) < insert_at:
        raise SystemExit(f"RX end offset 0x{insert_at:x} past EOF")
    if rx_filesz >= 0x332464 + CAVE_FILE_BYTES:
        return
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
    print(f"[fui-trace] extended RX cave +0x{CAVE_FILE_BYTES:x} bytes @ VA 0x{CAVE:x}")


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

    def save_volatile_regs(self) -> None:
        self.emit(0xD100203F)  # sub sp, sp, #128
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

    def gettid(self) -> None:
        """Linux/OHOS aarch64: x0 = gettid() after svc."""
        self.emit(0xD2801648)  # mov x8, #178
        self.emit(0xD4000001)  # svc #0

    def hilog_tid(self, msg_va: int, fmt_va: int, tag_va: int) -> None:
        self.gettid()
        self.emit(0x2A0003E6)  # mov w6, w0  (tid for %{public}d)
        self.emit(0x52800000)  # mov w0, #0
        self.emit(0x52800081)  # mov w1, #4
        self.emit(0x52800002)  # mov w2, #0
        self.emit(0x72A01E02)  # movk w2, #384, lsl #16  -> LOG_DEBUG domain
        self.adrp(3, tag_va)
        self.add_lo12(3, 3, tag_va)
        self.adrp(4, fmt_va)
        self.add_lo12(4, 4, fmt_va)
        self.adrp(5, msg_va)
        self.add_lo12(5, 5, msg_va)
        self.bl(VA_OH_LOG_PLT)

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


def build_trace_stub(
    stub_va: int,
    msg: str,
    orig: int,
    resume_va: int,
    fmt_va: int,
    tag_va: int,
) -> bytes:
    blob, idx = pack_strings([msg])
    msg_va = stub_va + 0x80 + idx[0]
    a = A64(stub_va)
    a.save_volatile_regs()
    a.hilog_tid(msg_va, fmt_va, tag_va)
    a.restore_volatile_regs()
    a.emit(orig)
    a.b(resume_va)
    code = bytearray(a.bytes())
    while len(code) < 0x80:
        code.extend(NOP)
    code.extend(blob)
    while len(code) % 8:
        code.append(0)
    return bytes(code)


def patch_core(path: Path) -> None:
    data = bytearray(path.read_bytes())
    extend_rx_cave(data)
    fmt_blob, idx = pack_strings(
        [
            "[FUI_TRACE] %{public}s tid=%{public}d",
            "wxOHOS",
        ]
    )
    fmt_va = STR_BASE + idx[0]
    tag_va = STR_BASE + idx[1]
    off = va_to_offset(data, STR_BASE)
    data[off : off + len(fmt_blob)] = fmt_blob

    stub_va = CAVE
    for site_va, orig, name in TRACE_SITES:
        msg = name
        stub = build_trace_stub(stub_va, msg, orig, site_va + 4, fmt_va, tag_va)
        soff = va_to_offset(data, stub_va)
        data[soff : soff + len(stub)] = stub
        bl_insn = 0x94000000 | (((stub_va - site_va) // 4) & 0x03FFFFFF)
        eoff = va_to_offset(data, site_va)
        data[eoff : eoff + 4] = struct.pack("<I", bl_insn)
        print(f"[fui-trace] {name} @ 0x{site_va:x} -> stub 0x{stub_va:x}")
        stub_va += (len(stub) + 15) & ~15

    path.write_bytes(data)
    print(f"[fui-trace] patched {path} ({len(TRACE_SITES)} entry probes, cave=0x{CAVE:x})")


def main() -> None:
    targets = [p for p in (LIB, BUILD_LIB) if p.exists()]
    if not targets:
        raise SystemExit("no libwx_ohosu_core found")
    for p in targets:
        patch_core(p)


if __name__ == "__main__":
    main()
