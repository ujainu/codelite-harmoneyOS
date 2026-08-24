// F-4: Interpose clStandardPaths::GetPluginsDirectory for OHOS when liblibcodelite
// still points at el2/filesDir. Resolved lazily from libcodelite_app PLT.
#include <wx/string.h>

#include <cstdlib>
#include <dlfcn.h>
#include <libgen.h>
#include <cstdio>

struct clStandardPaths;

namespace {

wxString OhosBundlePluginsDir()
{
    if ( const char* env = std::getenv("CODELITE_OHOS_PLUGINS_DIR") )
    {
        if ( env[0] )
            return wxString(env, wxConvUTF8);
    }

    Dl_info info {};
    if ( dladdr(reinterpret_cast<void*>(&OhosBundlePluginsDir), &info) != 0 && info.dli_fname )
    {
        char libPath[1024];
        std::snprintf(libPath, sizeof(libPath), "%s", info.dli_fname);
        if ( char* dir = dirname(libPath) )
            return wxString(dir, wxConvUTF8);
    }
    return wxString();
}

using GetPluginsDirectoryFn = wxString (*)(const clStandardPaths*);

GetPluginsDirectoryFn RealGetPluginsDirectory()
{
    static GetPluginsDirectoryFn fn = reinterpret_cast<GetPluginsDirectoryFn>(
        dlsym(RTLD_NEXT, "_ZNK15clStandardPaths19GetPluginsDirectoryEv"));
    return fn;
}

} // namespace

extern "C" __attribute__((visibility("default")))
wxString _ZNK15clStandardPaths19GetPluginsDirectoryEv(const clStandardPaths* self)
{
    const wxString bundleDir = OhosBundlePluginsDir();
    if ( !bundleDir.empty() )
        return bundleDir;

    if ( GetPluginsDirectoryFn real = RealGetPluginsDirectory() )
        return real(self);

    return wxString();
}
