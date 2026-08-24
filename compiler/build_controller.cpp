/////////////////////////////////////////////////////////////////////////////
// F-5.6: BuildController — RemoteCompiler + RemoteRunner orchestration.
/////////////////////////////////////////////////////////////////////////////

#include "build_controller.h"

#include "compiler_run_util.h"
#include "remote_compiler_backend.h"
#include "runner_backend.h"

#include <string>

namespace {

std::string JoinPath(const std::string& dir, const std::string& name)
{
    if ( dir.empty() )
        return name;
    if ( dir.back() == '/' )
        return dir + name;
    return dir + "/" + name;
}

} // namespace

BuildController::BuildController(std::string installDir, ProjectBuildConfig config)
    : m_installDir(std::move(installDir))
    , m_config(std::move(config))
{
    const std::string remoteDir = JoinPath(m_installDir, m_config.remoteWorkspace);
    const std::string runDir = JoinPath(m_installDir, m_config.runWorkspace);
    compiler = std::make_unique<RemoteCompilerBackend>(remoteDir, m_config);
    runner = std::make_unique<RemoteRunnerBackend>(runDir);
}

BuildController::~BuildController() = default;

const char* BuildController::CompilerName() const
{
    return compiler ? compiler->Name() : "";
}

const char* BuildController::RunnerName() const
{
    return runner ? runner->Name() : "";
}

std::string BuildController::DeriveOutputBinary(const ProjectBuildConfig& config, const std::string& sourceFile,
    const std::string& workspaceDir)
{
    if ( !config.output.empty() )
        return JoinPath(workspaceDir, config.output);

    if ( sourceFile.empty() )
        return JoinPath(workspaceDir, "hello");

    std::string base = CompilerBasename(sourceFile);
    const std::size_t dot = base.rfind('.');
    if ( dot != std::string::npos )
        base = base.substr(0, dot);
    if ( base.empty() )
        base = "hello";
    return JoinPath(workspaceDir, base);
}

BuildResult BuildController::BuildAndRun(const std::string& sourceFile)
{
    m_lastCompile = BuildResult{};
    m_lastRun = BuildResult{};

    if ( !compiler || !runner )
    {
        BuildResult fail;
        fail.success = false;
        fail.exitCode = -1;
        BuildMessage msg;
        msg.type = BuildMessage::Error;
        msg.text = "build controller backends not initialized";
        fail.messages.push_back(msg);
        return fail;
    }

    m_lastCompile = compiler->Build(sourceFile);
    if ( !m_lastCompile.success )
        return m_lastCompile;

    std::string binary = m_lastCompile.outputBinary;
    if ( binary.empty() )
    {
        const std::string remoteDir = JoinPath(m_installDir, m_config.remoteWorkspace);
        binary = DeriveOutputBinary(m_config, sourceFile, remoteDir);
    }

    m_lastRun = runner->Run(binary);

    BuildResult combined = m_lastRun;
    combined.messages.insert(combined.messages.begin(), m_lastCompile.messages.begin(),
        m_lastCompile.messages.end());
    combined.success = m_lastRun.success;
    return combined;
}
