#!/usr/bin/env python3
"""F-UI-3.3: wxFrameBase::SetMenuBar path (actual vtable target from 3.2b).

Probes libwx_ohosu_core @ cave 0x6F7E00 (NOP tail, separate from FUI31SMB @ 0x6F7D00).
Uses register-only stubs for mid-frame probes; entry probe may touch stack after prologue.
"""
from __future__ import annotations

import struct
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
LIB = ROOT / "host/fw2-hap/entry/libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so.4.0.0"
LLVM = Path(
    "/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native/llvm/bin"
)

CAVE = 0x6F7E00
STR_BASE = 0x6F8500
NOP = b"\x1f\x20\x03\xd5"
MARKER = b"FUI33FB\x00"

# Minimal slice: one mid-frame probe after DetachMenuBar (F-UI-3.3b).
TRACES: list[tuple[int, int, str, bool]] = [
    (0x43FEB0, 0xF9400288, "[FUI_FRAMEBASE] post DetachMenuBar", False),
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
    return CAVE <= target < CAVE + 0x600


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

    def spill_regs(self) -> None:
        self.emit(0xD10103FF)  # sub sp, sp, #64
        self.emit(0xA90107E0)  # stp x0, x1, [sp]
        self.emit(0xA9020FE2)  # stp x2, x3, [sp, #16]
        self.emit(0xF90023E8)  # str x8, [sp, #32]

    def unspill_regs(self) -> None:
        self.emit(0xA94107E0)  # ldp x0, x1, [sp]
        self.emit(0xA9420FE2)  # ldp x2, x3, [sp, #16]
        self.emit(0xF94023E8)  # ldr x8, [sp, #32]
        self.emit(0x910103FF)  # add sp, sp, #64

    def save_caller_saved(self) -> None:
        self.emit(0xD10083FF)  # sub sp, sp, #32
        self.emit(0xA9017BF3)  # stp x19, x30, [sp, #16]

    def restore_caller_saved(self) -> None:
        self.emit(0xA9417BF3)  # ldp x19, x30, [sp, #16]
        self.emit(0x910083FF)  # add sp, sp, #32

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
    ohlog_va: int,
    orig: int,
    resume_va: int,
    msg_va: int,
    fmt_va: int,
    tag_va: int,
    *,
    use_stack_save: bool,
) -> bytes:
    a = A64(stub_va)
    if use_stack_save:
        a.save_caller_saved()
    else:
        a.spill_regs()
    a.emit(0x52800000)
    a.emit(0x52800081)
    a.emit(0x52800002)
    a.emit(0x72A01E02)  # domain 0xF004 wxOHOS
    a.adrp(3, tag_va)
    a.add_lo12(3, 3, tag_va)
    a.adrp(4, fmt_va)
    a.add_lo12(4, 4, fmt_va)
    a.adrp(5, msg_va)
    a.add_lo12(5, 5, msg_va)
    a.bl(ohlog_va)
    if use_stack_save:
        a.restore_caller_saved()
    else:
        a.unspill_regs()
    a.emit(orig)
    a.b(resume_va)
    code = bytearray(a.bytes())
    while len(code) < 0x70:
        code.extend(NOP)
    return bytes(code)


def patch_core(path: Path) -> None:
    if not path.exists():
        raise SystemExit(f"missing {path}")
    ohlog = nm_ohlog_plt(path)
    data = bytearray(path.read_bytes())
    marker_off = va_to_offset(data, CAVE + 0x600)
    if MARKER in data:
        print(f"[f-ui-3.3] already patched framebase path in {path}")
        return

    msgs = [m for _, _, m, _ in TRACES]
    blob, idx = pack_strings(["%{public}s", "wxOHOS", *msgs])
    str_off = va_to_offset(data, STR_BASE)
    data[str_off : str_off + len(blob)] = blob

    fmt_va = STR_BASE + idx[0]
    tag_va = STR_BASE + idx[1]
    msg_vas = [STR_BASE + idx[2 + i] for i in range(len(msgs))]

    stub_va = CAVE
    for i, (site_va, orig, msg, use_stack) in enumerate(TRACES):
        site_off = va_to_offset(data, site_va)
        cur = struct.unpack_from("<I", data, site_off)[0]
        if is_bl_to_cave(cur, site_va):
            print(f"[f-ui-3.3] skip {msg} @ {site_va:#x}")
            stub_va += 0x80
            continue
        if cur != orig:
            raise SystemExit(f"[f-ui-3.3] unexpected @ {site_va:#x}: {cur:#x} expected {orig:#x}")
        resume = site_va + 4
        stub = build_log_stub(
            stub_va, ohlog, orig, resume, msg_vas[i], fmt_va, tag_va,
            use_stack_save=use_stack,
        )
        so = va_to_offset(data, stub_va)
        data[so : so + len(stub)] = stub
        bl_insn = 0x94000000 | (((stub_va - site_va) // 4) & 0x03FFFFFF)
        struct.pack_into("<I", data, site_off, bl_insn)
        print(f"[f-ui-3.3] {msg} @ {site_va:#x} -> {stub_va:#x}")
        stub_va += 0x80

    data[marker_off : marker_off + len(MARKER)] = MARKER
    path.write_bytes(data)
    print(f"[f-ui-3.3] patched {path} ({len(TRACES)} framebase probes)")


def main() -> None:
    patch_core(LIB)


if __name__ == "__main__":
    main()
