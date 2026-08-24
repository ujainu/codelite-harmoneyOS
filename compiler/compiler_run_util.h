/////////////////////////////////////////////////////////////////////////////
// F-5.4: shared process capture for compiler backends (host libentry only).
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include "compiler_types.h"

#include <string>

const char* CompilerResolveShell();

bool CompilerEnsureDir(const std::string& dir);

bool CompilerCopyFile(const std::string& src, const std::string& dst);

bool CompilerWriteTextFile(const std::string& path, const std::string& text);

bool CompilerReadTextFile(const std::string& path, std::string& out);

BuildResult CompilerRunShellCapture(const char* shell, const char* script);

BuildResult CompilerRunBinaryCapture(const std::string& binaryPath, const std::string& workDir = {});

bool CompilerParseRemoteResultFile(const std::string& path, BuildResult& out);

bool CompilerParseRemoteResultText(const std::string& text, BuildResult& out);

std::string CompilerBasename(const std::string& path);
