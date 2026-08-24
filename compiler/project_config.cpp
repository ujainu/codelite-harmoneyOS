/////////////////////////////////////////////////////////////////////////////
// F-8.3: Minimal build.json parser (no external JSON dependency).
/////////////////////////////////////////////////////////////////////////////

#include "project_config.h"

#include "compiler_run_util.h"

#include <cctype>
#include <string>

namespace {

std::string Trim(const std::string& s)
{
    std::size_t b = 0;
    while ( b < s.size() && std::isspace(static_cast<unsigned char>(s[b])) )
        ++b;
    std::size_t e = s.size();
    while ( e > b && std::isspace(static_cast<unsigned char>(s[e - 1])) )
        --e;
    return s.substr(b, e - b);
}

bool ExtractJsonString(const std::string& json, const std::string& key, std::string& out)
{
    const std::string needle = "\"" + key + "\"";
    const std::size_t pos = json.find(needle);
    if ( pos == std::string::npos )
        return false;
    const std::size_t colon = json.find(':', pos + needle.size());
    if ( colon == std::string::npos )
        return false;
    const std::size_t q1 = json.find('"', colon + 1);
    if ( q1 == std::string::npos )
        return false;
    const std::size_t q2 = json.find('"', q1 + 1);
    if ( q2 == std::string::npos )
        return false;
    out = json.substr(q1 + 1, q2 - q1 - 1);
    return true;
}

bool ExtractJsonStringArray(const std::string& json, const std::string& key, std::vector<std::string>& out)
{
    out.clear();
    const std::string needle = "\"" + key + "\"";
    const std::size_t pos = json.find(needle);
    if ( pos == std::string::npos )
        return false;
    const std::size_t bracket = json.find('[', pos);
    if ( bracket == std::string::npos )
        return false;
    const std::size_t end = json.find(']', bracket);
    if ( end == std::string::npos )
        return false;
    const std::string slice = json.substr(bracket + 1, end - bracket - 1);
    std::size_t i = 0;
    while ( i < slice.size() )
    {
        const std::size_t q1 = slice.find('"', i);
        if ( q1 == std::string::npos )
            break;
        const std::size_t q2 = slice.find('"', q1 + 1);
        if ( q2 == std::string::npos )
            break;
        out.push_back(slice.substr(q1 + 1, q2 - q1 - 1));
        i = q2 + 1;
    }
    return !out.empty();
}

std::string ParentDir(const std::string& path)
{
    const std::size_t slash = path.find_last_of('/');
    if ( slash == std::string::npos )
        return {};
    return path.substr(0, slash);
}

} // namespace

std::vector<std::string> ProjectBuildConfig::EffectiveFlags() const
{
    std::vector<std::string> all = flags;
    if ( mode == "debug" )
        all.push_back("-g");
    else if ( mode == "release" )
        all.push_back("-O2");
    return all;
}

std::string ProjectBuildConfig::FormatCompileCommand(const std::string& sourceBase) const
{
    std::string cmd = compiler + " " + sourceBase;
    for ( const std::string& flag : EffectiveFlags() )
        cmd += " " + flag;
    cmd += " -o " + output;
    return cmd;
}

bool LoadProjectBuildConfig(const std::string& buildJsonPath, ProjectBuildConfig& out)
{
    std::string json;
    if ( !CompilerReadTextFile(buildJsonPath, json) )
        return false;

    ProjectBuildConfig cfg;
    std::string value;
    if ( ExtractJsonString(json, "profile", value) )
        cfg.profile = value;
    if ( ExtractJsonString(json, "compiler", value) )
    {
        if ( value != "RemoteCompiler" )
            cfg.compiler = value;
    }
    if ( ExtractJsonString(json, "runner", value) )
        cfg.runner = value;
    if ( ExtractJsonString(json, "mode", value) )
        cfg.mode = value;
    if ( ExtractJsonString(json, "target", value) )
        cfg.target = value;
    if ( ExtractJsonString(json, "type", value) )
        cfg.type = value;
    if ( ExtractJsonString(json, "source", value) )
        cfg.source = value;
    if ( ExtractJsonString(json, "output", value) )
        cfg.output = value;
    if ( ExtractJsonString(json, "remoteWorkspace", value) )
        cfg.remoteWorkspace = value;
    if ( ExtractJsonString(json, "runWorkspace", value) )
        cfg.runWorkspace = value;

    std::vector<std::string> parsedFlags;
    if ( ExtractJsonStringArray(json, "flags", parsedFlags) )
        cfg.flags = std::move(parsedFlags);

    out = std::move(cfg);
    return true;
}

std::string FindBuildJsonForSource(const std::string& sourceFile)
{
    if ( sourceFile.empty() )
        return {};
    const std::string dir = ParentDir(sourceFile);
    if ( dir.empty() )
        return {};
    return dir + "/build.json";
}
