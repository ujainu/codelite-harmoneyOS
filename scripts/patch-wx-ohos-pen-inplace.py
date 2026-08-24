#!/usr/bin/env python3
"""F-UI-1.1: patch wxPen::GetJoin/GetCap in libwx to return OHOS defaults (no wxFAIL)."""
from __future__ import annotations

import shutil
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
LIB = ROOT / "host/fw2-hap/entry/libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so.4.0.0"
BUILD_LIB = ROOT / "build-wx-ohos-gui/lib/libwx_ohosu_core-3.3-OHOS.so.4.0.0"

VA_GET_CAP = 0x65B07C
VA_GET_JOIN = 0x65B128
# Skip wxFAIL_MSG blocks; fall through to epilogue with default enum value.
VA_GET_CAP_EPILOGUE = 0x65B11C
VA_GET_JOIN_EPILOGUE = 0x65B1C8
VA_PEN_CTOR_STYLE_CHECK = 0x65A6D4
VA_PEN_CTOR_OK = 0x65A720
VA_PEN_STYLE_CHECK = 0x65A770
VA_PEN_STYLE_OK = 0x65A7CC
WXJOIN_ROUND = 122
WXCAP_ROUND = 130


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


def patch_branch_to_epilogue(data: bytearray, branch_va: int, epilogue_va: int) -> None:
    off = va_to_offset(data, branch_va)
    off_words = (epilogue_va - branch_va) // 4
    insn = 0x14000000 | (off_words & 0x03FFFFFF)
    struct.pack_into("<I", data, off, insn)


def patch_return_value(data: bytearray, epilogue_va: int, value: int) -> None:
    off = va_to_offset(data, epilogue_va)
    # mov w0, #imm16
    insn = 0x52800000 | ((value & 0xFFFF) << 5)
    struct.pack_into("<I", data, off, insn)


def patch_one(path: Path) -> None:
    bak = path.with_suffix(path.suffix + ".bak-pen-patch")
    if not bak.exists():
        shutil.copy2(path, bak)
    data = bytearray(path.read_bytes())

    # GetCap: pen ok -> skip wxFAIL, return wxCAP_ROUND
    patch_branch_to_epilogue(data, 0x65B0A0, VA_GET_CAP_EPILOGUE)
    patch_return_value(data, VA_GET_CAP_EPILOGUE, WXCAP_ROUND)

    # GetJoin: pen ok -> skip wxFAIL, return wxJOIN_ROUND
    patch_branch_to_epilogue(data, 0x65B14C, VA_GET_JOIN_EPILOGUE)
    patch_return_value(data, VA_GET_JOIN_EPILOGUE, WXJOIN_ROUND)

    # wxPen(wxColour,int,wxPenStyle): skip invalid-style assert (DefaultWorkspacePage::OnPaint)
    patch_branch_to_epilogue(data, VA_PEN_CTOR_STYLE_CHECK, VA_PEN_CTOR_OK)

    # wxPen style enum assert (clRowEntry::Render uses non-solid styles)
    patch_branch_to_epilogue(data, VA_PEN_STYLE_CHECK, VA_PEN_STYLE_OK)

    path.write_bytes(data)
    print(f"[ohos-pen] patched GetCap/GetJoin defaults in {path}")


def main() -> None:
    targets = [p for p in (LIB, BUILD_LIB) if p.exists()]
    if not targets:
        raise SystemExit("no libwx_ohosu_core found")
    for p in targets:
        patch_one(p)


if __name__ == "__main__":
    main()
