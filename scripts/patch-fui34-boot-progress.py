#!/usr/bin/env python3
"""F-UI-3.4b/c: unblock CreateGUIControls after SetMenuBar.

1) libcodelite: skip wxAuiManager::Update call @ 0x5DA51C (THREAD_BLOCK hang).
2) libwx core: wxFrameBase::SetStatusBar skip broken PositionStatusBar/Layout vcalls.
3) libcodelite: skip clStatusBar ctor + SetStatusBar virtual @ 0x5DA520 (libplugin hang).
4) libcodelite: skip CreateRecentlyOpenedWorkspacesMenu + Edit-menu FindItem @ 0x5DA55C.
5) libcodelite: skip SideBar m_mgr->Update @ 0x47C328 + ApplySavedTabOrder Update @ 0x47CFB0.
6) libwx core: wxFrameBase::OnCreateStatusBar return null (LUA LoadMenu hang).
7) libplugin: CodeLiteLUA::CleanupDynamicMenuItems immediate ret.
8) libcodelite: inline container SetSizer @ 0x5DAB14 (null sizer deref).
9) libcodelite: skip FindAndReplaceDialog ctor @ 0x5DAC94 (frame vtable crash).
10) libcodelite: skip CreateRecentlyOpenedFilesMenu @ 0x5DAD4C (GetMenuLabel crash).
11) libcodelite: skip clBuiltinTerminalPane ctor @ 0x68F3F0 (DoSetWindowVariant vtable crash).
12) libwx core: wxWindowBase::DoSetWindowVariant immediate ret @ 0x4DC3B4.
13) libcodelite: inline frame SetSizer @ 0x5DA1C8 (null GetSizer deref at Layout).
14) libcodelite: nop duplicate wxString dtors @ 0x496B1C/496B74 (OnInit epilogue double-free).

Apply after patch-wx-fui3-3ef-boot-menu-fix.py. Markers ... / FUI34TRM / FUI34DSW / FUI34FSZ / FUI34OIN.
"""
from __future__ import annotations

import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[3]
CL = ROOT / "host/fw2-hap/entry/libs/arm64-v8a/libcodelite_app.so"
WX = ROOT / "host/fw2-hap/entry/libs/arm64-v8a/libwx_ohosu_core-3.3-OHOS.so.4.0.0"
PL = ROOT / "host/fw2-hap/entry/libs/arm64-v8a/libplugin.so"

