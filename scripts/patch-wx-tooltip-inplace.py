#!/usr/bin/env python3
"""Patch broken wxToolTip in libwx_ohosu_core — skip crashing wxString assign (boot-safe)."""
from __future__ import annotations

import shutil
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
LIB = ROOT / "host/fw2-hap/entry/libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so.4.0.0"
BUILD_LIB = ROOT / "build-wx-ohos-gui/lib/libwx_ohosu_core-3.3-OHOS.so.4.0.0"

# VA -> file offset via PT_LOAD .text
def va_to_offset(data: bytes, va: int) -> int:
    if data[:4] != b"\x7fELF":
        raise SystemExit("not ELF")
    e_phoff = struct.unpack_from("<Q", data, 0x20)[0]
    e_phentsize = struct.unpack_from("<H", data, 0x36)[0]
    e_phnum = struct.unpack_from("<H", data, 0x38)[0]
    for i in range(e_phnum):
        off = e_phoff + i * e_phentsize
        p_type, p_flags, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align = struct.unpack_from(
            "<IIQQQQQQ", data, off
        )
        if p_type != 1:
            continue
        if p_vaddr <= va < p_vaddr + p_filesz:
            return p_offset + (va - p_vaddr)
    raise SystemExit(f"VA 0x{va:x} not in PT_LOAD")


def patch(path: Path) -> None:
    bak = path.with_suffix(path.suffix + ".bak-tooltip-patch")
    if not bak.exists():
        shutil.copy2(path, bak)
    data = bytearray(path.read_bytes())

    # wxToolTip::wxToolTip — NOP the basic_string operator= call (keep zero-init m_text)
    ctor_bl = va_to_offset(data, 0x65F620)
    data[ctor_bl : ctor_bl + 4] = b"\x1f\x20\x03\xd5"  # nop

    # wxToolTip::SetTip — NOP branch to operator=; always ret with empty tip
    settip_b = va_to_offset(data, 0x65F65C)
    data[settip_b : settip_b + 4] = b"\x1f\x20\x03\xd5"  # nop

    path.write_bytes(data)
    print(f"[tooltip-patch] patched {path}")
    print(f"[tooltip-patch] backup {bak}")


def main() -> None:
    targets = [p for p in (LIB, BUILD_LIB) if p.exists()]
    if not targets:
        raise SystemExit("no libwx_ohosu_core found")
    for p in targets:
        patch(p)


if __name__ == "__main__":
    main()
