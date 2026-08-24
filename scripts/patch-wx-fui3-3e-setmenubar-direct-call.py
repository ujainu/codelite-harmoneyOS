#!/usr/bin/env python3
"""F-UI-3.3e: wxFrameBase::SetMenuBar direct-call fix (binary equivalent of ohos/frame.cpp).

clMainFrame vtable dispatch for DetachMenuBar/AttachMenuBar hangs inside
wxFrameBase::SetMenuBar even after F-UI-3.3c GetMenuBar inline fix.

Strategy (wxFrameBase::SetMenuBar @ 0x43FE78):
  1) Inline GetMenuBar — ldr x0,[x0,#680]; nop (same as 3.3c)
  2) Detach — bl wxFrameBase::DetachMenuBar @ 0x43FE08 (non-virtual)
  3) Attach — bl wxFrameBase::AttachMenuBar @ 0x43FE3c + normal epilogue

Never place executable stubs in the PLT tail (0x6F7800+) — corrupts GOT/PLT.
Verification uses existing [R-4] MenuBar Attach logs from menu.cpp.

Apply to pristine build lib (no 3.3c/3.3d trace markers). Marker FUI33E.
"""
from __future__ import annotations

import shutil
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
BUILD_LIB = ROOT / "build-wx-ohos-gui/lib/libwx_ohosu_core-3.3-OHOS.so.4.0.0"
HAP_LIB = ROOT / "host/fw2-hap/entry/libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so.4.0.0"

MARKER_VA = 0x6F8450
MARKER = b"FUI33E\x00"
NOP = 0xD503201F

# (site_va, orig, new, desc) — orig from pristine build lib
FIXES: list[tuple[int, int, int, str]] = [
    (0x43FE90, 0xF9438108, 0xF9415400, "GetMenuBar inline ldr x0,[x0,#680]"),
    (0x43FE94, 0xD63F0100, NOP, "GetMenuBar skip virtual blr"),
    (0x43FEA0, 0xF9400288, 0xAA1403E0, "Detach prep mov x0,x20"),
    (0x43FEA4, 0xAA1403E0, 0x97FFFFD9, "Detach direct bl wxFrameBase::DetachMenuBar"),
    (0x43FEA8, 0xF943BD08, NOP, "Detach nop (was vtable ldr)"),
    (0x43FEAC, 0xD63F0100, NOP, "Detach nop (was blr)"),
    (0x43FEB0, 0xF9400288, 0xAA1403E0, "Attach prep mov x0,x20"),
    (0x43FEB4, 0xAA1403E0, 0xAA1303E1, "Attach mov x1,x19"),
    (0x43FEB8, 0xAA1303E1, 0x97FFFFE1, "Attach direct bl wxFrameBase::AttachMenuBar"),
    (0x43FEBC, 0xA9414FF4, 0xA9414FF4, "Attach epilogue ldp x20,x19"),
    (0x43FEC0, 0xF943C102, 0xA8C27BFD, "Attach epilogue ldp x29,x30"),
    (0x43FEC4, 0xA8C27BFD, 0xD65F03C0, "Attach ret"),
    (0x43FEC8, 0xD61F0040, NOP, "Attach nop (was br x2)"),
]

_PRIOR: dict[int, set[int]] = {
    0x43FE90: {0xF9438108, 0xF9415400},
    0x43FE94: {0xD63F0100, NOP},
    0x43FEA8: {0xF943BD08, 0xF943B908},
    0x43FEC0: {0xF943C102, 0xF943BD02, 0xA8C27BFD},
    0x43FEBC: {0xA9414FF4, 0x94000000},  # allow re-patch from log-stub mistake
}


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


def patch_core(path: Path) -> None:
    if not path.exists():
        raise SystemExit(f"missing {path}")
    data = bytearray(path.read_bytes())

    if MARKER in data:
        print(f"[f-ui-3.3e] already patched {path}")
        return

    for site_va, orig, new, desc in FIXES:
        site_off = va_to_offset(data, site_va)
        cur = struct.unpack_from("<I", data, site_off)[0]
        if cur == new:
            print(f"[f-ui-3.3e] skip {desc} @ {site_va:#x}")
            continue
        allowed = _PRIOR.get(site_va, {orig})
        allowed.add(orig)
        if cur not in allowed and cur != new:
            raise SystemExit(
                f"[f-ui-3.3e] unexpected @ {site_va:#x}: {cur:#010x} expected one of {allowed} ({desc})"
            )
        struct.pack_into("<I", data, site_off, new)
        print(f"[f-ui-3.3e] {desc} @ {site_va:#x} ({new:#010x})")

    marker_off = va_to_offset(data, MARKER_VA)
    data[marker_off : marker_off + len(MARKER)] = MARKER
    path.write_bytes(data)
    print(f"[f-ui-3.3e] patched {path}")


def main() -> None:
    HAP_LIB.parent.mkdir(parents=True, exist_ok=True)
    if not BUILD_LIB.exists():
        raise SystemExit(f"missing pristine build lib {BUILD_LIB}")
    shutil.copy2(BUILD_LIB, HAP_LIB)
    print(f"[f-ui-3.3e] staged pristine lib from {BUILD_LIB}")
    patch_core(HAP_LIB)


if __name__ == "__main__":
    main()
