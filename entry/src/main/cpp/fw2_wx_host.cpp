/////////////////////////////////////////////////////////////////////////////
// Host: bootstrap official CodeLite MainFrame — NOT a demo/test wxFrame.
//
// Requires HarmonyCodeLite_EmbeddedStart from libcodelite_app.so (Minimal Boot).
/////////////////////////////////////////////////////////////////////////////

#include "fw2_wx_host.h"

#include <hilog/log.h>

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>

#include <sys/stat.h>
#include <unistd.h>

#include <dlfcn.h>
#include <libgen.h>
#include <link.h>
#include <cstdlib>

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/stdpaths.h"
    #include "wx/filename.h"
    #include "wx/menu.h"
#if defined(__WXOHOS__)
    #include "wx/frame.h"
#else
    #include "wx/univ/frame.h"
#endif
#endif

// wx/wx.h 不包含 stdpaths.h 和 filename.h（这俩头不随 WX_PRECOMP PCH 带入）。
// 当 WX_PRECOMP 打开时上面 guard 被跳过，因此这里无条件补 include，
// 保证 Fw2_EnsureWxReady 中 wxStandardPaths::Get().GetExecutablePath() 可链接。
#include "wx/stdpaths.h"
#include "wx/filename.h"

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xF002
#define LOG_TAG "FW2Host"

// Strong link to official CodeLiteApp (libcodelite_app.so). No weak / no demo frame.
extern "C" int HarmonyCodeLite_EmbeddedStart(int argc, char** argv);
extern "C" int HarmonyCodeLite_EmbeddedRun(void);
extern "C" void HarmonyCodeLite_EmbeddedShutdown(void);

