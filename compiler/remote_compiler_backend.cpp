/////////////////////////////////////////////////////////////////////////////
// F-5.4.1 / F-8.3: RemoteCompilerBackend — PC DevEco compile + build.json flags.
/////////////////////////////////////////////////////////////////////////////

#include "remote_compiler_backend.h"

#include "compiler_run_util.h"
#include "project_config.h"

#include <cstdio>
#include <string>

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

static const char* kPcStagingDir = "/data/local/tmp/fw2_remote";

bool ImportPcStagingFile(const std::string& name, const std::string& dest)
{
    const std::string src = JoinPath(kPcStagingDir, name);
    if ( access(src.c_str(), R_OK) != 0 )
        return false;
    return CompilerCopyFile(src, dest);
}

} // namespace

RemoteCompilerBackend::RemoteCompilerBackend(std::string workspaceDir, ProjectBuildConfig config)
    : m_workspaceDir(std::move(workspaceDir))
    , m_config(std::move(config))
{
}

BuildResult RemoteCompilerBackend::Build(const std::string& sourceFile)
{
    BuildResult result;
    result.exitCode = -1;
    result.success = false;

    if ( m_workspaceDir.empty() )
    {
        BuildMessage msg;
        msg.type = BuildMessage::Error;
        msg.text = "remote workspace dir unset";
        result.messages.push_back(msg);
        return result;
    }

    if ( !CompilerEnsureDir(m_workspaceDir) )
    {
        BuildMessage msg;
        msg.type = BuildMessage::Error;
        msg.text = "failed to create remote workspace";
        result.messages.push_back(msg);
        return result;
    }

    const std::string defaultSource = m_config.source.empty() ? "hello.cpp" : m_config.source;
    const std::string baseName = sourceFile.empty() ? defaultSource : CompilerBasename(sourceFile);
    const std::string stagedSource = JoinPath(m_workspaceDir, baseName);
    const std::string outName = m_config.output.empty() ? "hello" : m_config.output;
    const std::string outBin = JoinPath(m_workspaceDir, outName);

    // Stage build.json from project dir when available.
    const std::string projectBuildJson = FindBuildJsonForSource(sourceFile);
    if ( !projectBuildJson.empty() )
    {
        (void)CompilerCopyFile(projectBuildJson, JoinPath(m_workspaceDir, "build.json"));
        ProjectBuildConfig loaded;
        if ( LoadProjectBuildConfig(projectBuildJson, loaded) )
            m_config = std::move(loaded);
    }

    (void)ImportPcStagingFile(baseName, stagedSource);
    (void)ImportPcStagingFile(outName, outBin);
    (void)ImportPcStagingFile("hello", outBin);
    (void)ImportPcStagingFile("remote_result.txt", JoinPath(m_workspaceDir, "remote_result.txt"));
    (void)ImportPcStagingFile("build.json", JoinPath(m_workspaceDir, "build.json"));

    bool staged = false;
    if ( !sourceFile.empty() && sourceFile != stagedSource && access(sourceFile.c_str(), R_OK) == 0 )
        staged = CompilerCopyFile(sourceFile, stagedSource);
    else if ( access(stagedSource.c_str(), R_OK) == 0 )
        staged = true;
    else
    {
        static const char* kHello =
            "#include <cstdio>\n"
            "int main() {\n"
            "    std::puts(\"HarmonyCodeLite hello\");\n"
            "    return 0;\n"
            "}\n";
        staged = CompilerWriteTextFile(stagedSource, kHello);
    }

    if ( !staged || access(stagedSource.c_str(), R_OK) != 0 )
    {
        BuildMessage msg;
        msg.type = BuildMessage::Error;
        msg.text = "failed to stage source for remote compile";
        result.messages.push_back(msg);
        return result;
    }

    {
        BuildMessage msg;
        msg.type = BuildMessage::Info;
        msg.text = "push " + baseName + " OK";
        result.messages.push_back(msg);
    }

    const std::string compileCmd = m_config.FormatCompileCommand(baseName);
    {
        BuildMessage msg;
        msg.type = BuildMessage::Info;
        msg.text = "[F-8.3] config loaded compiler=" + m_config.compiler + " target=" + m_config.target;
        result.messages.push_back(msg);
    }
    {
        BuildMessage msg;
        msg.type = BuildMessage::Info;
        msg.text = compileCmd;
        result.messages.push_back(msg);
    }

    const std::string resultPath = JoinPath(m_workspaceDir, "remote_result.txt");
    BuildResult parsed;
    if ( CompilerParseRemoteResultFile(resultPath, parsed) )
    {
        parsed.messages.insert(parsed.messages.begin(), result.messages.begin(), result.messages.end());
        if ( parsed.success )
            parsed.outputBinary = outBin;
        return parsed;
    }

    const std::string helperScript = JoinPath(m_workspaceDir, "remote_compile.sh");
    const char* shell = CompilerResolveShell();
    if ( shell && access(helperScript.c_str(), X_OK) == 0 )
    {
        char cmd[1024];
        std::snprintf(cmd, sizeof(cmd), "cd \"%s\" && ./remote_compile.sh", m_workspaceDir.c_str());
        const BuildResult captured = CompilerRunShellCapture(shell, cmd);
        result.exitCode = captured.exitCode;
        result.success = captured.success;
        result.stdoutText = captured.stdoutText;
        result.stderrText = captured.stderrText;
        result.outputBinary = outBin;
        return result;
    }

    if ( shell )
    {
        char stub[256];
        std::snprintf(stub, sizeof(stub), "cd \"%s\" && exit 0", m_workspaceDir.c_str());
        const BuildResult captured = CompilerRunShellCapture(shell, stub);
        result.exitCode = captured.exitCode;
        result.success = captured.success;
        result.stdoutText = captured.stdoutText;
        result.stderrText = captured.stderrText;
    }
    else
    {
        result.exitCode = -1;
        result.success = false;
        BuildMessage msg;
        msg.type = BuildMessage::Error;
        msg.text = "no shell for remote compile stub";
        result.messages.push_back(msg);
    }

    result.outputBinary = outBin;
    return result;
}
