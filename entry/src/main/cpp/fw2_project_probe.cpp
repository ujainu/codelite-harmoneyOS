/////////////////////////////////////////////////////////////////////////////
// F-8.2: device probe — headless MyApp create after UI ready (libentry.so).
/////////////////////////////////////////////////////////////////////////////

#include "fw2_project_bridge.h"
#include "fw2_project_dialog.h"

#include "compiler_run_util.h"

#include <hilog/log.h>

#include <cstdlib>
#include <string>

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xF002
#define LOG_TAG "FW2Host"

namespace {

std::string JoinPath(const std::string& dir, const std::string& name)
{
    if ( dir.empty() )
        return name;
    if ( dir.back() == '/' )
        return dir + name;
    return dir + "/" + name;
}

void RemoveProjectDirForProbe(const std::string& projectDir)
{
    const char* shell = CompilerResolveShell();
    if ( !shell || projectDir.empty() )
        return;
    char cmd[512];
    std::snprintf(cmd, sizeof(cmd), "rm -rf \"%s\"", projectDir.c_str());
    (void)CompilerRunShellCapture(shell, cmd);
}

} // namespace

void Fw2_RunProjectProbe()
{
    OH_LOG_INFO(LOG_APP, "[F-8.2] probe enter (CallAfter wizard)");

    HarmonyProjectCreateParams params;
    params.name = "MyApp";
    const char* home = std::getenv("HOME");
    params.location = home && home[0] ? JoinPath(home, "Projects") : std::string();
    params.templ = HarmonyProjectTemplate::CppConsole;
    // Defer editor/build until MainBook layout is fully wired (AddPane path still patched).
    params.openEditor = false;
    params.buildAfterCreate = false;

    RemoveProjectDirForProbe(JoinPath(params.location, params.name));

    const int rc = Fw2_CreateHarmonyProject(params);
    OH_LOG_INFO(LOG_APP, "[F-8.2] probe create rc=%{public}d", rc);
}
