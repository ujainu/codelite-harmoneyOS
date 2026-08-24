/////////////////////////////////////////////////////////////////////////////
// F-5.4: LocalCompilerBackend — device-local clang++ when available.
/////////////////////////////////////////////////////////////////////////////

#include "local_compiler_backend.h"

#include "compiler_run_util.h"

#include <cstdio>
#include <string>

#include <unistd.h>

namespace {

bool PathExecutable(const char* path)
{
    return path && path[0] && access(path, X_OK) == 0;
}

std::string FindLocalClangxx(const char* shell)
{
    static const char* kFixed[] = {
        "/bin/clang++",
        "/system/bin/clang++",
        "/data/local/bin/clang++",
    };
    for ( const char* p : kFixed )
    {
        if ( PathExecutable(p) )
            return p;
    }

    if ( !shell )
        return {};

    BuildResult which = CompilerRunShellCapture(shell,
        "command -v clang++ 2>/dev/null || which clang++ 2>/dev/null");
    if ( which.exitCode != 0 )
        return {};

    std::string path = which.stdoutText;
    while ( !path.empty() && (path.back() == '\n' || path.back() == '\r' || path.back() == ' ') )
        path.pop_back();
    if ( PathExecutable(path.c_str()) )
        return path;
    return {};
}

} // namespace

BuildResult LocalCompilerBackend::Build(const std::string& sourceFile)
{
    BuildResult result;
    result.exitCode = -1;
    result.success = false;

    if ( sourceFile.empty() || access(sourceFile.c_str(), R_OK) != 0 )
    {
        BuildMessage msg;
        msg.type = BuildMessage::Error;
        msg.text = "source file missing or unreadable";
        result.messages.push_back(msg);
        return result;
    }

    const char* shell = CompilerResolveShell();
    const std::string clangxx = FindLocalClangxx(shell);
    if ( clangxx.empty() )
    {
        BuildMessage msg;
        msg.type = BuildMessage::Error;
        msg.text = "local clang++ not available (compiler_available=0)";
        result.messages.push_back(msg);
        return result;
    }

    const std::string base = CompilerBasename(sourceFile);
    const size_t dot = base.find_last_of('.');
    const std::string stem = (dot == std::string::npos) ? base : base.substr(0, dot);

    const size_t dirSlash = sourceFile.find_last_of('/');
    const std::string workDir = (dirSlash == std::string::npos) ? "." : sourceFile.substr(0, dirSlash);
    const std::string outBin = workDir + "/" + stem;

    char cmd[2048];
    std::snprintf(cmd, sizeof(cmd),
        "cd \"%s\" && \"%s\" \"%s\" -o \"%s\"",
        workDir.c_str(),
        clangxx.c_str(),
        sourceFile.c_str(),
        outBin.c_str());

    result = CompilerRunShellCapture(shell, cmd);
    if ( result.success )
    {
        BuildMessage msg;
        msg.type = BuildMessage::Info;
        msg.text = "local compile OK";
        result.messages.push_back(msg);
    }
    else
    {
        BuildMessage msg;
        msg.type = BuildMessage::Error;
        msg.text = "local compile failed";
        result.messages.push_back(msg);
    }
    return result;
}
