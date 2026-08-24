#pragma once

#include <string>
#include <vector>

// F-8.3: build.json → compiler invocation (host libentry only).
struct ProjectBuildConfig
{
    std::string profile = "remote-ohos-aarch64";
    std::string compiler = "clang++";
    std::string runner = "RemoteRunner";
    std::string mode;
    std::string target = "arm64";
    std::string type = "console";
    std::string source = "main.cpp";
    std::string output = "main";
    std::string remoteWorkspace = "build/remote";
    std::string runWorkspace = "build/run";
    std::vector<std::string> flags;

    std::vector<std::string> EffectiveFlags() const;
    std::string FormatCompileCommand(const std::string& sourceBase) const;
};

bool LoadProjectBuildConfig(const std::string& buildJsonPath, ProjectBuildConfig& out);

// .../HelloWorld/main.cpp → .../HelloWorld/build.json
std::string FindBuildJsonForSource(const std::string& sourceFile);
