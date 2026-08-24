#!/usr/bin/env python3
"""F-UI-1: patch libwx GetDefaultRenderer/GetCairoRenderer to use wxOhos renderer slot.

Requires libwx_ohos_graphics.so (merge-wx-graphics-renderer.sh) and libentry to fill
wxOhosRendererSlot via dlsym(wxOhosGetGraphicsRenderer) before wx UI init.
"""
from __future__ import annotations

import shutil
import struct
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SCRIPT_DIR = Path(__file__).resolve().parent
LIB = ROOT / "host/fw2-hap/entry/libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so.4.0.0"
BUILD_LIB = ROOT / "build-wx-ohos-gui/lib/libwx_ohosu_core-3.3-OHOS.so.4.0.0"
LIBENTRY = ROOT / "host/fw2-hap/entry/build/default/intermediates/cmake/default/obj/arm64-v8a/libentry.so"
HAP_LIBENTRY = ROOT / "host/fw2-hap/entry/libs/arm64-v8a/libentry.so"

VA_GET_DEFAULT = 0x43BA54
VA_GET_CAIRO = 0x43BA4C
VA_RENDERER_SLOT = 0x794548
CAVE = 0x6F7BC0
CAVE_FILE_BYTES = 0x1000
NOP = b"\x1f\x20\x03\xd5"
PATCH_SLOT_BYTES = 8


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
    e_phoff = struct.unpack_from("<Q", data, 0x20)[0]
    e_phentsize = struct.unpack_from("<H", data, 0x36)[0]
    e_phnum = struct.unpack_from("<H", data, 0x38)[0]
    rx_idx = None
    rx_off = rx_filesz = 0
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
    rx_vaddr = struct.unpack_from("<Q", data, e_phoff + rx_idx * e_phentsize + 16)[0]
    rx_end_vaddr = rx_vaddr + rx_filesz
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
        if p_type == 0x6474E552 and p_vaddr == rx_end_vaddr:
            p_vaddr += CAVE_FILE_BYTES
            if p_memsz > CAVE_FILE_BYTES:
                p_memsz -= CAVE_FILE_BYTES
        if p_type == 1 and i == rx_idx:
            p_filesz += CAVE_FILE_BYTES
            p_memsz += CAVE_FILE_BYTES
        struct.pack_into("<IIQQQQQQ", data, off, p_type, p_flags, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align)
    e_shoff = struct.unpack_from("<Q", data, 0x28)[0]
    e_shentsize = struct.unpack_from("<H", data, 0x3A)[0]
    e_shnum = struct.unpack_from("<H", data, 0x3C)[0]
    if e_shoff >= insert_at:
        struct.pack_into("<Q", data, 0x28, e_shoff + CAVE_FILE_BYTES)
        e_shoff += CAVE_FILE_BYTES
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
    print(f"[ohos-renderer] extended RX cave +0x{CAVE_FILE_BYTES:x} @ VA 0x{CAVE:x}")


class A64:
    def __init__(self, pc_base: int) -> None:
        self.pc_base = pc_base
        self.insns: list[int] = []

    @property
    def pc(self) -> int:
        return self.pc_base + len(self.insns) * 4

    def emit(self, insn: int) -> None:
        self.insns.append(insn & 0xFFFFFFFF)

    def bytes(self) -> bytes:
        return b"".join(struct.pack("<I", i) for i in self.insns)

    def b(self, target: int) -> None:
        off = (target - self.pc) // 4
        self.emit(0x14000000 | (off & 0x03FFFFFF))

    def adrp(self, rd: int, va: int) -> None:
        page = va & ~0xFFF
        imm = ((page >> 12) - (self.pc >> 12)) & 0x1FFFFFF
        immlo = (imm & 0x3) << 29
        immhi = (imm >> 2) << 5
        self.emit(0x90000000 | immlo | immhi | rd)

    def add_lo12(self, rd: int, rn: int, va: int) -> None:
        off = va & 0xFFF
        self.emit(0x91000000 | (off << 10) | (rn << 5) | rd)

    def ret(self) -> None:
        self.emit(0xD65F03C0)


def build_renderer_trampoline() -> bytes:
    a = A64(CAVE)
    a.adrp(0, VA_RENDERER_SLOT)
    a.add_lo12(0, 0, VA_RENDERER_SLOT)
    a.emit(0xF9400000)  # ldr x0, [x0]
    a.ret()
    return a.bytes()


def patch_core(path: Path) -> None:
    bak = path.with_suffix(path.suffix + ".bak-renderer-patch")
    if not bak.exists():
        shutil.copy2(path, bak)
    data = bytearray(path.read_bytes())
    extend_rx_cave(data)

    tramp = build_renderer_trampoline()
    off_cave = va_to_offset(data, CAVE)
    data[off_cave : off_cave + len(tramp)] = tramp

    for va in (VA_GET_DEFAULT, VA_GET_CAIRO):
        a_jump = A64(va)
        a_jump.b(CAVE)
        jump = a_jump.bytes() + NOP
        off = va_to_offset(data, va)
        data[off : off + PATCH_SLOT_BYTES] = jump

    off_slot = va_to_offset(data, VA_RENDERER_SLOT)
    struct.pack_into("<Q", data, off_slot, 0)

    path.write_bytes(data)
    print(f"[ohos-renderer] patched GetDefaultRenderer/GetCairoRenderer + slot @ 0x{VA_RENDERER_SLOT:x} in {path}")