namespace {
bool g_wxStarted = false;
std::string g_installDir;
std::thread g_runThread;
std::atomic<bool> g_runThreadStarted{false};

bool Fw2_LoadWxTooltipStub()
{
    static bool s_loaded = false;
    if ( s_loaded )
        return true;

    dlerror();
    if ( dlopen("libwx_ohosu_tooltip_stub.so", RTLD_NOW | RTLD_GLOBAL) )
    {
        OH_LOG_INFO(LOG_APP, "[tooltip-fix] dlopen OK libwx_ohosu_tooltip_stub.so");
        s_loaded = true;
        return true;
    }

    Dl_info info {};
    if ( dladdr(reinterpret_cast<void*>(&Fw2_SetInstallDir), &info) != 0 && info.dli_fname )
    {
        char libPath[1024];
        std::snprintf(libPath, sizeof(libPath), "%s", info.dli_fname);
        if ( char* libdir = dirname(libPath) )
        {
            const std::string path = std::string(libdir) + "/libwx_ohosu_tooltip_stub.so";
            dlerror();
            if ( dlopen(path.c_str(), RTLD_NOW | RTLD_GLOBAL) )
            {
                OH_LOG_INFO(LOG_APP, "[tooltip-fix] dlopen OK %{public}s", path.c_str());
                s_loaded = true;
                return true;
            }
            const char* err = dlerror();
            OH_LOG_ERROR(LOG_APP, "[tooltip-fix] dlopen FAIL %{public}s err=%{public}s",
                         path.c_str(), err ? err : "unknown");
        }
    }

    return false;
}

class wxGraphicsRenderer;

namespace {

constexpr uintptr_t kWxOhosRendererSlotRva = 0x794548;

struct WxCorePhdrQuery
{
    uintptr_t base = 0;
};

int WxCorePhdrCallback(struct dl_phdr_info* info, size_t, void* data)
{
    auto* query = static_cast<WxCorePhdrQuery*>(data);
    if ( !info || !info->dlpi_name || !info->dlpi_name[0] )
        return 0;
    if ( std::strstr(info->dlpi_name, "libwx_ohosu_core") == nullptr )
        return 0;
    query->base = static_cast<uintptr_t>(info->dlpi_addr);
    return 1;
}

uintptr_t Fw2_FindWxCoreBase()
{
    WxCorePhdrQuery query;
    dl_iterate_phdr(WxCorePhdrCallback, &query);
    return query.base;
}

} // namespace

bool Fw2_LoadWxGraphicsRenderer()
{
    static bool s_loaded = false;
    if ( s_loaded )
        return true;

    auto try_dlopen = [](const char* name) -> void* {
        dlerror();
        return dlopen(name, RTLD_NOW | RTLD_GLOBAL);
    };

    void* graphics = try_dlopen("libwx_ohos_graphics.so");
    if ( !graphics )
    {
        Dl_info info {};
        if ( dladdr(reinterpret_cast<void*>(&Fw2_SetInstallDir), &info) != 0 && info.dli_fname )
        {
            char libPath[1024];
            std::snprintf(libPath, sizeof(libPath), "%s", info.dli_fname);
            if ( char* libdir = dirname(libPath) )
            {
                const std::string path = std::string(libdir) + "/libwx_ohos_graphics.so";
                graphics = try_dlopen(path.c_str());
            }
        }
    }

    if ( !graphics )
    {
        const char* err = dlerror();
        OH_LOG_ERROR(LOG_APP, "[FUI_TRACE] wxOHOS graphics dlopen FAIL err=%{public}s",
                     err ? err : "unknown");
        return false;
    }

    dlerror();
    using RendererGetterFn = wxGraphicsRenderer*(*)();
    auto getter = reinterpret_cast<RendererGetterFn>(dlsym(graphics, "wxOhosGetGraphicsRenderer"));
    if ( !getter )
    {
        const char* err = dlerror();
        OH_LOG_ERROR(LOG_APP, "[FUI_TRACE] wxOhosGetGraphicsRenderer missing err=%{public}s",
                     err ? err : "unknown");
        return false;
    }

    wxGraphicsRenderer* renderer = getter();
    if ( !renderer )
    {
        OH_LOG_ERROR(LOG_APP, "[FUI_TRACE] wxOhosGetGraphicsRenderer returned null");
        return false;
    }

    const uintptr_t wxBase = Fw2_FindWxCoreBase();
    if ( !wxBase )
    {
        OH_LOG_ERROR(LOG_APP, "[FUI_TRACE] libwx_ohosu_core base not found for renderer slot");
        return false;
    }

    auto** slot = reinterpret_cast<wxGraphicsRenderer**>(wxBase + kWxOhosRendererSlotRva);
    *slot = renderer;
    OH_LOG_INFO(LOG_APP, "[FUI_TRACE] wxOHOS GraphicsRenderer slot=%{public}p renderer=%{public}p",
                static_cast<void*>(slot), static_cast<void*>(renderer));
    s_loaded = true;
    return true;
}

bool CopyFileSimple(const std::string& src, const std::string& dest)
{
    FILE* in = std::fopen(src.c_str(), "rb");
    if ( !in )
        return false;
    FILE* out = std::fopen(dest.c_str(), "wb");
    if ( !out )
    {
        std::fclose(in);
        return false;
    }

    char buf[4096];
    size_t n = 0;
    while ( (n = std::fread(buf, 1, sizeof(buf), in)) > 0 )
    {
        if ( std::fwrite(buf, 1, n, out) != n )
        {
            std::fclose(in);
            std::fclose(out);
            return false;
        }
    }

    std::fclose(in);
    std::fclose(out);
    return true;
}

void Fw2_SeedF1TestFile(const std::string& installDir)
{
    if ( installDir.empty() )
        return;

    const std::string src = installDir + "/samples/test.cpp";
    struct stat st {};
    if ( stat(src.c_str(), &st) != 0 || !S_ISREG(st.st_mode) )
    {
        OH_LOG_WARN(LOG_APP, "[F-1.4] seed skipped: missing %{public}s", src.c_str());
        return;
    }

    const char* home = std::getenv("HOME");
    if ( !home || !home[0] )
    {
        OH_LOG_WARN(LOG_APP, "[F-1.4] seed skipped: HOME unset");
        return;
    }

    const std::string destDir = std::string(home) + "/.codelite";
    mkdir(destDir.c_str(), 0755);
    const std::string dest = destDir + "/test.cpp";
    if ( !CopyFileSimple(src, dest) )
    {
        OH_LOG_ERROR(LOG_APP, "[F-1.4] seed copy failed src=%{public}s dest=%{public}s",
                     src.c_str(), dest.c_str());
        return;
    }

    OH_LOG_INFO(LOG_APP, "[F-1.4] seeded test.cpp → %{public}s", dest.c_str());
}

void Fw2_SeedF3SampleWorkspace(const std::string& installDir)
{
    if ( installDir.empty() )
        return;

    const std::string srcDir = installDir + "/samples/f3-sample";
    struct stat st {};
    if ( stat(srcDir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode) )
    {
        OH_LOG_WARN(LOG_APP, "[F-3.3] seed skipped: missing %{public}s", srcDir.c_str());
        return;
    }

    const char* home = std::getenv("HOME");
    if ( !home || !home[0] )
    {
        OH_LOG_WARN(LOG_APP, "[F-3.3] seed skipped: HOME unset");
        return;
    }

    const std::string destDir = std::string(home) + "/.codelite/f3-sample";
    mkdir((std::string(home) + "/.codelite").c_str(), 0755);
    mkdir(destDir.c_str(), 0755);

    static const char* kFiles[] = {
        "f3-sample.workspace",
        "f3-sample.project",
        "main.cpp",
    };

    for ( const char* name : kFiles )
    {
        const std::string src = srcDir + "/" + name;
        const std::string dest = destDir + "/" + name;
        if ( stat(src.c_str(), &st) != 0 || !S_ISREG(st.st_mode) )
        {
            OH_LOG_WARN(LOG_APP, "[F-3.3] seed missing file %{public}s", src.c_str());
            continue;
        }
        if ( !CopyFileSimple(src, dest) )
        {
            OH_LOG_ERROR(LOG_APP, "[F-3.3] seed copy failed src=%{public}s dest=%{public}s",
                         src.c_str(), dest.c_str());
            return;
        }
    }

    OH_LOG_INFO(LOG_APP, "[F-3.seed] seeded f3-sample → %{public}s", destDir.c_str());
}

void Fw2_SeedF4Plugins(const std::string& installDir)
{
    wxUnusedVar(installDir);

    Dl_info info {};
    if ( dladdr(reinterpret_cast<void*>(&Fw2_SetInstallDir), &info) == 0 || !info.dli_fname )
    {
        OH_LOG_WARN(LOG_APP, "[F-4.seed] skipped: dladdr failed");
        return;
    }

    char libPath[1024];
    std::snprintf(libPath, sizeof(libPath), "%s", info.dli_fname);
    char* dir = dirname(libPath);
    if ( !dir || !dir[0] )
    {
        OH_LOG_WARN(LOG_APP, "[F-4.seed] skipped: bundle lib dir unknown");
        return;
    }

    const char* pluginNames[] = {"HelpPlugin.dll", "EditorConfigPlugin.dll"};
    int present = 0;
    for ( const char* name : pluginNames )
    {
        const std::string path = std::string(dir) + "/" + name;
        struct stat st {};
        if ( stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode) )
        {
            ++present;
            OH_LOG_INFO(LOG_APP, "[F-4.seed] bundle plugin %{public}s size=%{public}ld",
                        name, static_cast<long>(st.st_size));
        }
        else
        {
            OH_LOG_WARN(LOG_APP, "[F-4.seed] missing bundle plugin %{public}s", path.c_str());
        }
    }

