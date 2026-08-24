#!/usr/bin/env python3
"""F-UI-3.4q/r: wxWindowDCImpl — cached TLW + backing bitmap for worker-thread DC bind.

Main-thread EnsureBackingStore success stores TLW (x21) and bitmap (x19) in BSS.
Worker parent-walk fail loads cached TLW; GetBackingBitmap null loads cached bitmap.

Uses NOP sled @ 0x64BC80 (not PLT tail).
"""
from __future__ import annotations

import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
WX = ROOT / "host/fw2-hap/entry/libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so.4.0.0"

TLW_CACHE = 0x794580
BMP_CACHE = 0x794588
STORE_CAVE = 0x64BC80
LOAD_CAVE = 0x64BC94
BMP_FALLBACK = 0x64BCA8
PARENT_FAIL = 0x653F20
EBS_SUCCESS = 0x6600E4
GET_BMP_NULL = 0x653FA4
CONTINUE = 0x653F38
BMP_OK = 0x653FA8
FAIL = 0x65410C
SILENT_FAIL = 0x654148

MARKER = (0x6F8490, b"FUI34TLC")  # exactly 8 bytes

ORIG_PARENT_FAIL = 0x1400007B  # b FAIL
ORIG_EBS_SUCCESS = 0x52800020  # mov w0, #1
ORIG_GET_BMP_NULL = 0xB4000D20  # cbz x0, SILENT_FAIL

# v1 layout (4-byte store cave) detection
OLD_STORE_FIRST = 0xB0000A48
OLD_STORE_STR_ONLY = 0xF902C115  # str x21 only @ +4


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


def adrp(rd: int, addr: int, pc: int) -> int:
    page = addr & ~0xFFF
    pc_page = pc & ~0xFFF
    imm = ((page - pc_page) >> 12) & 0x1FFFFF
    immlo = imm & 0x3
    immhi = (imm >> 2) & 0x7FFFF
    return 0x90000000 | (immlo << 29) | (immhi << 5) | (rd & 0x1F)


def b_insn(from_va: int, to_va: int) -> int:
    imm26 = ((to_va - from_va) >> 2) & 0x3FFFFFF
    return 0x14000000 | imm26


def bl_insn(from_va: int, to_va: int) -> int:
    imm26 = ((to_va - from_va) >> 2) & 0x3FFFFFF
    return 0x94000000 | imm26


def cbz(rt: int, from_va: int, to_va: int) -> int:
    imm19 = (to_va - from_va) >> 2
    return 0xB4000000 | ((imm19 & 0x7FFFF) << 5) | (rt & 0x1F)


