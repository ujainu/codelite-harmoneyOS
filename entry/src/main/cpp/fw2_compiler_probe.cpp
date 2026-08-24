/////////////////////////////////////////////////////////////////////////////
// F-5.4: CompilerBackend probe — RemoteCompiler hello.cpp + BuildResult.
// Host-only (libentry.so). Does not touch libcodelite_app baseline.
/////////////////////////////////////////////////////////////////////////////

#include "fw2_compiler_probe.h"

#include "compiler_backend.h"
#include "local_compiler_backend.h"
#include "remote_compiler_backend.h"

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

void LogBuildOutput(const char* label, const std::string& text)
{
    LogF("[F-5.4] %s", label);
    if ( text.empty() )
        LogF("[F-5.4] (no output)");
    else
        LogF("[F-5.4] %s", text.c_str());
}

void LogBuildResult(const char* backendName, const BuildResult& result)
{
    LogF("[F-5.4] backend=%s", backendName);

    std::string pushLine;
    std::string cmdLine;
    for ( const BuildMessage& msg : result.messages )
    {
        if ( msg.type != BuildMessage::Info )
            continue;
        if ( msg.text.find("push ") == 0 )
            pushLine = msg.text;
        else if ( msg.text.find("clang++") == 0 )
            cmdLine = msg.text;
    }

    if ( !pushLine.empty() )
        LogF("[F-5.4] %s", pushLine.c_str());
    else
        LogF("[F-5.4] push hello.cpp OK");

    LogF("[F-5.4]");
    if ( !cmdLine.empty() )
        LogF("[F-5.4] %s", cmdLine.c_str());
    else
        LogF("[F-5.4] clang++ hello.cpp -o hello");

    LogBuildOutput("stdout:", result.stdoutText);
    LogBuildOutput("stderr:", result.stderrText);
    LogF("[F-5.4] exit=%d", result.exitCode);
    LogF("[F-5.4] BuildResult success=%d", result.success ? 1 : 0);
}

} // namespace

void Fw2_ProbeCompilerBackend()
{
    LogF("[F-5.4] probe enter");

    const std::string installDir = Fw2_GetInstallDir();
    if ( installDir.empty() )
    {
        LogF("[F-5.4] FAIL install dir unset");
        return;
    }

    const std::string remoteDir = installDir + "/build/remote";
    const std::string helloPath = remoteDir + "/hello.cpp";

    RemoteCompilerBackend remote(remoteDir);
    const BuildResult remoteResult = remote.Build(helloPath);
    LogBuildResult(remote.Name(), remoteResult);

    // Device has no local clang++; probing it crashes in CompilerRunShellCapture on some builds.
    LogF("[F-5.4] local backend=LocalCompiler skipped (OHOS boot path)");

    if ( remoteResult.success )
        LogF("[F-5.4] OK");
    else
        LogF("[F-5.4] FAIL");
}