    OH_LOG_INFO(LOG_APP, "[F-4.1] OK bundle plugins present=%{public}d dir=%{public}s", present, dir);

    if ( present > 0 && dir[0] )
        setenv("CODELITE_OHOS_PLUGINS_DIR", dir, 1);
}

void Fw2_ProbeF4PluginsDir(const std::string& installDir)
{
    if ( installDir.empty() )
        return;

    const std::string dir = installDir + "/plugins";
    const char* names[] = {"HelpPlugin.dll", "EditorConfigPlugin.dll"};
    int present = 0;
    for ( const char* name : names )
    {
        const std::string path = dir + "/" + name;
        struct stat st {};
        if ( stat(path.c_str(), &st) == 0 )
        {
            ++present;
            OH_LOG_INFO(LOG_APP, "[F-4.2] OK plugin file %{public}s size=%{public}ld mode=%{public}o",
                        name, static_cast<long>(st.st_size), static_cast<unsigned>(st.st_mode & 0777));
        }
        else
        {
            OH_LOG_WARN(LOG_APP, "[F-4.2] missing plugin file %{public}s", path.c_str());
        }
    }

    OH_LOG_INFO(LOG_APP, "[F-4.1] probe pluginsDir=%{public}s present=%{public}d", dir.c_str(), present);

    Dl_info bundle {};
    if ( dladdr(reinterpret_cast<void*>(&Fw2_SetInstallDir), &bundle) != 0 && bundle.dli_fname )
    {
        char libPath[1024];
        std::snprintf(libPath, sizeof(libPath), "%s", bundle.dli_fname);
        if ( char* libdir = dirname(libPath) )
        {
            OH_LOG_INFO(LOG_APP, "[F-4.path] bundle plugin search path=%{public}s", libdir);
            for ( const char* name : names )
            {
                const std::string p = std::string(libdir) + "/" + name;
                struct stat pst {};
                if ( stat(p.c_str(), &pst) == 0 )
                    OH_LOG_INFO(LOG_APP, "[F-4.scan] found %{public}s", name);
            }
        }
    }

    auto tryDlopen = [](const char* label, const std::string& path) {
        dlerror();
        void* handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        if ( handle )
        {
            OH_LOG_INFO(LOG_APP, "[F-4.3] OK dlopen %{public}s", label);
            void* symInfo = dlsym(handle, "GetPluginInfo");
            void* symCreate = dlsym(handle, "CreatePlugin");
            OH_LOG_INFO(LOG_APP, "[F-4.4] symbols GetPluginInfo=%{public}d CreatePlugin=%{public}d",
                        symInfo ? 1 : 0, symCreate ? 1 : 0);
            dlclose(handle);
            return true;
        }
        const char* err = dlerror();
        OH_LOG_ERROR(LOG_APP, "[F-4.3] FAIL dlopen %{public}s path=%{public}s err=%{public}s",
                     label, path.c_str(), err ? err : "unknown");
        return false;
    };

    tryDlopen("HelpPlugin.dll", dir + "/HelpPlugin.dll");

    if ( wxTheApp )
    {
        const wxString binFolder = wxFileName(wxStandardPaths::Get().GetExecutablePath()).GetPath();
        OH_LOG_INFO(LOG_APP, "[F-4.1] app BinFolder=%{public}s", binFolder.utf8_str().data());
        OH_LOG_INFO(LOG_APP, "[F-4.1] app PluginsDirectory(el2)=%{public}s", dir.c_str());
    }

    Dl_info info {};
    if ( dladdr(reinterpret_cast<void*>(&Fw2_SetInstallDir), &info) != 0 && info.dli_fname )
    {
        char libPath[1024];
        std::snprintf(libPath, sizeof(libPath), "%s", info.dli_fname);
        char* libdir = dirname(libPath);
        if ( libdir && libdir[0] )
            tryDlopen("bundle HelpPlugin.dll", std::string(libdir) + "/HelpPlugin.dll");
    }

#if wxUSE_MENUS
    if ( wxTheApp )
    {
        if ( wxFrame* frame = wxDynamicCast(wxTheApp->GetTopWindow(), wxFrame) )
        {
            if ( wxMenuBar* bar = frame->GetMenuBar() )
            {
                wxMenu* pluginsMenu = nullptr;
                for ( size_t i = 0; i < bar->GetMenuCount(); ++i )
                {
                    const wxString label = bar->GetMenuLabelText(i);
                    if ( label.Contains("Plugin") )
                    {
                        pluginsMenu = bar->GetMenu(i);
                        break;
                    }
                }
                const size_t items = pluginsMenu ? pluginsMenu->GetMenuItemCount() : 0;
                OH_LOG_INFO(LOG_APP, "[F-4.5] OK pluginsMenu items=%{public}zu", items);
            }
        }
    }
#endif
}
}