CL_FIXES: list[tuple[int, int, int, str]] = [
    (0x5DA51C, 0x940624F5, 0xD503201F, "skip wxAuiManager::Update bl"),
    # str xzr,[x19,#768]; str xzr,[x19,#2256]; b CreateRecentlyOpenedWorkspacesMenu
    (0x5DA520, 0x52807900, 0xF901827F, "skip clStatusBar clear m_frameStatusBar"),
    (0x5DA524, 0x940613A7, 0xF9046A7F, "skip clStatusBar clear m_statusBar"),
    (0x5DA528, 0xAA0003F5, 0x1400000C, "skip clStatusBar branch to menu setup"),
    (0x5DA55C, 0x940646E1, 0xD503201F, "skip CreateRecentlyOpenedWorkspacesMenu bl"),
    (0x5DA560, 0xF9444260, 0x1400001D, "skip Edit-menu FindItem slice"),
]
CL_SIDEBAR_FIXES: list[tuple[int, int, int, str]] = [
    (0x47C328, 0x940B9D72, 0xD503201F, "skip SideBar CreateGUIControls m_mgr->Update bl"),
    (0x47CFB0, 0x940B9A50, 0xD503201F, "skip SideBar ApplySavedTabOrder m_mgr->Update bl"),
]
CL_SIZER_FIXES: list[tuple[int, int, int, str]] = [
    (0x5DAB14, 0x9406123F, 0xF9009AB6, "container SetSizer -> str sizer @ [panel+304]"),
]
CL_FRAME_SIZER_FIXES: list[tuple[int, int | tuple[int, ...], int, str]] = [
    (0x5DA1C8, (0x94061492, 0xF9009A74), 0xF9009E74, "frame SetSizer -> str sizer @ [frame+312]"),
]
CL_GETSIZER_FIXES: list[tuple[int, int, int, str]] = [
    (0x5DB214, 0xF9409A74, 0xF9409E74, "CreateGUIControls GetSizer @ [frame+312]"),
    (0x5E48EC, 0xF9409A74, 0xF9409E74, "PostConstruct GetSizer @ [frame+312]"),
]
CL_ONINIT_DTOR_FIXES: list[tuple[int, int, int, str]] = [
    (0x496B1C, 0x910583E0, 0xD503201F, "OnInit skip duplicate dtor sp+352 mov"),
    (0x496B20, 0x940B229C, 0xD503201F, "OnInit skip duplicate dtor sp+352 bl"),
    (0x496B74, 0x910643E0, 0xD503201F, "OnInit skip duplicate dtor sp+400 mov"),
    (0x496B78, 0x940B2286, 0xD503201F, "OnInit skip duplicate dtor sp+400 bl"),
]
CL_GETMENULABEL_FIXES: list[tuple[int, int, int, str]] = [
    (0x5E4D58, 0xD63F0100, 0xAA1F03E0, "CompleteInitialization skip GetMenuLabel blr"),
    (0x5E6A48, 0xD63F0100, 0xAA1F03E0, "CreateRecentlyOpenedWorkspacesMenu skip GetMenuLabel blr"),
    (0x5E71D8, 0xD63F0100, 0xAA1F03E0, "CreateRecentlyOpenedFilesMenu skip GetMenuLabel blr"),
    (0x6A14F8, 0xD63F0100, 0xAA1F03E0, "PluginManager::Load skip GetMenuLabel blr"),
]
CL_INIT_COMPLETED_FIXES: list[tuple[int, int, int, str]] = [
    (0x5E5604, 0xF943CD08, 0xB0001288, "CompleteInit m_initCompleted adrp x8"),
    (0x5E5608, 0xF9400140, 0x9126C108, "CompleteInit m_initCompleted add x8,#0x9b0"),
    (0x5E5610, 0xF943A821, 0x14000227, "CompleteInit skip post-init tail → epilogue"),
]
CL_PLUGIN_LOAD_FIXES: list[tuple[int, int, int, str]] = [
    (0x6A0AEC, 0xD63F0100, 0xD503201F, "PluginManager::Load skip CreateToolBar blr"),
    (0x6A14C4, 0x9403090B, 0xD503201F, "PluginManager::Load skip wxAuiManager::Update bl"),
    (0x5E4B00, 0x94061DE0, 0xD503201F, "CompleteInitialization skip CodeLiteLUA::Initialise bl"),
    (0x5E4D20, 0xD63F0100, 0xD503201F, "CompleteInitialization skip PluginManager::Load blr"),
    (0x5E4E60, 0x9405F1FC, 0xD503201F, "CompleteInitialization skip plugins toolbar Realize bl"),
    (0x5E4E68, 0x94061D1E, 0xD503201F, "CompleteInitialization skip llm::Manager::Initialise bl"),
    (0x5E4F60, 0x9105C3E0, 0x140001A1, "CompleteInitialization skip AddPane block → pre-initCompleted"),
]
CL_FIND_FIXES: list[tuple[int, int, int, str]] = [
    (0x5DAC94, 0x52808000, 0xAA1F03F7, "skip FindAndReplaceDialog mov x23,xzr"),
    (0x5DAC98, 0x940611CA, 0x14000005, "skip FindAndReplaceDialog branch to SetFindBar"),
]
CL_MENU_FIXES: list[tuple[int, int, int, str]] = [
    (0x5DAD4C, 0x94064511, 0xD503201F, "skip CreateRecentlyOpenedFilesMenu bl"),
]
CL_TERMINAL_FIXES: list[tuple[int, int | tuple[int, ...], int, str]] = [
    (0x68F3F0, 0x5280A000, 0xF9015A9F, "skip clBuiltinTerminalPane clear m_terminal"),
    (0x68F3F4, (0x14000089, 0x94033FF3), 0x1400008A, "skip clBuiltinTerminalPane branch to SetMinSize"),
]
CL_MARKER = (0x773A00, b"FUI34BCL\x00")
CL_MARKER_C = (0x773A10, b"FUI34CSB\x00")
CL_MARKER_D = (0x773A20, b"FUI34DWS\x00")
CL_MARKER_E = (0x773A30, b"FUI34ESB\x00")
CL_MARKER_S = (0x773A40, b"FUI34SZR\x00")
CL_MARKER_FS = (0x773A80, b"FUI34FSZ\x00")
CL_MARKER_OIN = (0x773A90, b"FUI34OIN\x00")
CL_MARKER_P = (0x773AA0, b"FUI34PLM\x00")
CL_MARKER_GML = (0x773AB0, b"FUI34GML\x00")
CL_MARKER_PLD = (0x773AD0, b"FUI34PLD\x00")
CL_MARKER_CIS = (0x773AE0, b"FUI34CIS\x00")
CL_MARKER_F = (0x773A50, b"FUI34FRD\x00")
CL_MARKER_M = (0x773A60, b"FUI34RFM\x00")
CL_MARKER_T = (0x773A70, b"FUI34TRM\x00")

