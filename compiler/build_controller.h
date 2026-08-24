#pragma once

#include "compiler_types.h"
#include "project_config.h"

#include <memory>
#include <string>

class CompilerBackend;
class RunnerBackend;

// F-5.6: unified compile-then-run entry for host bridge (no CodeLite BuildManager).
class BuildController
{
public:
    explicit BuildController(std::string installDir, ProjectBuildConfig config = {});
    ~BuildController();

    BuildResult BuildAndRun(const std::string& sourceFile);

    const BuildResult& LastCompileResult() const { return m_lastCompile; }
    const BuildResult& LastRunResult() const { return m_lastRun; }

    const char* CompilerName() const;
    const char* RunnerName() const;

private:
    static std::string DeriveOutputBinary(const ProjectBuildConfig& config, const std::string& sourceFile,
        const std::string& workspaceDir);

    std::string m_installDir;
    ProjectBuildConfig m_config;
    std::unique_ptr<CompilerBackend> compiler;
    std::unique_ptr<RunnerBackend> runner;
    BuildResult m_lastCompile;
    BuildResult m_lastRun;
};
