#pragma once

#include "compiler_types.h"

#include <string>

class CompilerBackend
{
public:
    virtual ~CompilerBackend() = default;

    virtual BuildResult Build(const std::string& sourceFile) = 0;

    virtual const char* Name() const = 0;
};