# tbz -> unconditional b epilogue (skip PositionStatusBar/Layout virtual dispatch)
WX_FIXES: list[tuple[int, int, int, str]] = [
    (0x43F928, 0x36000088, 0x14000002, "SetStatusBar skip PositionStatusBar/Layout"),
]
WX_OCSB_FIXES: list[tuple[int, int, int, str]] = [
    (0x43F58C, 0xA9BC7BFD, 0xAA1F03E0, "OnCreateStatusBar mov x0,xzr"),
    (0x43F590, 0xA9015FF8, 0xD65F03C0, "OnCreateStatusBar ret"),
]
WX_OCTB_FIXES: list[tuple[int, int, int, str]] = [
    (0x43FC84, 0xA9BC7BFD, 0xAA1F03E0, "OnCreateToolBar mov x0,xzr"),
    (0x43FC88, 0xF9000BF7, 0xD65F03C0, "OnCreateToolBar ret"),
]
WX_CTB_FIXES: list[tuple[int, int, int, str]] = [
    (0x43FC5C, 0xD63F0100, 0xAA1F03E0, "CreateToolBar skip OnCreateToolBar blr"),
]
WX_DSWV_FIXES: list[tuple[int, int, int, str]] = [
    (0x4DC3B4, 0xD10143FF, 0xD65F03C0, "DoSetWindowVariant immediate ret"),
]
PL_FIXES: list[tuple[int, int, int, str]] = [
    (0x6F91AC, 0xD10403FF, 0xD65F03C0, "CleanupDynamicMenuItems immediate ret"),
]
PL_FSWS_FIXES: list[tuple[int, int, int, str]] = [
    (0x63EB7C, 0x35000D28, 0x14000069, "clFileSystemWorkspace::Initialise skip view ctor"),
]
PL_LUA_LLM_FIXES: list[tuple[int, int, int, str]] = [
    (0x6F3F28, 0xD10403FF, 0xD65F03C0, "CodeLiteLUA::Initialise immediate ret"),
    (0x7CF430, 0xD10103FF, 0xD65F03C0, "llm::Manager::Initialise immediate ret"),
    (0x8279D8, 0xD105C3FF, 0xD65F03C0, "wxCF667InitBitmapResources immediate ret"),
]
# 0x6F8460 reserved for patch-wx-paint-backing.py (FUI34PBK).
WX_MARKER_O = (0x6F8470, b"FUI34OCB")
WX_MARKER_D = (0x6F8480, b"FUI34DSW")
WX_MARKER_B = (0x6F8490, b"FUI34OTB")
WX_MARKER_C = (0x6F84A0, b"FUI34CTB")
PL_MARKER = (0xC73A00, b"FUI34PLG\x00")
PL_MARKER_F = (0xC73A10, b"FUI34FWS\x00")
PL_MARKER_AI = (0xC73A20, b"FUI34AIB\x00")


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


