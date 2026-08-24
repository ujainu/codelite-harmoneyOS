#pragma once

#include "compiler_types.h"

#include <string>

class RunnerBackend
{
public:
    virtual ~RunnerBackend() = default;

    virtual BuildResult Run(const std::string& binaryPath) = 0;

    virtual const char* Name() const = 0;
};

// F-5.5: run PC-built OHOS binary staged via hdc under /data/local/tmp/fw2_run/.
class RemoteRunnerBackend : public RunnerBackend
{
public:
    explicit RemoteRunnerBackend(std::string runDir);

    BuildResult Run(const std::string& binaryPath) override;

    const char* Name() const override { return "RemoteRunner"; }

private:
    std::string m_runDir;
};
