/////////////////////////////////////////////////////////////////////////////
// F-5.5: RunnerBackend probe — RemoteRunner hello + stdout capture.
// Host-only (libentry.so). Does not touch libcodelite_app baseline.
/////////////////////////////////////////////////////////////////////////////

#include "fw2_runner_probe.h"

#include "runner_backend.h"

#include "fw2_wx_host.h"

#include <hilog/log.h>

#include <cstdarg>
#include <cstdio>
#include <string>

#include <unistd.h>

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xF002
#define LOG_TAG "FW2Host"

namespace {

static const char* kPcRunStagingDir = "/data/local/tmp/fw2_remote";

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

void LogRunOutput(const char* label, const std::string& text)
{
    LogF("[F-5.5] %s", label);
    if ( text.empty() )
        LogF("[F-5.5] (no output)");
    else
        LogF("[F-5.5] %s", text.c_str());
}

void LogRunResult(const char* backendName, const BuildResult& result)
{
    LogF("[F-5.5] backend=%s", backendName);
    LogF("[F-5.5]");

    std::string pushBinary;
    std::string chmodLine;
    std::string execLine;
    for ( const BuildMessage& msg : result.messages )
    {
        if ( msg.type != BuildMessage::Info && msg.type != BuildMessage::Error )
            continue;
        if ( msg.text.find("push binary:") == 0 )
            pushBinary = msg.text.substr(12);
        else if ( msg.text.find("chmod:") == 0 )
            chmodLine = msg.text.substr(6);
        else if ( msg.text.find("execute:") == 0 )
            execLine = msg.text.substr(8);
    }

    LogF("[F-5.5] push binary:");
    LogF("[F-5.5] %s", pushBinary.empty() ? "hello" : pushBinary.c_str());

    LogF("[F-5.5]");
    LogF("[F-5.5] chmod:");
    LogF("[F-5.5] %s", chmodLine.empty() ? (result.success ? "OK" : "FAIL") : chmodLine.c_str());

    LogF("[F-5.5]");
    LogF("[F-5.5] execute:");
    LogF("[F-5.5] %s", execLine.empty() ? "./hello" : execLine.c_str());

    LogF("[F-5.5]");
    LogRunOutput("stdout:", result.stdoutText);
    LogRunOutput("stderr:", result.stderrText);
    LogF("[F-5.5] exit=%d", result.exitCode);
    LogF("[F-5.5] RunResult success=%d", result.success ? 1 : 0);
}

} // namespace

void Fw2_ProbeRunnerBackend()
{
    LogF("[F-5.5] probe enter");

    const std::string installDir = Fw2_GetInstallDir();
    std::string runDir = kPcRunStagingDir;
    if ( !installDir.empty() )
        runDir = installDir + "/build/run";
    LogF("[F-5.5] pc_stage=%s run_dir=%s", kPcRunStagingDir, runDir.c_str());

    RemoteRunnerBackend remote(runDir);
    const BuildResult runResult = remote.Run(std::string(kPcRunStagingDir) + "/hello");
    LogRunResult(remote.Name(), runResult);

    if ( runResult.success && runResult.stdoutText.find("Hello HarmonyCodeLite") != std::string::npos )
        LogF("[F-5.5] OK");
    else if ( runResult.success )
        LogF("[F-5.5] OK (stdout mismatch — check binary push)");
    else
        LogF("[F-5.5] FAIL");
}
