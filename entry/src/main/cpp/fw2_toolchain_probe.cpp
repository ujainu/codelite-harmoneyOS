/////////////////////////////////////////////////////////////////////////////
// F-5.2: Toolchain detect — clang/ld.lld/make/cmake/ninja on OHOS device.
// Host-only (libentry.so). Does not touch libcodelite_app baseline.
/////////////////////////////////////////////////////////////////////////////

#include "fw2_toolchain_probe.h"

#include <hilog/log.h>

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include <dlfcn.h>
#include <libgen.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xF002
#define LOG_TAG "FW2Host"

namespace {

struct ToolInfo
{
    bool exists = false;
    std::string path;
    std::string version;
};

void LogF(const char* fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    OH_LOG_INFO(LOG_APP, "%{public}s", buf);
    std::fprintf(stderr, "%s\n", buf);
    std::fflush(stderr);
}

bool PathExecutable(const char* path)
{
    return path && path[0] && access(path, X_OK) == 0;
}

const char* ResolveShell()
{
    static const char* kShells[] = { "/bin/sh", "/system/bin/sh", "/data/local/sh" };
    for ( const char* sh : kShells )
    {
        if ( PathExecutable(sh) )
            return sh;
    }
    return nullptr;
}

std::string BundleLibDir()
{
    Dl_info info {};
    if ( dladdr(reinterpret_cast<void*>(&Fw2_ProbeToolchain), &info) == 0 || !info.dli_fname )
        return {};
    char libPath[1024];
    std::snprintf(libPath, sizeof(libPath), "%s", info.dli_fname);
    char* dir = dirname(libPath);
    return dir ? std::string(dir) : std::string();
}

void AppendDirPrefixes(std::vector<std::string>& out, const std::string& dir)
{
    if ( dir.empty() )
        return;
    out.push_back(dir);
    out.push_back(dir + "/tools");
    out.push_back(dir + "/../tools");
    out.push_back(dir + "/native/llvm/bin");
    out.push_back(dir + "/llvm/bin");
}

void BuildSearchPrefixes(std::vector<std::string>& prefixes)
{
    static const char* kFixed[] = {
        "/bin",
        "/system/bin",
        "/usr/bin",
        "/data/local/bin",
        "/data/local/tmp",
        "/storage/Users/currentUser/bin",
    };
    for ( const char* p : kFixed )
        prefixes.emplace_back(p);

    AppendDirPrefixes(prefixes, BundleLibDir());

    if ( const char* pathEnv = std::getenv("PATH") )
    {
        const std::string path = pathEnv;
        size_t start = 0;
        while ( start < path.size() )
        {
            const size_t end = path.find(':', start);
            const std::string part = path.substr(start, end == std::string::npos ? std::string::npos : end - start);
            if ( !part.empty() )
                prefixes.push_back(part);
            if ( end == std::string::npos )
                break;
            start = end + 1;
        }
    }
}

std::string JoinPath(const std::string& dir, const char* name)
{
    if ( dir.empty() )
        return name;
    if ( dir.back() == '/' )
        return dir + name;
    return dir + "/" + name;
}

bool RunShellCapture(const char* shell, const char* cmd, std::string& out, int& exitCode)
{
    out.clear();
    exitCode = -1;
    const std::string full = std::string(shell) + " -c \"" + cmd + "\"";
    FILE* fp = popen(full.c_str(), "r");
    if ( !fp )
        return false;

    char buf[512];
    while ( std::fgets(buf, sizeof(buf), fp) )
        out += buf;

    const int rc = pclose(fp);
    exitCode = WIFEXITED(rc) ? WEXITSTATUS(rc) : rc;
    return true;
}

void TrimLine(std::string& s)
{
    while ( !s.empty() && (s.back() == '\n' || s.back() == '\r' || s.back() == ' ') )
        s.pop_back();
    size_t start = 0;
    while ( start < s.size() && (s[start] == ' ' || s[start] == '\t') )
        ++start;
    if ( start > 0 )
        s.erase(0, start);
}

ToolInfo ProbeToolByPath(const char* name, const std::vector<std::string>& prefixes)
{
    ToolInfo info;
    for ( const std::string& prefix : prefixes )
    {
        const std::string candidate = JoinPath(prefix, name);
        if ( !PathExecutable(candidate.c_str()) )
            continue;
        info.exists = true;
        info.path = candidate;
        return info;
    }
    return info;
}

ToolInfo ProbeToolWhich(const char* shell, const char* name)
{
    ToolInfo info;
    std::string out;
    int exitCode = -1;
    const std::string cmd = std::string("command -v ") + name + " 2>/dev/null || which " + name + " 2>/dev/null";
    if ( !RunShellCapture(shell, cmd.c_str(), out, exitCode) || exitCode != 0 )
        return info;

    TrimLine(out);
    if ( out.empty() || !PathExecutable(out.c_str()) )
        return info;

    info.exists = true;
    info.path = out;
    return info;
}

void ProbeVersion(const char* shell, ToolInfo& tool, const char* versionArg = "--version")
{
    if ( !tool.exists || tool.path.empty() )
        return;

    std::string out;
    int exitCode = -1;
    const std::string cmd = "\"" + tool.path + "\" " + versionArg + " 2>&1 | head -1";
    if ( !RunShellCapture(shell, cmd.c_str(), out, exitCode) )
        return;

    TrimLine(out);
    if ( !out.empty() )
        tool.version = out;
}

ToolInfo ResolveTool(const char* shell, const char* name, const std::vector<std::string>& prefixes)
{
    ToolInfo tool = ProbeToolByPath(name, prefixes);
    if ( !tool.exists )
        tool = ProbeToolWhich(shell, name);
    ProbeVersion(shell, tool);
    return tool;
}

void LogTool(const char* label, const ToolInfo& tool)
{
    if ( tool.exists )
    {
        LogF("[F-5.2] %s path=%s", label, tool.path.c_str());
        if ( !tool.version.empty() )
            LogF("[F-5.2] %s version=%s", label, tool.version.c_str());
    }
    else
    {
        LogF("[F-5.2] %s path=(none)", label);
    }
}

void ScanBundleToolsDir(const std::string& bundleDir)
{
    if ( bundleDir.empty() )
        return;

    const std::string toolsDir = bundleDir + "/tools";
    struct stat st {};
    if ( stat(toolsDir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode) )
    {
        LogF("[F-5.2] bundle tools dir=%s exists=0", toolsDir.c_str());
        return;
    }

    LogF("[F-5.2] bundle tools dir=%s exists=1", toolsDir.c_str());
    static const char* kNames[] = { "clang++", "clang", "ld.lld", "cmake", "make", "ninja" };
    for ( const char* name : kNames )
    {
        const std::string p = JoinPath(toolsDir, name);
        if ( PathExecutable(p.c_str()) )
            LogF("[F-5.2] bundle tool %s=%s exec=1", name, p.c_str());
    }
}

} // namespace

