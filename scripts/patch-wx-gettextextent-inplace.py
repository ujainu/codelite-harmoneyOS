#!/usr/bin/env python3
"""In-place OHOS wx GetTextExtent fallback for libwx_ohosu_core."""
from __future__ import annotations

import shutil
import struct
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SCRIPT_DIR = Path(__file__).resolve().parent
LIB = ROOT / "host/fw2-hap/entry/libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so.4.0.0"
BUILD_LIB = ROOT / "build-wx-ohos-gui/lib/libwx_ohosu_core-3.3-OHOS.so.4.0.0"
STUB_SRC = SCRIPT_DIR / "gettextextent_stub.cpp"
TEXTEXT_SRC = ROOT / "third_party/wxWidgets/src/ohos/textextent.cpp"

VA_GCDC_GETTEXTEXTENT = 0x40152C
VA_GCDC_PATCH_SIZE = 0x1AC
VA_OH_LOG_PLT = 0x6F7770
NOP = b"\x1f\x20\x03\xd5"

CXX = Path(
    "/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native/llvm/bin/aarch64-unknown-linux-ohos-clang++"
)
SYSROOT = Path(
    "/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native/sysroot"
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


def pack_strings(strings: list[str]) -> tuple[bytes, list[int]]:
    blob = bytearray()
    addrs: list[int] = []
    for s in strings:
        addrs.append(len(blob))
        blob.extend(s.encode("utf-8") + b"\x00")
        while len(blob) % 8:
            blob.append(0)
    return bytes(blob), addrs


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

    def mov_reg(self, dst: int, src: int) -> None:
        self.emit(0xAA0003E0 | (src << 16) | dst)

    def mov_w(self, dst: int, imm16: int) -> None:
        self.emit(0x52800000 | ((imm16 & 0xFFFF) << 5) | dst)

    def cbz_x(self, reg: int, target: int) -> None:
        off = (target - self.pc) // 4
        self.emit(0xB4000000 | ((off & 0x7FFFF) << 5) | reg)

    def adrp(self, rd: int, va: int) -> None:
        page = va & ~0xFFF
        imm = ((page >> 12) - (self.pc >> 12)) & 0x1FFFFFF
        immlo = (imm & 0x3) << 29
        immhi = (imm >> 2) << 5
        self.emit(0x90000000 | immlo | immhi | rd)

    def add_lo12(self, rd: int, rn: int, va: int) -> None:
        off = va & 0xFFF
        self.emit(0x91000000 | (off << 10) | (rn << 5) | rd)

    def bl(self, target: int) -> None:
        off = (target - self.pc) // 4
        self.emit(0x94000000 | (off & 0x03FFFFFF))

    def hilog(self, msg_va: int, fmt_va: int, tag_va: int, width_reg: int | None = None) -> None:
        self.mov_w(0, 0)
        self.mov_w(1, 4)
        self.emit(0x52800002)
        self.emit(0x72A01E02)
        self.adrp(3, tag_va)
        self.add_lo12(3, 3, tag_va)
        self.adrp(4, fmt_va)
        self.add_lo12(4, 4, fmt_va)
        self.adrp(5, msg_va)
        self.add_lo12(5, 5, msg_va)
        if width_reg is not None:
            self.emit(0x2A0003E0 | (width_reg << 16) | 6)
            self.mov_w(7, 16)
        self.bl(VA_OH_LOG_PLT)


def build_gettextextent_patch() -> bytes:
    strings = [
        "[FULL_UI] OHOS GetTextExtent enter",
        "[FULL_UI] OHOS GetTextExtent return width=%{public}d height=%{public}d",
        "%{public}s",
        "wxOHOS",
    ]
    str_blob, str_addrs = pack_strings(strings)
    code_va = VA_GCDC_GETTEXTEXTENT
    str_base = code_va + 0x120
    enter_va = str_base + str_addrs[0]
    ret_va = str_base + str_addrs[1]
    fmt_va = str_base + str_addrs[2]
    tag_va = str_base + str_addrs[3]

    a = A64(code_va)
    a.emit(0xA9BA7BFD)
    a.emit(0xA9016FFC)
    a.emit(0xA90267FA)
    a.emit(0xA9035FF8)
    a.emit(0xA90457F6)
    a.emit(0xA9054FF4)
    a.emit(0x910003FD)
    a.mov_reg(19, 1)
    a.mov_reg(20, 2)
    a.mov_reg(21, 3)
    a.mov_reg(22, 4)
    a.mov_reg(23, 5)
    a.hilog(enter_va, fmt_va, tag_va)

    a.emit(0x39400268)  # ldrb w8, [x19]
    a.emit(0xB9400A69)  # ldr w9, [x19, #8]
    a.emit(0xD341FD0B)  # lsr x11, x8, #1
    a.emit(0x7200011F)  # tst w8, #1
    a.emit(0x1A890178)  # csel w24, w11, w9, eq
    a.mov_w(10, 8)
    a.emit(0x1B0A7F08)  # mul w8, w24, w10
    a.mov_w(9, 16)

    a.cbz_x(20, a.pc + 8)
    a.emit(0xB9000288)  # str w8, [x20]
    a.cbz_x(21, a.pc + 8)
    a.emit(0xB90002A9)  # str w9, [x21]
    a.cbz_x(22, a.pc + 8)
    a.emit(0xB90002DF)  # str wzr, [x22]
    a.cbz_x(23, a.pc + 8)
    a.emit(0xB90002FF)  # str wzr, [x23]

    a.hilog(ret_va, fmt_va, tag_va, width_reg=8)
    a.emit(0xA9454FF4)
    a.emit(0xA94457F6)
    a.emit(0xA9435FF8)
    a.emit(0xA94267FA)
    a.emit(0xA9416FFC)
    a.emit(0xA8C67BFD)
    a.emit(0xD65F03C0)

    code = bytearray(a.bytes())
    if len(code) > 0x120:
        raise SystemExit(f"GetTextExtent code overflow: {len(code)} > 0x120")
    code.extend(b"\x00" * (0x120 - len(code)))
    code.extend(str_blob)
    if len(code) > VA_GCDC_PATCH_SIZE:
        raise SystemExit(f"GetTextExtent patch overflow: {len(code)} > {VA_GCDC_PATCH_SIZE}")
    code.extend(NOP * ((VA_GCDC_PATCH_SIZE - len(code)) // 4))
    return bytes(code)


def ensure_stub_built() -> None:
    stub_so = SCRIPT_DIR / "gettextextent_stub.so"
    if stub_so.exists() and stub_so.stat().st_mtime >= max(
        STUB_SRC.stat().st_mtime, TEXTEXT_SRC.stat().st_mtime
    ):
        return
    tmp = SCRIPT_DIR / "gettextextent_stub.build"
    tmp.mkdir(exist_ok=True)
    common = [
        str(CXX),
        "--target=aarch64-linux-ohos",
        f"--sysroot={SYSROOT}",
        "-c",
        f"-I{ROOT}/build-wx-ohos-gui/lib/wx/include/ohos-unicode-3.3",
        f"-I{ROOT}/third_party/wxWidgets/include",
        "-D__WXOHOS__",
        "-DwxUSE_GUI=1",
        "-DwxUSE_BASE=1",
    ]
    subprocess.check_call(common + [str(TEXTEXT_SRC), "-o", str(tmp / "textextent.o")])
    subprocess.check_call(common + [str(STUB_SRC), "-o", str(tmp / "stub.o")])
    subprocess.check_call(
        [
            str(CXX),
            "--target=aarch64-linux-ohos",
            f"--sysroot={SYSROOT}",
            "-shared",
            "-o",
            str(stub_so),
            str(tmp / "stub.o"),
            str(tmp / "textextent.o"),
            "-lhilog_ndk.z",
        ]
    )


def patch_core(path: Path) -> None:
    patch = build_gettextextent_patch()
    data = bytearray(path.read_bytes())
    off = va_to_offset(data, VA_GCDC_GETTEXTEXTENT)
    data[off : off + len(patch)] = patch
    path.write_bytes(data)
    print(f"[gettextextent-patch] patched GCDC DoGetTextExtent @ 0x{VA_GCDC_GETTEXTEXTENT:x} ({len(patch)} bytes)")


def main() -> None:
    ensure_stub_built()
    targets = [p for p in (LIB, BUILD_LIB) if p.exists()]
    if not targets:
        raise SystemExit("no libwx_ohosu_core found")
    for p in targets:
        bak = p.with_suffix(p.suffix + ".bak-gettextextent-patch")
        if not bak.exists():
            shutil.copy2(p, bak)
        patch_core(p)


if __name__ == "__main__":
    main()
