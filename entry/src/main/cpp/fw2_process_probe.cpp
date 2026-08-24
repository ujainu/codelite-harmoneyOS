/////////////////////////////////////////////////////////////////////////////
// F-5.1: Process spawn probe — fork/exec/pipe on OHOS from libentry.so only.
// Does not touch libcodelite_app / AsyncProcess / wx build pipeline.
/////////////////////////////////////////////////////////////////////////////

#include "fw2_process_probe.h"

#include <hilog/log.h>

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xF002
#define LOG_TAG "FW2Host"

namespace {

void LogF(const char* fmt, ...)
{
    char buf[768];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    OH_LOG_INFO(LOG_APP, "%{public}s", buf);
    std::fprintf(stderr, "%s\n", buf);
    std::fflush(stderr);
}

bool PathExists(const char* path)
{
    struct stat st {};
    return path && path[0] && stat(path, &st) == 0;
}

bool PathExecutable(const char* path)
{
    return path && path[0] && access(path, X_OK) == 0;
}

void ProbeForkAvailable()
{
#if defined(_POSIX_VERSION) && !defined(__OHOS__)
    LogF("[F-5.PROC] fork compile-time=_POSIX_VERSION");
#else
    LogF("[F-5.PROC] fork compile-time=posix-fork-available");
#endif

    errno = 0;
    const pid_t pid = fork();
    if ( pid == 0 )
    {
        _exit(0);
    }
    if ( pid > 0 )
    {
        int status = 0;
        if ( waitpid(pid, &status, 0) == pid && WIFEXITED(status) && WEXITSTATUS(status) == 0 )
            LogF("[F-5.PROC] fork=OK");
        else
            LogF("[F-5.PROC] fork=OK wait=FAIL status=%d", status);
        return;
    }

    LogF("[F-5.PROC] fork=FAIL errno=%d (%s)", errno, std::strerror(errno));
}

void ProbeShellPaths()
{
    static const char* kShells[] = {
        "/bin/sh",
        "/system/bin/sh",
        "/data/local/sh",
    };
    int found = 0;
    for ( const char* sh : kShells )
    {
        if ( PathExecutable(sh) )
        {
            LogF("[F-5.PROC] sh path=%s exists=1 exec=1", sh);
            ++found;
        }
        else if ( PathExists(sh) )
        {
            LogF("[F-5.PROC] sh path=%s exists=1 exec=0", sh);
        }
    }
    if ( found == 0 )
        LogF("[F-5.PROC] sh path=(none) exists=0");
}

const char* ResolveShell()
{
    static const char* kShells[] = { "/bin/sh", "/system/bin/sh", "/data/local/sh" };
    for ( const char* sh : kShells )
    {
        if ( PathExecutable(sh) )
            return sh;
    }
    return nullptr;
}

bool SpawnEchoViaFork(const char* shell, std::string& out, pid_t& childPid, int& exitCode)
{
    out.clear();
    childPid = -1;
    exitCode = -1;

    int pipefd[2] = { -1, -1 };
    if ( pipe(pipefd) != 0 )
    {
        LogF("[F-5.1] FAIL pipe errno=%d (%s)", errno, std::strerror(errno));
        return false;
    }

    const pid_t pid = fork();
    if ( pid < 0 )
    {
        LogF("[F-5.1] FAIL fork errno=%d (%s)", errno, std::strerror(errno));
        close(pipefd[0]);
        close(pipefd[1]);
        return false;
    }

    if ( pid == 0 )
    {
        close(pipefd[0]);
        if ( dup2(pipefd[1], STDOUT_FILENO) < 0 )
            _exit(126);
        if ( dup2(pipefd[1], STDERR_FILENO) < 0 )
            _exit(126);
        close(pipefd[1]);
        execl(shell, "sh", "-c", "echo HarmonyCodeLite", static_cast<char*>(nullptr));
        _exit(127);
    }

    childPid = pid;
    close(pipefd[1]);

    char buf[256];
    ssize_t n = 0;
    while ( (n = read(pipefd[0], buf, sizeof(buf) - 1)) > 0 )
    {
        buf[n] = '\0';
        out += buf;
    }
    close(pipefd[0]);

    int status = 0;
    if ( waitpid(pid, &status, 0) != pid )
    {
        LogF("[F-5.1] FAIL waitpid errno=%d (%s)", errno, std::strerror(errno));
        return false;
    }

    exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    return true;
}

bool SpawnEchoViaPopen(const char* shell, std::string& out, int& exitCode)
{
    out.clear();
    exitCode = -1;

    std::string cmd = std::string(shell) + " -c \"echo HarmonyCodeLite\"";
    FILE* fp = popen(cmd.c_str(), "r");
    if ( !fp )
    {
        LogF("[F-5.1] FAIL popen errno=%d (%s)", errno, std::strerror(errno));
        return false;
    }

    char buf[256];
    while ( std::fgets(buf, sizeof(buf), fp) )
        out += buf;

    const int rc = pclose(fp);
    exitCode = WIFEXITED(rc) ? WEXITSTATUS(rc) : rc;
    LogF("[F-5.1] method=popen");
    return exitCode == 0;
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

} // namespace

void Fw2_ProbeProcess()
{
    LogF("[F-5.1] probe enter");

    ProbeForkAvailable();
    ProbeShellPaths();

    const char* shell = ResolveShell();
    if ( !shell )
    {
        LogF("[F-5.1] FAIL no executable shell");
        return;
    }

    std::string stdoutText;
    pid_t childPid = -1;
    int exitCode = -1;
    bool ok = SpawnEchoViaFork(shell, stdoutText, childPid, exitCode);

    if ( !ok || exitCode != 0 )
    {
        LogF("[F-5.1] fork/pipe path failed exit=%d — trying popen", exitCode);
        ok = SpawnEchoViaPopen(shell, stdoutText, exitCode);
        childPid = -1;
    }
    else
    {
        LogF("[F-5.1] method=fork_pipe");
    }

    TrimInPlace(stdoutText);

    if ( ok && exitCode == 0 )
    {
        LogF("[F-5.1] spawn pid=%d", static_cast<int>(childPid));
        LogF("[F-5.1] stdout=%s", stdoutText.empty() ? "(empty)" : stdoutText.c_str());
        LogF("[F-5.1] exit=%d", exitCode);
        LogF("[F-5.1] OK");
    }
    else
    {
        LogF("[F-5.1] FAIL exit=%d stdout=%s",
             exitCode, stdoutText.empty() ? "(empty)" : stdoutText.c_str());
    }
}
