/////////////////////////////////////////////////////////////////////////////
// F-5.3: Build-output stream probe — stdout + stderr pipes + exit code.
// Host-only (libentry.so). Reuses fork/pipe from F-5.1; no AsyncProcess.
/////////////////////////////////////////////////////////////////////////////

#include "fw2_process_output_probe.h"

#include <hilog/log.h>

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>

#include <fcntl.h>
#include <sys/wait.h>
#include <unistd.h>

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xF002
#define LOG_TAG "FW2Host"

namespace {

struct CaptureResult
{
    pid_t pid = -1;
    int exitCode = -1;
    std::string stdoutText;
    std::string stderrText;
    bool ok = false;
};

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

void TrimInPlace(std::string& s)
{
    while ( !s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ') )
        s.pop_back();
    size_t start = 0;
    while ( start < s.size() && (s[start] == ' ' || s[start] == '\t') )
        ++start;
    if ( start > 0 )
        s.erase(0, start);
}

const char* ResolveShell()
{
    static const char* kShells[] = { "/bin/sh", "/system/bin/sh", "/data/local/sh" };
    for ( const char* sh : kShells )
    {
        if ( sh && sh[0] && access(sh, X_OK) == 0 )
            return sh;
    }
    return nullptr;
}

void ReadPipe(int fd, std::string& out)
{
    char buf[512];
    ssize_t n = 0;
    while ( (n = read(fd, buf, sizeof(buf) - 1)) > 0 )
    {
        buf[n] = '\0';
        out += buf;
    }
}

CaptureResult RunShellScript(const char* shell, const char* script)
{
    CaptureResult result;

    int outPipe[2] = { -1, -1 };
    int errPipe[2] = { -1, -1 };
    if ( pipe(outPipe) != 0 || pipe(errPipe) != 0 )
    {
        LogF("[F-5.3] FAIL pipe errno=%d (%s)", errno, std::strerror(errno));
        if ( outPipe[0] >= 0 )
            close(outPipe[0]);
        if ( outPipe[1] >= 0 )
            close(outPipe[1]);
        if ( errPipe[0] >= 0 )
            close(errPipe[0]);
        if ( errPipe[1] >= 0 )
            close(errPipe[1]);
        return result;
    }

    const pid_t pid = fork();
    if ( pid < 0 )
    {
        LogF("[F-5.3] FAIL fork errno=%d (%s)", errno, std::strerror(errno));
        close(outPipe[0]);
        close(outPipe[1]);
        close(errPipe[0]);
        close(errPipe[1]);
        return result;
    }

    if ( pid == 0 )
    {
        close(outPipe[0]);
        close(errPipe[0]);
        if ( dup2(outPipe[1], STDOUT_FILENO) < 0 )
            _exit(126);
        if ( dup2(errPipe[1], STDERR_FILENO) < 0 )
            _exit(126);
        close(outPipe[1]);
        close(errPipe[1]);
        execl(shell, "sh", "-c", script, static_cast<char*>(nullptr));
        _exit(127);
    }

    result.pid = pid;
    close(outPipe[1]);
    close(errPipe[1]);

    ReadPipe(outPipe[0], result.stdoutText);
    ReadPipe(errPipe[0], result.stderrText);
    close(outPipe[0]);
    close(errPipe[0]);

    int status = 0;
    if ( waitpid(pid, &status, 0) != pid )
    {
        LogF("[F-5.3] FAIL waitpid errno=%d (%s)", errno, std::strerror(errno));
        return result;
    }

    result.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    result.ok = true;
    return result;
}

bool TestStdout(const char* shell)
{
    const CaptureResult r = RunShellScript(shell, "echo HarmonyBuild");
    if ( !r.ok )
        return false;

    std::string line = r.stdoutText;
    TrimInPlace(line);
    LogF("[F-5.3] process started pid=%d", static_cast<int>(r.pid));
    LogF("[F-5.3] stdout line=%s", line.empty() ? "(empty)" : line.c_str());
    LogF("[F-5.3] exit=%d", r.exitCode);

    return r.exitCode == 0 && line == "HarmonyBuild";
}

bool TestStderr(const char* shell)
{
    const CaptureResult r = RunShellScript(shell, "echo ErrorMessage >&2");
    if ( !r.ok )
        return false;

    std::string line = r.stderrText;
    TrimInPlace(line);
    LogF("[F-5.3] stderr line=%s", line.empty() ? "(empty)" : line.c_str());

    return r.exitCode == 0 && line == "ErrorMessage";
}

bool TestExitCode(const char* shell)
{
    const CaptureResult r = RunShellScript(shell, "exit 7");
    if ( !r.ok )
        return false;

    LogF("[F-5.3] exit=%d", r.exitCode);
    return r.exitCode == 7;
}

bool TestMockBuild(const char* shell)
{
    const char* script =
        "echo compiling main.cpp; "
        "echo warning:test >&2; "
        "exit 0";
    const CaptureResult r = RunShellScript(shell, script);
    if ( !r.ok )
        return false;

    std::string out = r.stdoutText;
    std::string err = r.stderrText;
    TrimInPlace(out);
    TrimInPlace(err);

    LogF("[F-5.3] mock stdout=%s", out.empty() ? "(empty)" : out.c_str());
    LogF("[F-5.3] mock stderr=%s", err.empty() ? "(empty)" : err.c_str());
    LogF("[F-5.3] mock exit=%d", r.exitCode);

    return r.exitCode == 0 && out.find("compiling main.cpp") != std::string::npos
           && err.find("warning:test") != std::string::npos;
}

} // namespace

void Fw2_ProbeProcessOutput()
{
    LogF("[F-5.3] probe enter");

    const char* shell = ResolveShell();
    if ( !shell )
    {
        LogF("[F-5.3] FAIL no shell");
        return;
    }

    bool pass = true;
    if ( !TestStdout(shell) )
    {
        LogF("[F-5.3] FAIL stdout capture");
        pass = false;
    }
    if ( !TestStderr(shell) )
    {
        LogF("[F-5.3] FAIL stderr capture");
        pass = false;
    }
    if ( !TestExitCode(shell) )
    {
        LogF("[F-5.3] FAIL exit code capture");
        pass = false;
    }
    if ( !TestMockBuild(shell) )
    {
        LogF("[F-5.3] FAIL mock build stream");
        pass = false;
    }

    if ( pass )
        LogF("[F-5.3] OK");
    else
        LogF("[F-5.3] FAIL");
}
