#!/usr/bin/env python3
"""F-UI-3.4n: unblock TLW backing store when clMainFrame GetSize returns garbage.

EnsureBackingStore calls GetSize then GetClientSize via vtable; clMainFrame drift
yields 0/garbage sizes or null GetClientSize. MV-4.4 blue stays on paint fail.

Inline patch (no PLT-tail code cave):
1) After GetSize, always branch to seed block (skip GetClientSize).
2) Seed 800x600 then re-enter size validation (2090x1324 OOM on emulator).
3) Redirect size-fail branches to the seed block.
"""
from __future__ import annotations

import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
WX = ROOT / "host/fw2-hap/entry/libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so.4.0.0"

ALWAYS_SEED = 0x65FEB8  # b.gt 65fed8 → b seed block (skip GetClientSize)
SEED_BLOCK = 0x65FEBC
SEED_AFTER = 0x65FED8

SIZE_W_FAIL = 0x65FEE0
SIZE_H_FAIL = 0x65FEE8
SIZE_W_MIN = 0x65FEF4
SIZE_H_MIN = 0x65FEFC

MARKER = (0x6F8460, b"FUI34PBK")  # exactly 8 bytes — do not overwrite PLT NOP @ +8

SEED_INSNS = struct.pack(
    "<IIIIIII",
    0x52806408,  # mov w8, #800 width
    0x52804B09,  # mov w9, #600 height
    0xB9002BE9,  # str w9, [sp, #40] height
    0xB9002FE8,  # str w8, [sp, #44] width
    0x14000000 | (((SEED_AFTER - (SEED_BLOCK + 16)) >> 2) & 0x3FFFFFF),
    0xD503201F,
    0xD503201F,
)

# 2090x1324 seed (works on golden HW; bad_alloc on current emulator)
SEED_2090 = struct.pack(
    "<IIIIIII",
    0x52810548,
    0x5280A589,
    0xB9002BE9,
    0xB9002FE8,
    0x14000000 | (((SEED_AFTER - (SEED_BLOCK + 16)) >> 2) & 0x3FFFFFF),
    0xD503201F,
    0xD503201F,
)

# Broken STR offsets (#36/#40) from first patch attempt
OLD_SEED_WRONG = struct.pack(
    "<IIIIIII",
    0x52806408,
    0x52804B09,
    0xB90027E9,
    0xB9002BE8,
    0x14000000 | (((SEED_AFTER - (SEED_BLOCK + 16)) >> 2) & 0x3FFFFFF),
    0xD503201F,
    0xD503201F,
)

ORIG_GETCLIENT = struct.pack(
    "<IIIIIII",
    0xF94002A8,
    0x9100B3E1,
    0x9100A3E2,
    0xAA1503E0,
    0xF942BD08,
    0xD63F0100,
    0x294523E9,
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


def b_insn(from_va: int, to_va: int) -> int:
    imm26 = ((to_va - from_va) >> 2) & 0x3FFFFFF
    return 0x14000000 | imm26


def cond_branch(from_va: int, to_va: int, cond: int) -> int:
    imm19 = (to_va - from_va) >> 2
    return 0x54000000 | ((imm19 & 0x7FFFF) << 5) | (cond & 0xF)


def apply_bytes(data: bytearray, va: int, new: bytes, expect: bytes | None, label: str) -> bool:
    off = va_to_offset(data, va)
    cur = bytes(data[off : off + len(new)])
    if cur == new:
        print(f"[f-ui-3.4n] skip {label} @ {va:#x}")
        return False
    if expect is not None and cur != expect:
        raise SystemExit(
            f"[f-ui-3.4n] unexpected @ {va:#x}: {cur.hex()} expected {expect.hex()} ({label})"
        )
    data[off : off + len(new)] = new
    print(f"[f-ui-3.4n] {label} @ {va:#x} in {WX.name}")
    return True


def apply_u32(data: bytearray, va: int, new: int, expect: int | None, label: str) -> bool:
    return apply_bytes(
        data, va, struct.pack("<I", new), None if expect is None else struct.pack("<I", expect), label
    )


def apply_seed(data: bytearray) -> bool:
    off = va_to_offset(data, SEED_BLOCK)
    cur = bytes(data[off : off + len(SEED_INSNS)])
    if cur == SEED_INSNS:
        print(f"[f-ui-3.4n] skip inline 800x600 seed @ {SEED_BLOCK:#x}")
        return False
    if cur not in (ORIG_GETCLIENT, SEED_2090, OLD_SEED_WRONG):
        raise SystemExit(f"[f-ui-3.4n] unexpected seed block @ {SEED_BLOCK:#x}: {cur.hex()}")
    data[off : off + len(SEED_INSNS)] = SEED_INSNS
    print(f"[f-ui-3.4n] inline 800x600 seed (skip GetClientSize) @ {SEED_BLOCK:#x} in {WX.name}")
    return True


def main() -> None:
    data = bytearray(WX.read_bytes())
    changed = False
    changed |= apply_u32(
        data,
        ALWAYS_SEED,
        b_insn(ALWAYS_SEED, SEED_BLOCK),
        0x5400010C,
        "after GetSize always → seed block",
    )
    changed |= apply_seed(data)
    changed |= apply_u32(
        data,
        SIZE_W_FAIL,
        cond_branch(SIZE_W_FAIL, SEED_BLOCK, 0xB),
        0x540011AB,
        "width < 1 -> seed block",
    )
    changed |= apply_u32(
        data,
        SIZE_H_FAIL,
        cond_branch(SIZE_H_FAIL, SEED_BLOCK, 0xB),
        0x5400116B,
        "height < 1 -> seed block",
    )
    changed |= apply_u32(
        data,
        SIZE_W_MIN,
        cond_branch(SIZE_W_MIN, SEED_BLOCK, 0x3),
        0x54001103,
        "width < 64 -> seed block",
    )
    changed |= apply_u32(
        data,
        SIZE_H_MIN,
        cond_branch(SIZE_H_MIN, SEED_BLOCK, 0x3),
        0x540010C3,
        "height < 64 -> seed block",
    )

    if not changed:
        print(f"[f-ui-3.4n] skip {WX.name} (already patched)")
        return

    moff = va_to_offset(data, MARKER[0])
    data[moff : moff + len(MARKER[1])] = MARKER[1]
    WX.write_bytes(data)
    print("[f-ui-3.4n] paint backing patch applied")


if __name__ == "__main__":
    main()
