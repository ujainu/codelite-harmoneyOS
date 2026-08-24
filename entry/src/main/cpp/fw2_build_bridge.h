#pragma once

#ifdef __cplusplus
extern "C" {
#endif

// F-5.6.5: Host bridge — wx Harmony menu → BuildController.
void Fw2_BuildRemote(const char* sourceFile);
void Fw2_RunRemote();
void Fw2_RegisterBuildBridge();
void Fw2_EnsureMainMenuBarAttached();
void Fw2_InstallHarmonyMenuIfNeeded();
void Fw2_ProbeHarmonyBuildMenu();

#ifdef __cplusplus
}

class wxMenuBar;
class wxFrame;
wxMenuBar* Fw2_GetMainMenuBarPtr();
wxFrame* Fw2_GetMainFramePtr();
void Fw2_ReattachMainMenuBarOnMainLoop();
void Fw2_FlushMenuBarChrome();
#endif
