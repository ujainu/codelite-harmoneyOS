#!/usr/bin/env python3
"""Patch wx + libcodelite for OHOS UI boot (StaticText, StatusBar, icons, centre)."""
from __future__ import annotations

import shutil
import struct
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
SCRIPT_DIR = Path(__file__).resolve().parent
LIB = ROOT / "host/fw2-hap/entry/libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so.4.0.0"
BASELINE = LIB.with_suffix(LIB.suffix + ".bak-tooltip-patch")
BUILD_LIB = ROOT / "build-wx-ohos-gui/lib/libwx_ohosu_core-3.3-OHOS.so.4.0.0"
BASE_LIB = ROOT / "host/fw2-hap/entry/libs/arm64-v8a/libwx_baseu-3.3-OHOS.so.4.0.0"
BUILD_BASE = ROOT / "build-wx-ohos-gui/lib/libwx_baseu-3.3-OHOS.so.4.0.0"
CL_LIB = ROOT / "host/fw2-hap/entry/libs/arm64-v8a/libcodelite_app.so"

DOGETBEST = bytes.fromhex("0810805213028052137d60b3e00313aac0035fd6")
MEASURE = bytes.fromhex("0810805213028052287d60b3e00308aa420000b4490000b9c0035fd6")
NOP = b"\x1f\x20\x03\xd5"
RET = b"\xc0\x03\x5f\xd6"

VA_DOGETBEST = 0x538700
VA_MEASURE = 0x518500
VA_AUTORESIZE = 0x4BCAE8
VA_ASSERT_VARIANT = 0x4DC460
VA_TRAP_VARIANT = 0x4DC478
VA_SETLABEL_SKIP_MARKUP_DEL = 0x5388E0
VA_VARIANT_DEFAULT = 0x4DC42C
VARIANT_DEFAULT_SKIP = bytes.fromhex("31000014")
VA_WX_ON_ASSERT = 0x129BD8
VA_SHOW_ASSERT = 0x12C844
SHOW_ASSERT_STUB = bytes.fromhex("00008052c0035fd6")
VA_SET_ICONS_CL = 0x404494
VA_DOCENTRE = 0x4D2064

VA_SB_CREATE = 0x64BBE4
VA_SB_CREATE_SIZE = 0xEC
VA_SB_INITCOLOURS = 0x64C9C0
VA_SB_ONPAINT = 0x64B728
VA_SB_DOUPDATE = 0x64BDA0
VA_SB_SETMIN = 0x64CB04
VA_SB_GETFIELD = 0x64C538
VA_SB_DOGETBEST = 0x64BCD4
VA_CREATE_CONTROL = 0x3F261C
VA_SET_FIELDS = 0x4B9504
VA_CREATEBASE = 0x4DA49C
VA_CREATEBASE_SIZE = 0x1A4
VA_RESERVE_ID = 0x4E3EB0
VA_OH_LOG_PLT = 0x6F7770
VA_WX_GLOBAL_PAGE = 0x789000
OFF_WX_DEFAULT_POS = 3704
OFF_WX_DEFAULT_SIZE = 3520
OFF_WX_DEFAULT_VALIDATOR = 3920

