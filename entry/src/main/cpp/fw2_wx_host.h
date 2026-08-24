#pragma once

// Host bootstraps official CodeLiteApp (clMainFrame). No demo wxFrame.

// B4-002: InstallDir from Host Deployment (filesDir/share/codelite).
void Fw2_SetInstallDir(const char* installDir);
const char* Fw2_GetInstallDir();

bool Fw2_EnsureWxReady();
// BOOT-001: OnRun/MainLoop after Attach+Show (phase-1 returns after first Dispatch).
int Fw2_EmbeddedRun();
void Fw2_ShutdownWx();
void* Fw2_GetTopFrame(); // wxTopLevelWindow* / clMainFrame*
