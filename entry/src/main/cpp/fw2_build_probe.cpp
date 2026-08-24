/////////////////////////////////////////////////////////////////////////////
// F-5.6: BuildController probe — RemoteCompiler + RemoteRunner end-to-end.
// Host-only (libentry.so). Does not touch libcodelite_app baseline.
/////////////////////////////////////////////////////////////////////////////

#include "fw2_build_probe.h"

#include "build_controller.h"

#include "fw2_wx_host.h"

#include <hilog/log.h>

#include <cstdarg>
#include <cstdio>
#include <string>

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xF002
#define LOG_TAG "FW2Host"

namespace {

void LogF(const char* fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    OH_LOG_INFO(LOG_APP, "%{public}s", buf);
    std::fprintf(stderr, "%s\n", buf);
    std::fflush(stderr);
}

} // namespace

void Fw2_RunBuildProbe()
{
    LogF("[F-5.6] probe enter");

    const std::string installDir = Fw2_GetInstallDir();
    if ( installDir.empty() )
    {
        LogF("[F-5.6] FAIL install dir unset");
        return;
    }

    LogF("[F-5.6] controller=create");
    BuildController controller(installDir);
    LogF("[F-5.6] compiler=%s", controller.CompilerName());

    const std::string helloPath = installDir + "/build/remote/hello.cpp";
    const BuildResult result = controller.BuildAndRun(helloPath);

    LogF("[F-5.6] compile success=%d", controller.LastCompileResult().success ? 1 : 0);
    LogF("[F-5.6] runner=%s", controller.RunnerName());
    LogF("[F-5.6] stdout:");
    if ( result.stdoutText.empty() )
        LogF("[F-5.6] (no output)");
    else
        LogF("[F-5.6] %s", result.stdoutText.c_str());
    LogF("[F-5.6] exit=%d", result.exitCode);

    if ( controller.LastCompileResult().success && result.success
         && result.stdoutText.find("Hello HarmonyCodeLite") != std::string::npos )
        LogF("[F-5.6] OK");
    else if ( controller.LastCompileResult().success && result.success )
        LogF("[F-5.6] OK (stdout mismatch — check run_result push)");
    else
        LogF("[F-5.6] FAIL");
}