void Fw2_SetInstallDir(const char* installDir)
{
    if ( installDir && installDir[0] )
        g_installDir = installDir;
    else
        g_installDir.clear();
    OH_LOG_INFO(LOG_APP, "[B-4.3] Host InstallDir=%{public}s",
                g_installDir.empty() ? "(unset)" : g_installDir.c_str());
    Fw2_SeedF1TestFile(g_installDir);
    Fw2_SeedF3SampleWorkspace(g_installDir);
    Fw2_SeedF4Plugins(g_installDir);
}

const char* Fw2_GetInstallDir()
{
    return g_installDir.c_str();
}

bool Fw2_EnsureWxReady()
{
    if ( g_wxStarted && wxTheApp && wxTheApp->GetTopWindow() )
        return true;

    static char arg0[] = "codelite";
    static char basedirArg[1024];
    char* argv_store[3] = { arg0, nullptr, nullptr };
    int argc = 1;

    if ( !g_installDir.empty() ) {
        std::snprintf(basedirArg, sizeof(basedirArg), "--basedir=%s", g_installDir.c_str());
        argv_store[1] = basedirArg;
        argc = 2;
        OH_LOG_INFO(LOG_APP, "[B-4.3] EmbeddedStart with %{public}s", basedirArg);
    }

    OH_LOG_INFO(LOG_APP, "[B-1] calling HarmonyCodeLite_EmbeddedStart");
    if ( !Fw2_LoadWxGraphicsRenderer() ) {
        OH_LOG_ERROR(LOG_APP, "[FUI_TRACE] wxGraphicsRenderer preload failed — GetBestXButtonSize may hang");
    }
    if ( !Fw2_LoadWxTooltipStub() ) {
        OH_LOG_ERROR(LOG_APP, "[tooltip-fix] wxToolTip stub missing — clMainFrame may SIGSEGV");
    }
    if ( !HarmonyCodeLite_EmbeddedStart(argc, argv_store) ) {
        OH_LOG_ERROR(LOG_APP, "Minimal Boot FAIL: EmbeddedStart returned 0 (first failing B-n in CodeLite logs)");
        return false;
    }

    g_wxStarted = true;
    OH_LOG_INFO(LOG_APP, "Minimal Boot: CodeLiteApp ready top=%{public}p",
                static_cast<void*>(wxTheApp ? wxTheApp->GetTopWindow() : nullptr));
    Fw2_ProbeF4PluginsDir(g_installDir);
    return wxTheApp && wxTheApp->GetTopWindow();
}

