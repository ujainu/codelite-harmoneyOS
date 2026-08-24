#!/usr/bin/env python3
"""F-UI-3.4o: wxWindowDCImpl — don't bail when EnsureBackingStore fails on worker thread.

Main-thread Show already created g_tlwBacking; MainLoop thread paints child controls
while EnsureBackingStore may fail (GetSize drift). Skip the tbz @ 0x653F94 so
GetBackingBitmap still runs against the existing TLW backing store.
"""
from __future__ import annotations

import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
WX = ROOT / "host/fw2-hap/entry/libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so.4.0.0"

SITE = 0x653F94
ORIG = 0x36000BC0  # tbz w0, #0, 0x65410c
NEW = 0xD503201F  # nop
MARKER = (0x6F8488, b"FUI34DBF")  # exactly 8 bytes — do not overwrite PLT NOP @ +8


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


def main() -> None:
    data = bytearray(WX.read_bytes())
    off = va_to_offset(data, SITE)
    cur = struct.unpack_from("<I", data, off)[0]
    if cur == NEW:
        print(f"[f-ui-3.4o] skip EnsureBackingStore-fail tbz @ {SITE:#x}")
        return
    if cur not in (ORIG, NEW):
        raise SystemExit(f"[f-ui-3.4o] unexpected @ {SITE:#x}: {cur:#010x} expected {ORIG:#010x}")
    struct.pack_into("<I", data, off, NEW)
    moff = va_to_offset(data, MARKER[0])
    data[moff : moff + len(MARKER[1])] = MARKER[1]
    WX.write_bytes(data)
    print(f"[f-ui-3.4o] DC bind fallback (NOP tbz) @ {SITE:#x} in {WX.name}")


if __name__ == "__main__":
    main()
