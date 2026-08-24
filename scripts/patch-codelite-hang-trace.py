#!/usr/bin/env python3
"""Insert [FULL_UI_TRACE] hilog probes into libcodelite_app for CreateGUIControls hang diagnosis.

Cave must live in executable .text (NOT 0x772930 — that is past RX segment end).
Stubs must NOT touch x29/x30; only save x0-x15 around OH_LOG_Print.

Confirmed hang (2026-08-10): inside GetBestXButtonSize after [P-3.2] MemoryDC;
process never reaches post-0x5da428 CreateGUIControls steps.
"""
from __future__ import annotations

import shutil
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
CL = ROOT / "host/fw2-hap/entry/libs/arm64-v8a/libcodelite_app.so"
BASELINE = ROOT / "host/fw2-hap/baseline/20260809-f565-before-menu/libcodelite_app.so"

VA_OH_LOG = 0x762900
CAVE = 0x4FE65C  # overwrite CompilerMainPageBase ctor (unused during boot)
STR_BASE = CAVE + 0x2000
VA2OFF = lambda va: va - 0x1000

# (patch_va, original_insn, message) — resume at patch_va + 4
TRACES: list[tuple[int, int, str]] = [
    (0x5DA2B4, 0x94003143, "[FULL_UI_TRACE] before GetBestXButtonSize #1"),
    (0x5E682C, 0x9100C3E0, "[FULL_UI_TRACE] after wxMemoryDC ctor"),
    (0x5E6840, 0xF94023E0, "[FULL_UI_TRACE] after wxGCDC ctor"),
    (0x5E68A4, 0x297E57B6, "[FULL_UI_TRACE] after wxDC GetTextExtent"),
    (0x5DA428, 0xF94002A8, "[FULL_UI_TRACE] after GetBestXButtonSize #2"),
    (0x5DA484, 0xF9406FE8, "[FULL_UI_TRACE] after LoadMenuBar"),
    (0x5DA520, 0x52807900, "[FULL_UI_TRACE] after aui Update"),
    (0x5DA540, 0xF9400268, "[FULL_UI_TRACE] after statusbar"),
    (0x5DABEC, 0xF9018E77, "[FULL_UI_TRACE] after MainBook"),
    (0x5DB0F0, 0xD00010A8, "[FULL_UI_TRACE] after toolbar"),
]


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

    def hilog(self, msg_va: int, fmt_va: int, tag_va: int) -> None:
        self.emit(0x52800000)
        self.emit(0x52800081)
        self.emit(0x52800002)
        self.emit(0x72A01E02)
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


def build_stub(stub_va: int, msg: str, orig: int, resume_va: int, fmt_va: int, tag_va: int) -> bytes:
    blob, idx = pack_strings([msg])
    msg_va = stub_va + 0x60 + idx[0]
    a = A64(stub_va)
    a.save_volatile_regs()
    a.hilog(msg_va, fmt_va, tag_va)
    a.restore_volatile_regs()
    a.emit(orig)
    a.b(resume_va)
    code = bytearray(a.bytes())
    while len(code) < 0x60:
        code.extend(b"\x1f\x20\x03\xd5")
    code.extend(blob)
    while len(code) % 8:
        code.append(0)
    return bytes(code)


def main() -> None:
    if BASELINE.exists():
        shutil.copy2(BASELINE, CL)
        print(f"[trace] restored baseline -> {CL}")
    data = bytearray(CL.read_bytes())
    fmt_tag, idx = pack_strings(["%{public}s", "CodeLiteBoot"])
    fmt_va = STR_BASE + idx[0]
    tag_va = STR_BASE + idx[1]
    data[VA2OFF(STR_BASE) : VA2OFF(STR_BASE) + len(fmt_tag)] = fmt_tag

    stub_va = CAVE
    for site_va, orig, msg in TRACES:
        stub = build_stub(stub_va, msg, orig, site_va + 4, fmt_va, tag_va)
        data[VA2OFF(stub_va) : VA2OFF(stub_va) + len(stub)] = stub
        bl_insn = 0x94000000 | (((stub_va - site_va) // 4) & 0x03FFFFFF)
        data[VA2OFF(site_va) : VA2OFF(site_va) + 4] = struct.pack("<I", bl_insn)
        print(f"[trace] {msg} @ {site_va:#x} -> stub {stub_va:#x}")
        stub_va += (len(stub) + 15) & ~15

    CL.write_bytes(data)
    print(f"[trace] patched {CL} ({len(TRACES)} probes, cave={CAVE:#x})")


if __name__ == "__main__":
    main()