int Fw2_EmbeddedRun()
{
    if ( !g_wxStarted || !wxTheApp ) {
        OH_LOG_ERROR(LOG_APP, "[BOOT-001] FAIL EmbeddedRun: wx not ready");
        return -1;
    }
    // BOOT-001 phase-2: MainLoop must stay resident without freezing Ability UI thread.
    // Host still owns lifecycle; wx owns the loop (no second Host while).
    if ( g_runThreadStarted.exchange(true) ) {
        OH_LOG_INFO(LOG_APP, "[BOOT-001] phase-2 EmbeddedRun already started");
        return 0;
    }

    OH_LOG_INFO(LOG_APP, "[BOOT-001] phase-2 Host → EmbeddedRun on dedicated thread");
    g_runThread = std::thread([]() {
        OH_LOG_INFO(LOG_APP, "[BOOT-001] phase-2 run thread enter");
        const int rc = HarmonyCodeLite_EmbeddedRun();
        OH_LOG_INFO(LOG_APP, "[BOOT-001] phase-2 run thread leave rc=%{public}d", rc);
    });
    // Nudge wx pending queue (OnInit / PostConstruct CallAfter) once MainLoop thread starts.
    if ( wxTheApp ) {
        for ( int i = 0; i < 8; ++i ) {
            wxTheApp->WakeUpIdle();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    }
    return 0;
}

void Fw2_ShutdownWx()
{
    if ( !g_wxStarted )
        return;

    if ( wxTheApp )
        wxTheApp->ExitMainLoop();

    if ( g_runThread.joinable() )
        g_runThread.join();
    g_runThreadStarted = false;

    HarmonyCodeLite_EmbeddedShutdown();
    g_wxStarted = false;
    OH_LOG_INFO(LOG_APP, "MainFrame: CodeLite embedded shutdown");
}

void* Fw2_GetTopFrame()
{
    if ( !wxTheApp )
        return nullptr;
    return static_cast<void*>(wxTheApp->GetTopWindow());
}
