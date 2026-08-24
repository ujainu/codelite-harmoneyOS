#!/usr/bin/env python3
"""F-UI-3.3a-2: GetMenuBar virtual target + thunk entry (libwx_ohosu_core).

Sites:
  0x43FE84 — SetMenuBar body (post-prologue)
  0x43FE94 — log x8 GetMenuBar target, blr x8 → resume 0x43FE98
  0x41C56C — wxFrameBase::GetMenuBar thunk enter (ldr #680)
  0x43FE98 — post GetMenuBar cmp

Marker FUI33GMB. Never combine with FUI33FA/FUI33FB on same lib.
"""
from __future__ import annotations

import os
import struct
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
LIB = ROOT / "host/fw2-hap/entry/libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so.4.0.0"
LLVM = Path(
    "/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native/llvm/bin"
)

CAVE = 0x6F7E00
STR_BASE = 0x6F8600
NOP = b"\x1f\x20\x03\xd5"
MARKER = b"FUI33GMB\x00"

LOG_PROBES: list[tuple[int, int, str]] = []
if os.environ.get("F_UI33A2_BLR_ONLY", "0") != "1":
    LOG_PROBES = [
        (0x43FE84, 0xF9400008, "[FUI_GETMB] SetMenuBar body"),
        (0x43FE98, 0xEB13001F, "[FUI_GETMB] post GetMenuBar cmp"),
    ]

BLR_SITE = 0x43FE94
BLR_ORIG = 0xD63F0100
BLR_RESUME = 0x43FE98
BLR_FMT = "[FUI_GETMB] target=%{public}llx"


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
    return CAVE <= target < CAVE + 0x800


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

    def mov_reg(self, rd: int, rn: int) -> None:
        self.emit(0xAA0003E0 | (rn << 16) | rd)

    def spill_regs(self) -> None:
        # SetMenuBar frame already has x20/x19 on [sp,#16]; OH_LOG clobbers x19-x28.
        self.emit(0xD10103FF)  # sub sp, sp, #64
        self.emit(0xA90107E0)  # stp x0, x1, [sp]
        self.emit(0xA9020FE2)  # stp x2, x3, [sp, #16]
        self.emit(0xF90023E8)  # str x8, [sp, #32]

    def unspill_regs(self) -> None:
        self.emit(0xA94107E0)  # ldp x0, x1, [sp]
        self.emit(0xA9420FE2)  # ldp x2, x3, [sp, #16]
        self.emit(0xF94023E8)  # ldr x8, [sp, #32]
        self.emit(0x910103FF)  # add sp, sp, #64

    def reload_setmenubar_frame(self) -> None:
        # wxFrameBase::SetMenuBar prologue: stp x20, x19, [sp, #16]
        self.emit(0xA9414FF4)  # ldp x20, x19, [sp, #16]

    def hilog_ptr(self, ohlog: int, fmt_va: int, tag_va: int, val_reg: int) -> None:
        self.emit(0x52800000)
        self.emit(0x52800081)
        self.emit(0x52800002)
        self.emit(0x72A01E02)  # domain F004 wxOHOS
        if val_reg != 5:
            self.mov_reg(5, val_reg)
        self.adrp(3, tag_va)
        self.add_lo12(3, 3, tag_va)
        self.adrp(4, fmt_va)
        self.add_lo12(4, 4, fmt_va)
        self.bl(ohlog)

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


def build_log_stub(
    stub_va: int,
    ohlog: int,
    orig: int,
    resume_va: int,
    msg_va: int,
    fmt_va: int,
    tag_va: int,
) -> bytes:
    a = A64(stub_va)
    a.spill_regs()
    a.adrp(5, msg_va)
    a.add_lo12(5, 5, msg_va)
    a.hilog_ptr(ohlog, fmt_va, tag_va, 5)
    a.unspill_regs()
    a.emit(orig)
    a.b(resume_va)
    code = bytearray(a.bytes())
    while len(code) < 0x70:
        code.extend(NOP)
    return bytes(code)


def build_blr_wrapper_stub(
    stub_va: int,
    ohlog: int,
    fmt_va: int,
    tag_va: int,
    resume_va: int,
) -> bytes:
    a = A64(stub_va)
    a.spill_regs()
    a.hilog_ptr(ohlog, fmt_va, tag_va, 8)
    a.unspill_regs()
    a.reload_setmenubar_frame()
    a.emit(BLR_ORIG)
    a.b(resume_va)
    code = bytearray(a.bytes())
    while len(code) < 0x70:
        code.extend(NOP)
    return bytes(code)


def patch_core(path: Path) -> None:
    if not path.exists():
        raise SystemExit(f"missing {path}")
    raw = path.read_bytes()
    for bad in (b"FUI33FA", b"FUI33FB"):
        if bad in raw:
            raise SystemExit(f"[f-ui-3.3a-2] marker {bad!r} present — restore golden first")
    ohlog = nm_ohlog_plt(path)
    data = bytearray(raw)
    marker_off = va_to_offset(data, CAVE + 0x700)
    if MARKER in data:
        print(f"[f-ui-3.3a-2] already patched GetMenuBar slice in {path}")
        return

    str_items = ["%{public}s", "%{public}llx", "wxOHOS", BLR_FMT]
    str_items += [m for _, _, m in LOG_PROBES]
    blob, idx = pack_strings(str_items)
    str_off = va_to_offset(data, STR_BASE)
    data[str_off : str_off + len(blob)] = blob

    fmt_s_va = STR_BASE + idx[0]
    fmt_x_va = STR_BASE + idx[1]
    tag_va = STR_BASE + idx[2]
    blr_fmt_va = STR_BASE + idx[3]
    msg_vas = [STR_BASE + idx[4 + i] for i in range(len(LOG_PROBES))]

    stub_va = CAVE
    for i, (site_va, orig, msg) in enumerate(LOG_PROBES):
        site_off = va_to_offset(data, site_va)
        cur = struct.unpack_from("<I", data, site_off)[0]
        if is_bl_to_cave(cur, site_va):
            print(f"[f-ui-3.3a-2] skip {msg} @ {site_va:#x}")
            stub_va += 0x80
            continue
        if cur != orig:
            raise SystemExit(f"[f-ui-3.3a-2] unexpected @ {site_va:#x}: {cur:#x} expected {orig:#x}")
        stub = build_log_stub(
            stub_va, ohlog, orig, site_va + 4, msg_vas[i], fmt_s_va, tag_va
        )
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

    site_off = va_to_offset(data, BLR_SITE)
    cur = struct.unpack_from("<I", data, site_off)[0]
    if not is_bl_to_cave(cur, BLR_SITE):
        if cur != BLR_ORIG:
            raise SystemExit(f"[f-ui-3.3a-2] unexpected blr @ {BLR_SITE:#x}: {cur:#x}")
        stub = build_blr_wrapper_stub(stub_va, ohlog, blr_fmt_va, tag_va, BLR_RESUME)
        so = va_to_offset(data, stub_va)
        data[so : so + len(stub)] = stub
        struct.pack_into(
            "<I",
            data,
            site_off,
            0x94000000 | (((stub_va - BLR_SITE) // 4) & 0x03FFFFFF),
        )
        print(f"[f-ui-3.3a-2] GetMenuBar blr wrapper @ {BLR_SITE:#x} -> {stub_va:#x}")
        stub_va += 0x80

    data[marker_off : marker_off + len(MARKER)] = MARKER
    path.write_bytes(data)
    print(f"[f-ui-3.3a-2] patched {path}")


def main() -> None:
    patch_core(LIB)


if __name__ == "__main__":
    main()
