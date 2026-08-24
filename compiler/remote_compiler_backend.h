#pragma once

#include "compiler_backend.h"
#include "project_config.h"

#include <string>

// F-5.4.1: compile via PC DevEco toolchain + hdc staging (result file or helper script).
class RemoteCompilerBackend : public CompilerBackend
{
public:
    RemoteCompilerBackend(std::string workspaceDir, ProjectBuildConfig config = {});

    BuildResult Build(const std::string& sourceFile) override;

    const char* Name() const override { return "RemoteCompiler"; }

    const ProjectBuildConfig& Config() const { return m_config; }

private:
    std::string m_workspaceDir;
    ProjectBuildConfig m_config;
};