def mem_insn(op: int, rt: int, rn: int, off: int) -> int:
    assert off % 8 == 0
    return op | ((off // 8) << 10) | (rn << 5) | rt


def cache_off(addr: int) -> int:
    return addr & 0xFFF


def build_caves() -> list[tuple[int, int]]:
    tlw_off = cache_off(TLW_CACHE)
    bmp_off = cache_off(BMP_CACHE)
    return [
        # store: TLW + bitmap from EnsureBackingStore success (x21/x19 live)
        (STORE_CAVE + 0, adrp(8, TLW_CACHE, STORE_CAVE)),
        (STORE_CAVE + 4, mem_insn(0xF9000000, 21, 8, tlw_off)),
        (STORE_CAVE + 8, mem_insn(0xF9000000, 19, 8, bmp_off)),
        (STORE_CAVE + 12, 0x52800020),  # mov w0, #1
        (STORE_CAVE + 16, 0xD65F03C0),  # ret
        # load: cached TLW when parent-walk exhausts
        (LOAD_CAVE + 0, adrp(8, TLW_CACHE, LOAD_CAVE)),
        (LOAD_CAVE + 4, mem_insn(0xF9400000, 21, 8, tlw_off)),
        (LOAD_CAVE + 8, cbz(21, LOAD_CAVE + 8, LOAD_CAVE + 16)),
        (LOAD_CAVE + 12, b_insn(LOAD_CAVE + 12, CONTINUE)),
        (LOAD_CAVE + 16, b_insn(LOAD_CAVE + 16, FAIL)),
        # fallback: cached bitmap when GetBackingBitmap returns null
        (BMP_FALLBACK + 0, adrp(8, BMP_CACHE, BMP_FALLBACK)),
        (BMP_FALLBACK + 4, mem_insn(0xF9400000, 25, 8, bmp_off)),
        (BMP_FALLBACK + 8, cbz(25, BMP_FALLBACK + 8, SILENT_FAIL)),
        (BMP_FALLBACK + 12, b_insn(BMP_FALLBACK + 12, BMP_OK)),
    ]


def already_patched(data: bytes, caves: list[tuple[int, int]]) -> bool:
    for va, insn in caves:
        off = va_to_offset(data, va)
        if struct.unpack_from("<I", data, off)[0] != insn:
            return False
    pf = struct.unpack_from("<I", data, va_to_offset(data, PARENT_FAIL))[0]
    ebs = struct.unpack_from("<I", data, va_to_offset(data, EBS_SUCCESS))[0]
    gbn = struct.unpack_from("<I", data, va_to_offset(data, GET_BMP_NULL))[0]
    return (
        pf == b_insn(PARENT_FAIL, LOAD_CAVE)
        and ebs == bl_insn(EBS_SUCCESS, STORE_CAVE)
        and gbn == cbz(0, GET_BMP_NULL, BMP_FALLBACK)
    )


def main() -> None:
    data = bytearray(WX.read_bytes())
    caves = build_caves()

    if already_patched(data, caves):
        print(f"[f-ui-3.4r] skip TLW/bitmap cache (already patched) in {WX.name}")
        return

    pf_off = va_to_offset(data, PARENT_FAIL)
    cur_pf = struct.unpack_from("<I", data, pf_off)[0]
    cur_ebs = struct.unpack_from("<I", data, va_to_offset(data, EBS_SUCCESS))[0]
    cur_gbn = struct.unpack_from("<I", data, va_to_offset(data, GET_BMP_NULL))[0]

    new_pf = b_insn(PARENT_FAIL, LOAD_CAVE)
    new_ebs = bl_insn(EBS_SUCCESS, STORE_CAVE)
    new_gbn = cbz(0, GET_BMP_NULL, BMP_FALLBACK)

    if cur_pf not in (ORIG_PARENT_FAIL, b_insn(PARENT_FAIL, 0x64BC90), new_pf):
        raise SystemExit(f"[f-ui-3.4r] unexpected @ {PARENT_FAIL:#x}: {cur_pf:#010x}")
    if cur_ebs not in (ORIG_EBS_SUCCESS, bl_insn(EBS_SUCCESS, STORE_CAVE), new_ebs):
        raise SystemExit(f"[f-ui-3.4r] unexpected @ {EBS_SUCCESS:#x}: {cur_ebs:#010x}")
    if cur_gbn not in (ORIG_GET_BMP_NULL, new_gbn):
        raise SystemExit(f"[f-ui-3.4r] unexpected @ {GET_BMP_NULL:#x}: {cur_gbn:#010x}")

    for va, insn in caves:
        struct.pack_into("<I", data, va_to_offset(data, va), insn)

    struct.pack_into("<I", data, pf_off, new_pf)
    struct.pack_into("<I", data, va_to_offset(data, EBS_SUCCESS), new_ebs)
    struct.pack_into("<I", data, va_to_offset(data, GET_BMP_NULL), new_gbn)

    moff = va_to_offset(data, MARKER[0])
    data[moff : moff + len(MARKER[1])] = MARKER[1]
    WX.write_bytes(data)
    print(
        f"[f-ui-3.4r] TLW/bitmap cache store@{EBS_SUCCESS:#x} "
        f"load@{PARENT_FAIL:#x} bmp@{GET_BMP_NULL:#x} "
        f"slots@{TLW_CACHE:#x}/{BMP_CACHE:#x}"
    )


if __name__ == "__main__":
    main()
