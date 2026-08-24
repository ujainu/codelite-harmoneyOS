/////////////////////////////////////////////////////////////////////////////
// F-5.4: shared process capture for compiler backends (host libentry only).
/////////////////////////////////////////////////////////////////////////////

#include "compiler_run_util.h"

#include <cerrno>
#include <cstdio>
#include <cstring>

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

const char* CompilerResolveShell()
{
    static const char* kShells[] = { "/bin/sh", "/system/bin/sh", "/data/local/sh" };
    for ( const char* sh : kShells )
    {
        if ( sh && sh[0] && access(sh, X_OK) == 0 )
            return sh;
    }
    return nullptr;
}

bool CompilerEnsureDir(const std::string& dir)
{
    if ( dir.empty() )
        return false;
    if ( access(dir.c_str(), F_OK) == 0 )
        return true;

    std::string cur;
    for ( size_t i = 0; i < dir.size(); ++i )
    {
        const char c = dir[i];
        cur.push_back(c);
        if ( c != '/' )
            continue;
        if ( cur.size() <= 1 )
            continue;
        if ( access(cur.c_str(), F_OK) != 0 && mkdir(cur.c_str(), 0755) != 0 && errno != EEXIST )
            return false;
    }
    if ( access(dir.c_str(), F_OK) != 0 && mkdir(dir.c_str(), 0755) != 0 && errno != EEXIST )
        return false;
    return access(dir.c_str(), F_OK) == 0;
}

bool CompilerCopyFile(const std::string& src, const std::string& dst)
{
    FILE* in = std::fopen(src.c_str(), "rb");
    if ( !in )
        return false;
    FILE* out = std::fopen(dst.c_str(), "wb");
    if ( !out )
    {
        std::fclose(in);
        return false;
    }

    char buf[4096];
    size_t n = 0;
    bool ok = true;
    while ( (n = std::fread(buf, 1, sizeof(buf), in)) > 0 )
    {
        if ( std::fwrite(buf, 1, n, out) != n )
        {
            ok = false;
            break;
        }
    }
    std::fclose(in);
    std::fclose(out);
    return ok;
}

bool CompilerWriteTextFile(const std::string& path, const std::string& text)
{
    FILE* fp = std::fopen(path.c_str(), "wb");
    if ( !fp )
        return false;
    const bool ok = std::fwrite(text.data(), 1, text.size(), fp) == text.size();
    std::fclose(fp);
    return ok;
}

bool CompilerReadTextFile(const std::string& path, std::string& out)
{
    out.clear();
    FILE* fp = std::fopen(path.c_str(), "rb");
    if ( !fp )
        return false;
    char buf[4096];
    size_t n = 0;
    while ( (n = std::fread(buf, 1, sizeof(buf), fp)) > 0 )
        out.append(buf, n);
    std::fclose(fp);
    return true;
}

namespace {

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

} // namespace

BuildResult CompilerRunShellCapture(const char* shell, const char* script)
{
    BuildResult result;

    int outPipe[2] = { -1, -1 };
    int errPipe[2] = { -1, -1 };
    if ( pipe(outPipe) != 0 || pipe(errPipe) != 0 )
        return result;

    const pid_t pid = fork();
    if ( pid < 0 )
    {
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

    close(outPipe[1]);
    close(errPipe[1]);
    ReadPipe(outPipe[0], result.stdoutText);
    ReadPipe(errPipe[0], result.stderrText);
    close(outPipe[0]);
    close(errPipe[0]);

    int status = 0;
    if ( waitpid(pid, &status, 0) == pid && WIFEXITED(status) )
        result.exitCode = WEXITSTATUS(status);
    else
        result.exitCode = -1;

    result.success = (result.exitCode == 0);
    return result;
}

BuildResult CompilerRunBinaryCapture(const std::string& binaryPath, const std::string& workDir)
{
    BuildResult result;

    if ( binaryPath.empty() || access(binaryPath.c_str(), R_OK) != 0 )
        return result;

    int outPipe[2] = { -1, -1 };
    int errPipe[2] = { -1, -1 };
    if ( pipe(outPipe) != 0 || pipe(errPipe) != 0 )
        return result;

    const pid_t pid = fork();
    if ( pid < 0 )
    {
        close(outPipe[0]);
        close(outPipe[1]);
        close(errPipe[0]);
        close(errPipe[1]);
        return result;
    }

    if ( pid == 0 )
    {
        if ( !workDir.empty() )
            (void)chdir(workDir.c_str());
        close(outPipe[0]);
        close(errPipe[0]);
        if ( dup2(outPipe[1], STDOUT_FILENO) < 0 )
            _exit(126);
        if ( dup2(errPipe[1], STDERR_FILENO) < 0 )
            _exit(126);
        close(outPipe[1]);
        close(errPipe[1]);
        const char* argv[] = { binaryPath.c_str(), static_cast<char*>(nullptr) };
        execv(binaryPath.c_str(), const_cast<char**>(argv));
        _exit(127);
    }

    close(outPipe[1]);
    close(errPipe[1]);
    ReadPipe(outPipe[0], result.stdoutText);
    ReadPipe(errPipe[0], result.stderrText);
    close(outPipe[0]);
    close(errPipe[0]);

    int status = 0;
    if ( waitpid(pid, &status, 0) == pid && WIFEXITED(status) )
        result.exitCode = WEXITSTATUS(status);
    else
        result.exitCode = -1;

    result.success = (result.exitCode == 0);
    return result;
}

bool CompilerParseRemoteResultFile(const std::string& path, BuildResult& out)
{
    std::string text;
    if ( !CompilerReadTextFile(path, text) )
        return false;
    return CompilerParseRemoteResultText(text, out);
}

bool CompilerParseRemoteResultText(const std::string& text, BuildResult& out)
{
    if ( text.empty() )
        return false;

    out = BuildResult {};
    out.exitCode = -1;
    out.success = false;

    enum Section
    {
        Meta,
        StdoutSection,
        StderrSection
    } section = Meta;

    size_t pos = 0;
    while ( pos < text.size() )
    {
        size_t end = text.find('\n', pos);
        if ( end == std::string::npos )
            end = text.size();
        std::string line = text.substr(pos, end - pos);
        pos = (end < text.size()) ? end + 1 : end;

        if ( line == "---stdout---" )
        {
            section = StdoutSection;
            continue;
        }
        if ( line == "---stderr---" )
        {
            section = StderrSection;
            continue;
        }

        if ( section == Meta && line.rfind("exit=", 0) == 0 )
        {
            out.exitCode = std::atoi(line.c_str() + 5);
            continue;
        }
        if ( section == Meta && line.rfind("success=", 0) == 0 )
        {
            out.success = (line.size() > 8 && line[8] == '1');
            continue;
        }

        if ( section == StdoutSection )
        {
            if ( !out.stdoutText.empty() )
                out.stdoutText.push_back('\n');
            out.stdoutText += line;
        }
        else if ( section == StderrSection )
        {
            if ( !out.stderrText.empty() )
                out.stderrText.push_back('\n');
            out.stderrText += line;
        }
    }

    if ( out.exitCode >= 0 )
    {
        if ( text.find("success=") == std::string::npos )
            out.success = (out.exitCode == 0);
        return true;
    }
    return false;
}

std::string CompilerBasename(const std::string& path)
{
    const size_t slash = path.find_last_of('/');
    if ( slash == std::string::npos )
        return path;
    return path.substr(slash + 1);
}