void Fw2_ProbeToolchain()
{
    LogF("[F-5.2] probe enter");

    const char* shell = ResolveShell();
    if ( !shell )
    {
        LogF("[F-5.2] shell=FAIL");
        LogF("[F-5.2] compiler_available=0");
        LogF("[F-5.2] FAIL");
        return;
    }
    LogF("[F-5.2] shell=OK path=%s", shell);

    const std::string bundleDir = BundleLibDir();
    if ( !bundleDir.empty() )
        LogF("[F-5.2] bundle_lib_dir=%s", bundleDir.c_str());
    ScanBundleToolsDir(bundleDir);

    std::vector<std::string> prefixes;
    BuildSearchPrefixes(prefixes);

    const ToolInfo clangxx = ResolveTool(shell, "clang++", prefixes);
    const ToolInfo clang = ResolveTool(shell, "clang", prefixes);
    const ToolInfo lld = ResolveTool(shell, "ld.lld", prefixes);
    const ToolInfo cmake = ResolveTool(shell, "cmake", prefixes);
    const ToolInfo make = ResolveTool(shell, "make", prefixes);
    const ToolInfo ninja = ResolveTool(shell, "ninja", prefixes);

    LogTool("clang++", clangxx);
    LogTool("clang", clang);
    LogTool("ld.lld", lld);
    LogTool("cmake", cmake);
    LogTool("make", make);
    LogTool("ninja", ninja);

    const int compilerAvailable = (clangxx.exists || clang.exists) ? 1 : 0;
    LogF("[F-5.2] compiler_available=%d", compilerAvailable);
    LogF("[F-5.2] OK");
}
