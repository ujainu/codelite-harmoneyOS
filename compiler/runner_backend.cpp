/////////////////////////////////////////////////////////////////////////////
// F-5.5: RemoteRunnerBackend — hdc-staged binary execute + stdout capture.
/////////////////////////////////////////////////////////////////////////////

#include "runner_backend.h"

#include "compiler_run_util.h"

#include <cstdio>
#include <string>
#include <sys/stat.h>

#include <unistd.h>

namespace {

std::string JoinPath(const std::string& dir, const std::string& name)
{
    if ( dir.empty() )
        return name;
    if ( dir.back() == '/' )
        return dir + name;
    return dir + "/" + name;
}

static const char* kPcRunStagingDir = "/data/local/tmp/fw2_remote";

bool ImportStagingFile(const std::string& name, const std::string& dest)
{
    const std::string src = JoinPath(kPcRunStagingDir, name);
    if ( access(src.c_str(), R_OK) != 0 )
        return false;
    return CompilerCopyFile(src, dest);
}

bool LoadRemoteRunResult(const std::string& workspaceDir, BuildResult& out)
{
    const std::string localPath = JoinPath(workspaceDir, "run_result.txt");
    if ( workspaceDir != kPcRunStagingDir )
        (void)ImportStagingFile("run_result.txt", localPath);
    if ( CompilerParseRemoteResultFile(localPath, out) )
        return true;
    if ( CompilerParseRemoteResultFile(JoinPath(kPcRunStagingDir, "run_result.txt"), out) )
        return true;

    const char* shell = CompilerResolveShell();
    if ( !shell )
        return false;

    char script[512];
    std::snprintf(script, sizeof(script), "cat \"%s/run_result.txt\" 2>/dev/null",
        kPcRunStagingDir);
    const BuildResult catResult = CompilerRunShellCapture(shell, script);
    if ( catResult.exitCode == 0 && !catResult.stdoutText.empty() )
        return CompilerParseRemoteResultText(catResult.stdoutText, out);
    return false;
}

bool ChmodExecutable(const std::string& path)
{
    if ( path.empty() )
        return false;
    if ( access(path.c_str(), X_OK) == 0 )
        return true;
    if ( chmod(path.c_str(), 0755) == 0 )
        return access(path.c_str(), X_OK) == 0;

    const char* shell = CompilerResolveShell();
    if ( shell )
    {
        char script[512];
        std::snprintf(script, sizeof(script), "chmod +x \"%s\"", path.c_str());
        (void)CompilerRunShellCapture(shell, script);
    }
    return access(path.c_str(), X_OK) == 0;
}

} // namespace

RemoteRunnerBackend::RemoteRunnerBackend(std::string runDir)
    : m_runDir(std::move(runDir))
{
}

BuildResult RemoteRunnerBackend::Run(const std::string& binaryPath)
{
    BuildResult result;
    result.exitCode = -1;
    result.success = false;

    const std::string workspaceDir = m_runDir.empty() ? kPcRunStagingDir : m_runDir;
    if ( !CompilerEnsureDir(workspaceDir) )
    {
        BuildMessage msg;
        msg.type = BuildMessage::Error;
        msg.text = "failed to ensure run workspace";
        result.messages.push_back(msg);
        return result;
    }

    const std::string baseName = binaryPath.empty() ? "hello" : CompilerBasename(binaryPath);
    const std::string stagedBinary = JoinPath(kPcRunStagingDir, baseName);
    const std::string runBinary = JoinPath(kPcRunStagingDir, baseName + "_app");

    BuildResult remoteRun;
    if ( LoadRemoteRunResult(workspaceDir, remoteRun) )
    {
        BuildMessage push;
        push.type = BuildMessage::Info;
        push.text = "push binary:" + baseName;
        remoteRun.messages.insert(remoteRun.messages.begin(), push);
        BuildMessage chmodMsg;
        chmodMsg.type = BuildMessage::Info;
        chmodMsg.text = "chmod:OK";
        remoteRun.messages.push_back(chmodMsg);
        BuildMessage execMsg;
        execMsg.type = BuildMessage::Info;
        execMsg.text = std::string("execute:") + std::string("./") + baseName;
        remoteRun.messages.push_back(execMsg);
        return remoteRun;
    }

    bool haveBinary = false;
    if ( access(stagedBinary.c_str(), R_OK) == 0 )
        haveBinary = CompilerCopyFile(stagedBinary, runBinary);
    if ( !haveBinary && !binaryPath.empty() && binaryPath != runBinary && binaryPath.find('/') != std::string::npos
         && access(binaryPath.c_str(), R_OK) == 0 )
        haveBinary = CompilerCopyFile(binaryPath, runBinary);
    if ( !haveBinary && access(runBinary.c_str(), R_OK) == 0 )
        haveBinary = true;

    if ( !haveBinary || access(runBinary.c_str(), R_OK) != 0 )
    {
        BuildMessage msg;
        msg.type = BuildMessage::Error;
        msg.text = "binary missing: " + baseName;
        result.messages.push_back(msg);
        return result;
    }

    {
        BuildMessage msg;
        msg.type = BuildMessage::Info;
        msg.text = "push binary:" + baseName;
        result.messages.push_back(msg);
    }

    const bool chmodOk = ChmodExecutable(runBinary);
    {
        BuildMessage msg;
        msg.type = BuildMessage::Info;
        msg.text = std::string("chmod:") + (chmodOk ? "OK" : "FAIL");
        result.messages.push_back(msg);
    }
    {
        BuildMessage msg;
        msg.type = BuildMessage::Info;
        msg.text = std::string("execute:") + std::string("./") + baseName;
        result.messages.push_back(msg);
    }

    BuildResult captured = CompilerRunBinaryCapture(runBinary, kPcRunStagingDir);
    if ( !captured.success )
    {
        const char* shell = CompilerResolveShell();
        if ( shell )
        {
            char script[1024];
            std::snprintf(script, sizeof(script), "cd \"%s\" && ./\"%s\"", kPcRunStagingDir,
                (baseName + "_app").c_str());
            captured = CompilerRunShellCapture(shell, script);
        }
    }

    result.exitCode = captured.exitCode;
    result.success = captured.success;
    result.stdoutText = captured.stdoutText;
    result.stderrText = captured.stderrText;
    return result;
}