def patch_libentry_needed(path: Path, libname: str = "libwx_ohos_graphics.so") -> None:
    if not path.exists():
        print(f"[ohos-renderer] skip libentry NEEDED (missing {path})")
        return
    bak = path.with_suffix(path.suffix + ".bak-graphics-needed")
    if not bak.exists():
        shutil.copy2(path, bak)
    data = bytearray(path.read_bytes())

    e_phoff = struct.unpack_from("<Q", data, 0x20)[0]
    e_phentsize = struct.unpack_from("<H", data, 0x36)[0]
    e_phnum = struct.unpack_from("<H", data, 0x38)[0]
    dyn_phdr_off = None
    dyn_off = dyn_filesz = 0
    for i in range(e_phnum):
        off = e_phoff + i * e_phentsize
        p_type, p_flags, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align = struct.unpack_from(
            "<IIQQQQQQ", data, off
        )
        if p_type == 2:
            dyn_phdr_off = off
            dyn_off, dyn_filesz = p_offset, p_filesz

    if dyn_phdr_off is None:
        raise SystemExit(f"no PT_DYNAMIC in {path}")

    e_shoff = struct.unpack_from("<Q", data, 0x28)[0]
    e_shentsize = struct.unpack_from("<H", data, 0x3A)[0]
    e_shnum = struct.unpack_from("<H", data, 0x3C)[0]
    dynstr_sh_off = None
    dynstr_off = dynstr_size = 0
    for i in range(e_shnum):
        soff = e_shoff + i * e_shentsize
        sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size, sh_link, sh_info, sh_addralign, sh_entsize = struct.unpack_from(
            "<IIQQQQIIQQ", data, soff
        )
        if sh_type == 3:
            dynstr_sh_off = soff
            dynstr_off, dynstr_size = sh_offset, sh_size

    if dynstr_sh_off is None:
        raise SystemExit(f"no .dynstr in {path}")

    null_off = None
    strsz_off = None
    for j in range(0, dyn_filesz, 16):
        tag, val = struct.unpack_from("<QQ", data, dyn_off + j)
        if tag == 0:
            null_off = dyn_off + j
        elif tag == 0xA:
            strsz_off = dyn_off + j
        elif tag == 1 and data[dynstr_off + val :].startswith(libname.encode()):
            print(f"[ohos-renderer] libentry already NEEDED {libname}")
            return
    if null_off is None:
        raise SystemExit("DT_NULL not found in libentry .dynamic")
    if strsz_off is None:
        raise SystemExit("DT_STRSZ not found in libentry .dynamic")

    new_str = libname.encode() + b"\x00"
    new_str_idx = struct.unpack_from("<Q", data, strsz_off + 8)[0]
    insert_str_at = dynstr_off + new_str_idx
    if insert_str_at + len(new_str) > len(data):
        data.extend(b"\x00" * (insert_str_at + len(new_str) - len(data)))
    data[insert_str_at : insert_str_at + len(new_str)] = new_str
    new_strsz = new_str_idx + len(new_str)
    struct.pack_into("<Q", data, strsz_off + 8, new_strsz)

    struct.pack_into("<QQ", data, null_off, 1, new_str_idx)
    struct.pack_into("<QQ", data, null_off + 16, 0, 0)

    p_type, p_flags, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align = struct.unpack_from(
        "<IIQQQQQQ", data, dyn_phdr_off
    )
    p_filesz += 16
    p_memsz += 16
    struct.pack_into(
        "<IIQQQQQQ", data, dyn_phdr_off, p_type, p_flags, p_offset, p_vaddr, p_paddr, p_filesz, p_memsz, p_align
    )

    sh_name, sh_type, sh_flags, sh_addr, sh_offset, sh_size, sh_link, sh_info, sh_addralign, sh_entsize = struct.unpack_from(
        "<IIQQQQIIQQ", data, dynstr_sh_off
    )
    sh_size = new_strsz
    struct.pack_into(
        "<IIQQQQIIQQ",
        data,
        dynstr_sh_off,
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

    path.write_bytes(data)
    print(f"[ohos-renderer] added NEEDED {libname} to {path}")


def main() -> None:
    merge = SCRIPT_DIR / "merge-wx-graphics-renderer.sh"
    if merge.exists():
        subprocess.check_call(["bash", str(merge)])

    targets = [p for p in (LIB, BUILD_LIB) if p.exists()]
    if not targets:
        raise SystemExit("no libwx_ohosu_core found")
    for p in targets:
        patch_core(p)

    # libentry already dlopen()s libwx_ohosu_tooltip_stub.so (RTLD_GLOBAL).
    # Ship graphics renderer under that soname so constructor fills renderer slot
    # without rebuilding libentry (link currently blocked on wx OHOS vtable gaps).
    graphics = ROOT / "host/fw2-hap/entry/libs/arm64-v8a/libwx_ohos_graphics.so"
    tooltip_stub = ROOT / "host/fw2-hap/entry/libs/arm64-v8a/libwx_ohosu_tooltip_stub.so"
    if graphics.exists():
        import shutil
        shutil.copy2(graphics, tooltip_stub)
        print(f"[ohos-renderer] staged {tooltip_stub.name} <- libwx_ohos_graphics.so")


if __name__ == "__main__":
    main()
