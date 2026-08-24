#pragma once

#include <string>

// F-8.1 / F-8.2: New Harmony Project (host libentry only).
#include "fw2_project_dialog.h"

#ifdef __cplusplus
extern "C" {
#endif

// F-8.1 probe path: fixed HelloWorld project (headless).
int Fw2_NewHarmonyConsoleProject();
void Fw2_InstallHarmonyProjectMenuIfNeeded();
void Fw2_ProbeHarmonyNewProject();
void Fw2_RunProjectProbe();

#ifdef __cplusplus
}
#endif
