#pragma once

#include <string>

// F-8.2: New Harmony Project dialog (libentry.so only).

enum class HarmonyProjectTemplate
{
    CppConsole = 0,
    HarmonyConsole = 1,
};

struct HarmonyProjectCreateParams
{
    std::string name;
    std::string location;
    HarmonyProjectTemplate templ = HarmonyProjectTemplate::CppConsole;
    bool openEditor = true;
    bool buildAfterCreate = true;
};

// Returns 1 on success, 0 on cancel, -1 on error.
int Fw2_ShowNewHarmonyProjectDialog();

// Headless create (probe / programmatic).
int Fw2_CreateHarmonyProject(const HarmonyProjectCreateParams& params);
