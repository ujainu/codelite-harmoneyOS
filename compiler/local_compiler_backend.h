#pragma once

#include "compiler_backend.h"

// F-5.4: device-local clang++ when toolchain becomes available (compiler_available=1).
class LocalCompilerBackend : public CompilerBackend
{
public:
    BuildResult Build(const std::string& sourceFile) override;

    const char* Name() const override { return "LocalCompiler"; }
};
