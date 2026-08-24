#!/usr/bin/env python3
"""F-UI-3.3c: clMainFrame vtable slot fix in wxFrameBase::SetMenuBar.

Root cause: wxFrameBase hardcodes wxFrame slot offsets (#0x700/#0x778/#0x780) but
runtime object is clMainFrame (_ZTV11clMainFrame): GetMenuBar +0x6F8,
DetachMenuBar +0x770, AttachMenuBar +0x778. wxFrame +0x788/+0x790 slots are empty
on clMainFrame → SIGSEGV at typeinfo (0x7ba570).

Strategy:
  1) Inline GetMenuBar @ 0x43FE90 — ldr x0,[x0,#680]; nop (skip broken vtable blr)
  2) DetachMenuBar ldr #0x778 -> #0x770 (wxFrameBase::DetachMenuBar)
  3) AttachMenuBar ldr #0x780 -> #0x778 (wxFrameBase::AttachMenuBar)

Marker FUI33C. Restore golden before apply.
"""
from __future__ import annotations

import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
LIB = ROOT / "host/fw2-hap/entry/libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so.4.0.0"

# (site_va, orig, new, description)
FIXES: list[tuple[int, int, int, str]] = [
    # was: ldr x8,[x8,#0x700]; blr x8  -> inline m_frameMenuBar load
    (0x43FE90, 0xF9438108, 0xF9415400, "GetMenuBar inline ldr x0,[x0,#680]"),
    (0x43FE94, 0xD63F0100, 0xD503201F, "GetMenuBar skip blr -> nop"),
    (0x43FEA8, 0xF943BD08, 0xF943B908, "DetachMenuBar ldr #0x778->#0x770"),
    (0x43FEC0, 0xF943C102, 0xF943BD02, "AttachMenuBar ldr #0x780->#0x778"),
]

# Prior trial encodings (wxFrame +0x10 drift — wrong for clMainFrame)
_PRIOR: dict[int, int] = {
    0x43FE90: 0xF9438908,  # GetMenuBar ldr #0x710 only
    0x43FEA8: 0xF943C508,  # Detach #0x788
    0x43FEC0: 0xF943C902,  # Attach #0x790
}

MARKER_VA = 0x6F8448
MARKER = b"FUI33C\x00"
NOP = 0xD503201F


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
    marker_off = va_to_offset(data, MARKER_VA)

    for site_va, orig, new, desc in FIXES:
        site_off = va_to_offset(data, site_va)
        cur = struct.unpack_from("<I", data, site_off)[0]
        if cur == new:
            print(f"[f-ui-3.3c] skip {desc} @ {site_va:#x}")
            continue
        # allow re-patch from prior vtable-offset-only fix
        if cur == new:
            continue
        if cur != orig and cur not in (_PRIOR.get(site_va, orig),):
            if site_va == 0x43FE94 and cur in (NOP, 0xD503201F):
                print(f"[f-ui-3.3c] skip {desc} @ {site_va:#x}")
                continue
            raise SystemExit(
                f"[f-ui-3.3c] unexpected @ {site_va:#x}: {cur:#x} expected {orig:#x} ({desc})"
            )
        struct.pack_into("<I", data, site_off, new)
        print(f"[f-ui-3.3c] {desc} @ {site_va:#x} ({new:#x})")

    if MARKER not in data:
        data[marker_off : marker_off + len(MARKER)] = MARKER

    path.write_bytes(data)
    print(f"[f-ui-3.3c] patched {path}")


def main() -> None:
    patch_core(LIB)


if __name__ == "__main__":
    main()
