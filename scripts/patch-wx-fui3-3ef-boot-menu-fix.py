#!/usr/bin/env python3
"""F-UI-3.3e + 3.4: boot menu path binary fixes for libwx_ohosu_core.

3.3e — wxFrameBase::SetMenuBar direct Detach/Attach calls (clMainFrame vtable).
3.4  — wxMenuBar::Attach SetSize skips parent GetClientAreaOrigin;
         wxFrameBase::GetClientAreaOrigin inline GetToolBar member;
         wxFrameBase::DoGiveHelp inline GetStatusBar member.

Marker FUI33E + FUI34A. Apply to pristine build lib.
"""
from __future__ import annotations

import shutil
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
BUILD_LIB = ROOT / "build-wx-ohos-gui/lib/libwx_ohosu_core-3.3-OHOS.so.4.0.0"
HAP_LIB = ROOT / "host/fw2-hap/entry/libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so.4.0.0"

# Exactly 8 bytes at 0x6F8450 only — slots are 8 bytes apart in PLT NOP tail.
# Never write 7-byte + NUL markers at 0x6F8450/0x6F8458 (corrupts adjacent NOPs).
MARKER_EF = b"FUI33EF"
MARKER_EF_VA = 0x6F8450
LEGACY_MARKERS = (b"FUI33E\x00", b"FUI34A\x00")
NOP = 0xD503201F
NOP_BYTES = struct.pack("<I", NOP)

FIXES: list[tuple[int, int, int, str]] = [
    # --- F-UI-3.3e SetMenuBar ---
    (0x43FE90, 0xF9438108, 0xF9415400, "SetMenuBar GetMenuBar inline"),
    (0x43FE94, 0xD63F0100, NOP, "SetMenuBar skip GetMenuBar blr"),
    (0x43FEA0, 0xF9400288, 0xAA1403E0, "SetMenuBar Detach mov x0,x20"),
    (0x43FEA4, 0xAA1403E0, 0x97FFFFD9, "SetMenuBar Detach bl"),
    (0x43FEA8, 0xF943BD08, NOP, "SetMenuBar Detach nop ldr"),
    (0x43FEAC, 0xD63F0100, NOP, "SetMenuBar Detach nop blr"),
    (0x43FEB0, 0xF9400288, 0xAA1403E0, "SetMenuBar Attach mov x0,x20"),
    (0x43FEB4, 0xAA1403E0, 0xAA1303E1, "SetMenuBar Attach mov x1,x19"),
    (0x43FEB8, 0xAA1303E1, 0x97FFFFE1, "SetMenuBar Attach bl"),
    (0x43FEBC, 0xA9414FF4, 0xA9414FF4, "SetMenuBar epilogue ldp"),
    (0x43FEC0, 0xF943C102, 0xA8C27BFD, "SetMenuBar epilogue ldp fp"),
    (0x43FEC4, 0xA8C27BFD, 0xD65F03C0, "SetMenuBar ret"),
    (0x43FEC8, 0xD61F0040, NOP, "SetMenuBar nop br"),
    # --- F-UI-3.4 Attach / client origin ---
    (0x6596DC, 0x52800065, 0x528001E5, "MenuBar Attach SetSize NO_ADJUSTMENTS flags"),
    (0x43EED8, 0xAA1403E0, 0xF9418A80, "GetClientAreaOrigin inline m_frameToolBar"),
    (0x43EEDC, 0xF943A908, NOP, "GetClientAreaOrigin nop vtable ldr"),
    (0x43EEE0, 0xD63F0100, NOP, "GetClientAreaOrigin nop GetToolBar blr"),
    (0x43F99C, 0xF9439108, 0xF9418260, "DoGiveHelp inline m_frameStatusBar"),
    (0x43F9A0, 0xD63F0100, NOP, "DoGiveHelp nop GetStatusBar blr"),
]

_PRIOR: dict[int, set[int]] = {
    0x43FE90: {0xF9438108, 0xF9415400},
    0x43FE94: {0xD63F0100, NOP},
    0x43FEA8: {0xF943BD08, 0xF943B908},
    0x43FEC0: {0xF943C102, 0xF943BD02, 0xA8C27BFD},
    0x43FEBC: {0xA9414FF4, 0x94000000},
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
    has_ef = MARKER_EF in data
    if has_ef:
        print(f"[f-ui-3.3ef] already patched {path}")
        return

    for site_va, orig, new, desc in FIXES:
        site_off = va_to_offset(data, site_va)
        cur = struct.unpack_from("<I", data, site_off)[0]
        if cur == new:
            print(f"[f-ui-3.3ef] skip {desc} @ {site_va:#x}")
            continue
        allowed = _PRIOR.get(site_va, {orig})
        allowed.add(orig)
        if cur not in allowed and cur != new:
            raise SystemExit(
                f"[f-ui-3.3ef] unexpected @ {site_va:#x}: {cur:#010x} expected {allowed} ({desc})"
            )
        struct.pack_into("<I", data, site_off, new)
        print(f"[f-ui-3.3ef] {desc} @ {site_va:#x} ({new:#010x})")

    moff = va_to_offset(data, MARKER_EF_VA)
    data[moff : moff + len(MARKER_EF)] = MARKER_EF
    # Restore NOP slot @ +8 if a legacy 7-byte marker pair was applied earlier.
    nop_off = va_to_offset(data, MARKER_EF_VA + 8)
    if bytes(data[nop_off : nop_off + 4]) != NOP_BYTES:
        data[nop_off : nop_off + 4] = NOP_BYTES
        print(f"[f-ui-3.3ef] restored PLT NOP @ {MARKER_EF_VA + 8:#x}")
    path.write_bytes(data)
    print(f"[f-ui-3.3ef] patched {path}")


def main() -> None:
    HAP_LIB.parent.mkdir(parents=True, exist_ok=True)
    if not BUILD_LIB.exists():
        raise SystemExit(f"missing pristine build lib {BUILD_LIB}")
    import os
    if os.environ.get("WX_SKIP_PRISTINE_COPY") != "1":
        shutil.copy2(BUILD_LIB, HAP_LIB)
        print(f"[f-ui-3.3ef] staged pristine lib from {BUILD_LIB}")
    elif not HAP_LIB.exists():
        raise SystemExit(f"missing {HAP_LIB} and WX_SKIP_PRISTINE_COPY=1")
    patch_core(HAP_LIB)


if __name__ == "__main__":
    main()