PATCH_LEN = 64
CXX = Path(
    "/Applications/DevEco-Studio.app/Contents/sdk/default/openharmony/native/llvm/bin/aarch64-unknown-linux-ohos-clang++"
)
OBJDUMP = CXX.parent / "llvm-objdump"


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

    def mov_x(self, dst: int, src_w: int) -> None:
        self.emit(0x2A0003E0 | (src_w << 16) | dst)

    def orr_w(self, dst: int, src1: int, src2: int) -> None:
        self.emit(0x2A000000 | (src2 << 16) | (src1 << 5) | dst)

    def cbz_w(self, reg: int, target: int) -> None:
        off = (target - self.pc) // 4
        self.emit(0x34000000 | (off << 5) | reg)

    def adrp(self, rd: int, va: int) -> None:
        page = va & ~0xFFF
        imm = ((page >> 12) - (self.pc >> 12)) & 0x1FFFFFF
        immlo = (imm & 0x3) << 29
        immhi = (imm >> 2) << 5
        self.emit(0x90000000 | immlo | immhi | rd)

    def add_lo12(self, rd: int, rn: int, va: int) -> None:
        off = va & 0xFFF
        self.emit(0x91000000 | (off << 10) | (rn << 5) | rd)

    def ldr_u64(self, rt: int, rn: int, off: int) -> None:
        assert off % 8 == 0
        self.emit(0xF9400000 | ((off // 8) << 10) | (rn << 5) | rt)

    def str_w(self, rt: int, rn: int, off: int) -> None:
        assert off % 4 == 0
        self.emit(0xB9000000 | ((off // 4) << 10) | (rn << 5) | rt)

    def str_x64(self, rt: int, rn: int, off: int) -> None:
        assert off % 8 == 0
        self.emit(0xF9000000 | ((off // 8) << 10) | (rn << 5) | rt)

    def b_eq(self, target: int) -> None:
        off = (target - self.pc) // 4
        self.emit(0x54000000 | ((off & 0x7FFFF) << 5))

    def b(self, target: int) -> None:
        off = (target - self.pc) // 4
        self.emit(0x14000000 | (off & 0x03FFFFFF))

    def bl(self, target: int) -> None:
        off = (target - self.pc) // 4
        self.emit(0x94000000 | (off & 0x03FFFFFF))

    def hilog(self, msg_va: int, fmt_va: int, tag_va: int) -> None:
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
        self.bl(VA_OH_LOG_PLT)


def pack_strings(strings: list[str]) -> tuple[bytes, list[int]]:
    blob = bytearray()
    addrs: list[int] = []
    for s in strings:
        addrs.append(len(blob))
        blob.extend(s.encode("utf-8") + b"\x00")
        while len(blob) % 8:
            blob.append(0)
    return bytes(blob), addrs


def build_hilog_ret_stub(base_va: int, message: str) -> bytes:
    str_blob, str_addrs = pack_strings([message, "%{public}s", "wxOHOS"])
    a = A64(base_va)
    msg_va = base_va + 0x28 + str_addrs[0]
    fmt_va = base_va + 0x28 + str_addrs[1]
    tag_va = base_va + 0x28 + str_addrs[2]
    a.hilog(msg_va, fmt_va, tag_va)
    a.emit(0xD65F03C0)
    code = bytearray(a.bytes())
    while len(code) % 8:
        code.append(0)
    code.extend(str_blob)
    code.extend(NOP * ((PATCH_LEN - len(code)) // 4))
    return bytes(code)


def build_createbase_stub() -> bytes:
    """Minimal wxWindowBase::CreateBase — direct member init, no virtual SetMinSize/SetName."""
    strings = [
        "[FULL_UI] wxWindowBase CreateBase OHOS enter",
        "[FULL_UI] CreateBase OHOS OK",
        "%{public}s",
        "wxOHOS",
    ]
    str_blob, str_addrs = pack_strings(strings)

    a = A64(VA_CREATEBASE)
    str_base = VA_CREATEBASE + 0x70
    enter_va = str_base + str_addrs[0]
    ok_va = str_base + str_addrs[1]
    fmt_va = str_base + str_addrs[2]
    tag_va = str_base + str_addrs[3]

    a.emit(0xA9BC7BFD)  # stp x29, x30, [sp, #-64]!
    a.emit(0xA9024FF4)  # stp x20, x19, [sp, #32]
    a.emit(0xA90157F6)  # stp x22, x21, [sp, #16]  — save caller x21/x22 (size/pos refs)
    a.emit(0x910003FD)
    a.mov_reg(19, 0)    # this
    a.mov_reg(20, 1)    # parent
    a.mov_x(9, 2)       # w9 = w2 (id) — caller-saved; do not use x24 (TLW keeps parent)
    a.mov_reg(21, 5)    # style in callee-saved x21
    a.hilog(enter_va, fmt_va, tag_va)
    a.emit(0x3100053F)  # cmn w9, #1  (id == wxID_ANY)

    skip_reserve_idx = len(a.insns)
    a.emit(0xD503201F)  # b.eq placeholder
    a.str_w(9, 19, 84)
    skip_done_idx = len(a.insns)
    a.emit(0xD503201F)  # b placeholder

    reserve_pc = a.pc
    a.mov_x(0, 9)       # ReserveId(id)
    a.bl(VA_RESERVE_ID)
    a.str_w(0, 19, 84)

    done_pc = a.pc
    a.str_x64(20, 19, 88)
    a.str_x64(21, 19, 336)
    a.hilog(ok_va, fmt_va, tag_va)
    a.mov_w(0, 1)
    a.emit(0xA9424FF4)  # ldp x20, x19, [sp, #32]
    a.emit(0xA94157F6)  # ldp x22, x21, [sp, #16]
    a.emit(0xA8C47BFD)  # ldp x29, x30, [sp], #64
    a.emit(0xD65F03C0)

    off_eq = (reserve_pc - (VA_CREATEBASE + skip_reserve_idx * 4)) // 4
    a.insns[skip_reserve_idx] = 0x54000000 | ((off_eq & 0x7FFFF) << 5)
    off_b = (done_pc - (VA_CREATEBASE + skip_done_idx * 4)) // 4
    a.insns[skip_done_idx] = 0x14000000 | (off_b & 0x03FFFFFF)

    code = bytearray(a.bytes())
    while len(code) % 8:
        code.append(0)
    code.extend(str_blob)
    if len(code) > VA_CREATEBASE_SIZE:
        raise SystemExit(f"CreateBase stub overflow: {len(code)} > {VA_CREATEBASE_SIZE}")
    code.extend(NOP * ((VA_CREATEBASE_SIZE - len(code)) // 4))
    return bytes(code)


def build_statusbar_create() -> bytes:
    """Create via CreateControl + SetFieldsCount; boot logs via InitColours/OnPaint stubs."""
    a = A64(VA_SB_CREATE)
    fail_pc = VA_SB_CREATE + 0x8C

    a.emit(0xA9BD7BFD)
    a.emit(0xA9024FF4)
    a.emit(0x910003FD)
    a.mov_reg(19, 0)
    a.mov_reg(20, 1)
    a.mov_x(21, 2)
    a.mov_x(22, 3)
    a.mov_reg(23, 4)
    a.emit(0x52A00128)
    a.orr_w(22, 22, 8)
    a.adrp(3, VA_WX_GLOBAL_PAGE)
    a.adrp(4, VA_WX_GLOBAL_PAGE)
    a.ldr_u64(3, 3, OFF_WX_DEFAULT_POS)
    a.ldr_u64(4, 4, OFF_WX_DEFAULT_SIZE)
    a.adrp(6, VA_WX_GLOBAL_PAGE)
    a.ldr_u64(6, 6, OFF_WX_DEFAULT_VALIDATOR)
    a.mov_reg(0, 19)
    a.mov_reg(1, 20)
    a.mov_x(2, 21)
    a.mov_reg(5, 22)
    a.mov_reg(7, 23)
    a.bl(VA_CREATE_CONTROL)
    a.cbz_w(0, fail_pc)
    a.mov_w(8, 22)
    a.str_w(8, 19, 596)
    a.mov_reg(0, 19)
    a.mov_w(1, 1)
    a.emit(0xAA1F03E2)
    a.bl(VA_SET_FIELDS)
    a.bl(VA_SB_INITCOLOURS)
    a.bl(VA_SB_ONPAINT)
    a.mov_w(0, 1)
    a.emit(0xA9424FF4)
    a.emit(0xA8C37BFD)
    a.emit(0xD65F03C0)
    while a.pc < fail_pc:
        a.emit(0xD503201F)
    a.mov_w(0, 0)
    a.emit(0xA9424FF4)
    a.emit(0xA8C37BFD)
    a.emit(0xD65F03C0)

    code = bytearray(a.bytes())
    if len(code) > VA_SB_CREATE_SIZE:
        raise SystemExit(f"statusbar Create stub overflow: {len(code)} > {VA_SB_CREATE_SIZE}")
    code.extend(NOP * ((VA_SB_CREATE_SIZE - len(code)) // 4))
    return bytes(code)


def build_do_update_stub() -> bytes:
    str_blob, str_addrs = pack_strings(
        ["[FULL_UI] wxStatusBar SetStatusText OK", "%{public}s", "wxOHOS"]
    )
    a = A64(VA_SB_DOUPDATE)
    msg_va = VA_SB_DOUPDATE + 0x30 + str_addrs[0]
    fmt_va = VA_SB_DOUPDATE + 0x30 + str_addrs[1]
    tag_va = VA_SB_DOUPDATE + 0x30 + str_addrs[2]
    a.hilog(msg_va, fmt_va, tag_va)
    a.emit(0xD65F03C0)
    code = bytearray(a.bytes())
    while len(code) % 8:
        code.append(0)
    code.extend(str_blob)
    code.extend(NOP * ((PATCH_LEN - len(code)) // 4))
    return bytes(code)


def extract_symbol_bytes(obj: Path, sym: str) -> bytes:
    res = subprocess.run(
        [str(OBJDUMP), "-d", f"--disassemble-symbols={sym}", str(obj)],
        capture_output=True,
        text=True,
        check=True,
    )
    insns: list[int] = []
    for ln in res.stdout.splitlines():
        ln = ln.strip()
        if not ln or ln.endswith(">:") or ln.startswith("/") or ln.startswith("Disassembly"):
            continue
        if ":" not in ln:
            continue
        parts = ln.split(":", 1)[1].strip().split()
        if parts and len(parts[0]) == 8:
            insns.append(int(parts[0], 16))
    return struct.pack("<" + "I" * len(insns), *insns)


def compile_statusbar_stubs() -> dict[str, bytes]:
    src = SCRIPT_DIR / "statusbar_stub.cpp"
    with tempfile.TemporaryDirectory() as td:
        obj = Path(td) / "statusbar_stub.o"
        subprocess.check_call([str(CXX), "--target=aarch64-linux-ohos", "-c", str(src), "-o", str(obj)])
        return {
            "SetMinHeight": extract_symbol_bytes(obj, "_ZN11wxStatusBar12SetMinHeightEi"),
            "GetFieldRect": extract_symbol_bytes(obj, "_ZNK11wxStatusBar12GetFieldRectEiR6wxRect"),
            "DoGetBestClientSize": extract_symbol_bytes(obj, "_ZNK11wxStatusBar19DoGetBestClientSizeEv"),
        }


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


def write_at(data: bytearray, va: int, blob: bytes) -> None:
    off = va_to_offset(data, va)
    data[off : off + len(blob)] = blob


def write_stub(data: bytearray, va: int, stub: bytes) -> None:
    off = va_to_offset(data, va)
    patch = stub + NOP * ((PATCH_LEN - len(stub)) // 4)
    data[off : off + len(patch)] = patch


def patch_core(path: Path) -> None:
    import os

    if BASELINE.exists() and os.environ.get("WX_SKIP_BASELINE_COPY") != "1":
        shutil.copy2(BASELINE, path)
    stubs = compile_statusbar_stubs()
    data = bytearray(path.read_bytes())
    write_at(data, VA_CREATEBASE, build_createbase_stub())
    write_at(data, VA_SB_CREATE, build_statusbar_create())
    write_at(data, VA_SB_DOUPDATE, build_do_update_stub())
    write_at(data, VA_SB_INITCOLOURS, build_hilog_ret_stub(
        VA_SB_INITCOLOURS, "[FULL_UI] wxStatusBar SetFieldsCount OK"))
    write_at(data, VA_SB_ONPAINT, build_hilog_ret_stub(
        VA_SB_ONPAINT, "[FULL_UI] wxStatusBar Create OK"))
    write_stub(data, VA_SB_SETMIN, stubs["SetMinHeight"])
    write_stub(data, VA_SB_GETFIELD, stubs["GetFieldRect"])
    write_stub(data, VA_SB_DOGETBEST, stubs["DoGetBestClientSize"])
    write_stub(data, VA_DOGETBEST, DOGETBEST)
    write_stub(data, 0x4DB6BC, DOGETBEST)  # wxWindowBase::GetBestSize boot stub
    write_stub(data, VA_MEASURE, MEASURE)
    write_stub(data, VA_AUTORESIZE, RET)
    write_stub(data, VA_ASSERT_VARIANT, NOP)
    write_stub(data, VA_TRAP_VARIANT, NOP)
    write_stub(data, VA_SETLABEL_SKIP_MARKUP_DEL, bytes.fromhex("05000014"))
    off = va_to_offset(data, VA_VARIANT_DEFAULT)
    data[off : off + 4] = VARIANT_DEFAULT_SKIP
    off = va_to_offset(data, VA_DOCENTRE)
    data[off : off + 4] = RET
    off = va_to_offset(data, 0x53883C)
    data[off : off + 4] = RET  # wxGenericStaticText::SetLabel boot no-op
    off = va_to_offset(data, 0x65CCF0)
    data[off : off + 4] = NOP   # wxStaticText::Create skip SetLabel vcall
    path.write_bytes(data)
    print(f"[ui-boot-patch] patched core + createbase + statusbar {path}")


def patch_base(path: Path) -> None:
    bak = path.with_suffix(path.suffix + ".bak-stattext-patch")
    if not bak.exists():
        shutil.copy2(path, bak)
    data = bytearray(path.read_bytes())
    write_stub(data, VA_WX_ON_ASSERT, RET)
    write_stub(data, VA_SHOW_ASSERT, SHOW_ASSERT_STUB)
    path.write_bytes(data)
    print(f"[ui-boot-patch] patched base {path}")


def patch_cl(path: Path) -> None:
    bak = path.with_suffix(path.suffix + ".bak-ui-boot-patch")
    if not bak.exists():
        shutil.copy2(path, bak)
    data = bytearray(path.read_bytes())
    write_stub(data, VA_SET_ICONS_CL, RET)
    path.write_bytes(data)
    print(f"[ui-boot-patch] patched codelite {path}")


def main() -> None:
    core_targets = [p for p in (LIB, BUILD_LIB) if p.exists()]
    base_targets = [p for p in (BASE_LIB, BUILD_BASE) if p.exists()]
    if not core_targets:
        raise SystemExit("no libwx_ohosu_core found")
    for p in core_targets:
        patch_core(p)
    for p in base_targets:
        patch_base(p)
    if CL_LIB.exists():
        patch_cl(CL_LIB)
    else:
        print(f"[ui-boot-patch] skip libcodelite_app (missing {CL_LIB})")

    import os

    fui_trace = SCRIPT_DIR / "patch-wx-fui-trace-dispatch.py"
    renderer = SCRIPT_DIR / "patch-wx-ohos-renderer-inplace.py"
    pen = SCRIPT_DIR / "patch-wx-ohos-pen-inplace.py"
    gettextextent = SCRIPT_DIR / "patch-wx-gettextextent-inplace.py"
    if os.environ.get("F_UI_1_DISPATCH_TRACE") == "1" and fui_trace.exists():
        subprocess.check_call([sys.executable, str(fui_trace)])
    elif os.environ.get("F_UI_1_SKIP_RENDERER") != "1" and renderer.exists():
        subprocess.check_call([sys.executable, str(renderer)])
        if pen.exists():
            subprocess.check_call([sys.executable, str(pen)])
    elif gettextextent.exists():
        subprocess.check_call([sys.executable, str(gettextextent)])


if __name__ == "__main__":
    main()