def apply_fixes(
    path: Path,
    fixes: list[tuple[int, int, int, str]],
    markers: list[tuple[int, bytes]],
    label: str,
) -> None:
    data = bytearray(path.read_bytes())
    done_marker = markers[-1] if markers else None
    already = bool(done_marker and done_marker[1] in data)
    changed = False
    for site_va, orig, new, desc in fixes:
        off = va_to_offset(data, site_va)
        cur = struct.unpack_from("<I", data, off)[0]
        if cur == new:
            print(f"[{label}] skip {desc} @ {site_va:#x}")
            continue
        allowed = (orig,) if isinstance(orig, int) else orig
        if cur not in allowed:
            raise SystemExit(
                f"[{label}] unexpected @ {site_va:#x}: {cur:#010x} expected one of "
                f"{', '.join(f'{o:#010x}' for o in allowed)}"
            )
        struct.pack_into("<I", data, off, new)
        changed = True
        print(f"[{label}] {desc} @ {site_va:#x} in {path.name}")
    if not changed and already:
        print(f"[{label}] skip {path.name} (already patched)")
        return
    for marker in markers:
        moff = va_to_offset(data, marker[0])
        data[moff : moff + len(marker[1])] = marker[1]
    path.write_bytes(data)


def main() -> None:
    apply_fixes(CL, CL_FIXES, [CL_MARKER, CL_MARKER_C, CL_MARKER_D], "f-ui-3.4c")
    apply_fixes(CL, CL_SIDEBAR_FIXES, [CL_MARKER_E], "f-ui-3.4e")
    apply_fixes(CL, CL_SIZER_FIXES, [CL_MARKER_S], "f-ui-3.4g")
    apply_fixes(CL, CL_FRAME_SIZER_FIXES, [CL_MARKER_FS], "f-ui-3.4k")
    apply_fixes(CL, CL_GETSIZER_FIXES, [CL_MARKER_FS], "f-ui-3.4k")
    apply_fixes(CL, CL_FIND_FIXES, [CL_MARKER_F], "f-ui-3.4h")
    apply_fixes(CL, CL_MENU_FIXES, [CL_MARKER_M], "f-ui-3.4i")
    apply_fixes(CL, CL_TERMINAL_FIXES, [CL_MARKER_T], "f-ui-3.4j")
    apply_fixes(CL, CL_ONINIT_DTOR_FIXES, [CL_MARKER_OIN], "f-ui-3.4l")
    apply_fixes(CL, CL_GETMENULABEL_FIXES, [CL_MARKER_P, CL_MARKER_GML], "f-ui-3.4m")
    apply_fixes(CL, CL_PLUGIN_LOAD_FIXES, [CL_MARKER_PLD, CL_MARKER_CIS], "f-ui-3.4s")
    apply_fixes(CL, CL_INIT_COMPLETED_FIXES, [CL_MARKER_CIS], "f-ui-3.4u")
    apply_fixes(WX, WX_FIXES, [], "f-ui-3.4b")
    apply_fixes(WX, WX_OCSB_FIXES, [WX_MARKER_O], "f-ui-3.4f")
    apply_fixes(WX, WX_OCTB_FIXES, [WX_MARKER_B], "f-ui-3.4v")
    apply_fixes(WX, WX_CTB_FIXES, [WX_MARKER_C], "f-ui-3.4w")
    apply_fixes(WX, WX_DSWV_FIXES, [WX_MARKER_D], "f-ui-3.4j")
    apply_fixes(PL, PL_FIXES, [PL_MARKER], "f-ui-3.4f")
    apply_fixes(PL, PL_FSWS_FIXES, [PL_MARKER_F], "f-ui-3.4m")
    apply_fixes(PL, PL_LUA_LLM_FIXES, [PL_MARKER_AI], "f-ui-3.4t")
    print("[f-ui-3.4s] boot progress patches applied")


if __name__ == "__main__":
    main()
