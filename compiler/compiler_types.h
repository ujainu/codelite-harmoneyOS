#pragma once

#include <string>
#include <vector>

struct BuildMessage
{
    enum Type
    {
        Info,
        Warning,
        Error
    };

    Type type = Info;
    std::string text;
};

struct BuildResult
{
    int exitCode = -1;
    bool success = false;
    std::vector<BuildMessage> messages;
    std::string stdoutText;
    std::string stderrText;
    std::string outputBinary;
};
