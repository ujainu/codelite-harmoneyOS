#!/usr/bin/env python3
"""F-UI-3.4p: keep PostConstruct CallAfter(CompleteInitialization) intact.

Direct CompleteInitialization from PostConstruct was reverted (UI THREAD_BLOCK).
A bad restore blob once wrote str instead of ldr @ 0x5E4950 — always verify against
golden bytes and repair if drifted. Clears FUI34CID marker when present.
"""
from __future__ import annotations

import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
CL = ROOT / "host/fw2-hap/entry/libs/arm64-v8a/libcodelite_app.so"

PATCH_VA = 0x5E4940
MARKER = (0x773AC0, b"FUI34CID")

# 88 bytes @ PostConstruct CallAfter from golden libcodelite_app.so (20260811 snapshot)
GOLDEN_CALLAFTER = bytes.fromhex(
    "000e8052"
    "9fea0594"
    "68100090"
    "f40300aa"
    "085941f9"
    "020140b9"
    "01008012"
    "edee0594"
    "68100090"
    "691000d0"
    "e00313aa"
    "e10314aa"
    "08c144f9"
    "29c143f9"
    "930a00f9"
    "6a0240f9"
    "9f3600f9"
    "08410091"
    "93a605a9"
    "880200f9"
    "482140f9"
    "00013fd6"
)


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
    data = bytearray(CL.read_bytes())
    off = va_to_offset(data, PATCH_VA)
    cur = bytes(data[off : off + len(GOLDEN_CALLAFTER)])
    changed = False

    if cur != GOLDEN_CALLAFTER:
        data[off : off + len(GOLDEN_CALLAFTER)] = GOLDEN_CALLAFTER
        changed = True
        print(f"[f-ui-3.4p] restored PostConstruct CallAfter @ {PATCH_VA:#x} ({len(GOLDEN_CALLAFTER)} bytes)")

    moff = va_to_offset(data, MARKER[0])
    if bytes(data[moff : moff + len(MARKER[1])]) == MARKER[1]:
        data[moff : moff + len(MARKER[1])] = b"\x00" * len(MARKER[1])
        changed = True
        print("[f-ui-3.4p] cleared FUI34CID marker")

    if not changed:
        print("[f-ui-3.4p] PostConstruct CallAfter OK")
        return

    CL.write_bytes(data)


if __name__ == "__main__":
    main()
