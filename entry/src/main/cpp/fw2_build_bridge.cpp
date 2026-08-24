/////////////////////////////////////////////////////////////////////////////
// F-5.6.5: Build bridge — Harmony menu → BuildController (libentry.so).
/////////////////////////////////////////////////////////////////////////////

#include "fw2_build_bridge.h"

#include "build_controller.h"
#include "project_config.h"
#include "runner_backend.h"

#include "fw2_project_bridge.h"
#include "fw2_wx_host.h"

#include <hilog/log.h>

#include <cstdarg>
#include <cstdio>
#include <string>

#include <unistd.h>

#include <atomic>

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/frame.h"
    #include "wx/menu.h"
    #include "wx/menuitem.h"
    #include "wx/toplevel.h"
    #include "wx/xrc/xmlres.h"
#endif

// wx/xrc/xmlres.h 不在 wx/wx.h 的默认 include 链里，WX_PRECOMP 打开时
// 上面的 guard 被跳过，无条件补 include，保证 wxXmlResource::Get() 声明可见。
#include "wx/xrc/xmlres.h"

#include "wx/ohos/toplevel.h"
#include "wx/ohos/bitmap.h"

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xF002
#define LOG_TAG "FW2Host"

extern "C" void HarmonyCodeLite_F565_RegisterBuildBridge(void (*buildFn)(const char*),
                                                         void (*runFn)()) __attribute__((weak));
extern "C" int HarmonyCodeLite_F565_ProbeHarmonyMenuOpen() __attribute__((weak));
extern "C" int HarmonyCodeLite_F565_TriggerBuildRemote() __attribute__((weak));
extern "C" void* HarmonyCodeLite_F565_GetMainMenuBar() __attribute__((weak));

class clMainFrame : public wxFrame
{
public:
    static clMainFrame* Get();
};

namespace {

static const int kHarmonyBuildRemote = 59001;
static const int kHarmonyRunRemote = 59002;

// SetMenuBar runs on OHOS init thread; MainLoop/CallAfter runs on EmbeddedRun thread.
// Cache the attached bar so worker-side bridge code can reattach on the loop thread.
std::atomic<void*> g_mainMenuBarCache{nullptr};

void CacheMainMenuBar(wxMenuBar* bar)
{
    if ( bar )
        g_mainMenuBarCache.store(static_cast<void*>(bar), std::memory_order_release);
}

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

std::string JoinPath(const std::string& dir, const std::string& name)
{
    if ( dir.empty() )
        return name;
    if ( dir.back() == '/' )
        return dir + name;
    return dir + "/" + name;
}

std::string DefaultSourceFile(const std::string& installDir, const ProjectBuildConfig& config)
{
    const std::string remoteDir = JoinPath(installDir, config.remoteWorkspace);
    const std::string configured = JoinPath(remoteDir, config.source.empty() ? "main.cpp" : config.source);
    if ( access(configured.c_str(), R_OK) == 0 )
        return configured;
    return JoinPath(remoteDir, "hello.cpp");
}

std::string DefaultBinary(const std::string& installDir, const ProjectBuildConfig& config)
{
    const std::string remoteDir = JoinPath(installDir, config.remoteWorkspace);
    const std::string outName = config.output.empty() ? "hello" : config.output;
    return JoinPath(remoteDir, outName);
}

ProjectBuildConfig LoadBuildConfig(const std::string& installDir, const std::string& sourceFile)
{
    ProjectBuildConfig config;
    std::string buildJsonPath = FindBuildJsonForSource(sourceFile);
    if ( buildJsonPath.empty() )
        buildJsonPath = JoinPath(installDir, "build/remote/build.json");
    if ( LoadProjectBuildConfig(buildJsonPath, config) )
        LogF("[F-8.3] loaded build.json from %s", buildJsonPath.c_str());
    else
        LogF("[F-8.3] using default build config (no build.json at %s)", buildJsonPath.c_str());
    return config;
}

void LogBuildRunResult(const BuildController& controller, const BuildResult& result)
{
    LogF("[F-5.6] controller=create");
    LogF("[F-5.6] compiler=%s", controller.CompilerName());
    for ( const BuildMessage& msg : controller.LastCompileResult().messages )
        LogF("[F-5.6] %s", msg.text.c_str());
    LogF("[F-5.6] compile success=%d", controller.LastCompileResult().success ? 1 : 0);
    LogF("[F-5.6] runner=%s", controller.RunnerName());
    LogF("[F-5.6] run success=%d", result.success ? 1 : 0);
    LogF("[F-5.6.5] stdout:");
    if ( result.stdoutText.empty() )
        LogF("[F-5.6.5] (no output)");
    else
        LogF("[F-5.6.5] %s", result.stdoutText.c_str());
    LogF("[F-5.6] exit=%d", result.exitCode);
}

wxFrame* GetMainFrame()
{
    wxFrame* frame = wxDynamicCast(static_cast<wxWindow*>(Fw2_GetTopFrame()), wxFrame);
    if ( !frame && clMainFrame::Get() )
        frame = clMainFrame::Get();
    return frame;
}

wxMenuBar* LookupMainMenuBarFromWx()
{
    wxFrame* frame = GetMainFrame();
    if ( !frame )
        return nullptr;

    if ( wxMenuBar* bar = frame->GetMenuBar() )
        return bar;

    if ( HarmonyCodeLite_F565_GetMainMenuBar )
    {
        if ( wxMenuBar* bar = static_cast<wxMenuBar*>(HarmonyCodeLite_F565_GetMainMenuBar()) )
            return bar;
    }

    return nullptr;
}

wxMenuBar* GetMainMenuBar()
{
    if ( wxMenuBar* cached = static_cast<wxMenuBar*>(
             g_mainMenuBarCache.load(std::memory_order_acquire)) )
        return cached;

    if ( wxMenuBar* bar = LookupMainMenuBarFromWx() )
    {
        CacheMainMenuBar(bar);
        return bar;
    }

    return nullptr;
}

int FindHarmonyMenuIndex(wxMenuBar* bar)
{
    if ( !bar )
        return -1;
    for ( size_t i = 0; i < bar->GetMenuCount(); ++i )
    {
        if ( bar->GetMenuLabelText(i) == wxString("Harmony") )
            return static_cast<int>(i);
    }
    return -1;
}

} // namespace

void Fw2_FlushMenuBarChrome()
{
    wxFrame* frame = GetMainFrame();
    wxMenuBar* bar = GetMainMenuBar();
    if ( !frame || !bar )
    {
        LogF("[R-4] host FlushMenuBarChrome skip frame=%p bar=%p",
             static_cast<void*>(frame), static_cast<void*>(bar));
        return;
    }

    // Direct wxMenuBar::Attach — libcodelite SetMenuBar vtable may no-op without
    // [R-4] MenuBar Create/Attach logs (see FUI_FRAME / menu.cpp).
    bar->Attach(frame);
    bar->Show(true);
    bar->Refresh(true);
    bar->Update();

    if ( wxTopLevelWindowOHOS* tlw = wxDynamicCast(frame, wxTopLevelWindowOHOS) )
    {
        tlw->PaintMenuBarOverChildren();
        // [R-6] Present the backing store so the painted MenuBar pixels
        // (including text drawn by DrawText) are flushed to the screen.
        // Without this, the paint happens but nobody presents the buffer.
        tlw->PresentBackingStore();
        LogF("[R-6] host FlushMenuBarChrome ok+present titles=%zu", bar->GetMenuCount());
    }
    else
    {
        LogF("[R-6] host FlushMenuBarChrome WARN: frame not wxTopLevelWindowOHOS");
    }
}

void Fw2_EnsureMainMenuBarAttached()
{
    wxFrame* frame = GetMainFrame();
    if ( !frame )
    {
        LogF("[R-4] host EnsureMainMenuBarAttached skip: no frame");
        return;
    }

    if ( frame->GetMenuBar() )
    {
        CacheMainMenuBar(frame->GetMenuBar());
        LogF("[R-4] host EnsureMainMenuBarAttached ok: already attached");
        return;
    }

    // CreateGUIControls may still be inside its first LoadMenuBar when EmbeddedStart
    // returns; pump init events so clMainFrame can SetMenuBar on the main thread.
    if ( wxTheApp )
    {
        for ( int i = 0; i < 100 && !frame->GetMenuBar(); ++i )
            wxTheApp->ProcessPendingEvents();
    }

    if ( frame->GetMenuBar() )
    {
        CacheMainMenuBar(frame->GetMenuBar());
        LogF("[R-4] host EnsureMainMenuBarAttached ok: attached after ProcessPendingEvents");
        return;
    }

    wxMenuBar* bar = nullptr;
    if ( HarmonyCodeLite_F565_GetMainMenuBar )
        bar = static_cast<wxMenuBar*>(HarmonyCodeLite_F565_GetMainMenuBar());

    static constexpr size_t kMinMainMenuTitles = 8;

    if ( bar && bar->GetMenuCount() >= kMinMainMenuTitles )
    {
        LogF("[R-4] host EnsureMainMenuBarAttached F565 bar=%p titles=%zu",
             static_cast<void*>(bar), bar->GetMenuCount());
        frame->SetMenuBar(bar);
        CacheMainMenuBar(bar);
        LogF("[R-4] host EnsureMainMenuBarAttached SetMenuBar ok titles=%zu",
             bar->GetMenuCount());
        return;
    }

    if ( !bar && wxXmlResource::Get() )
    {
        LogF("[R-4] host EnsureMainMenuBarAttached LoadMenuBar main_menu (main thread pre-Show)");
        bar = wxXmlResource::Get()->LoadMenuBar(wxString("main_menu"));
        LogF("[R-4] host LoadMenuBar returned bar=%p titles=%zu",
             static_cast<void*>(bar), bar ? bar->GetMenuCount() : 0);
    }

    if ( !bar || bar->GetMenuCount() < kMinMainMenuTitles )
    {
        LogF("[R-4] host EnsureMainMenuBarAttached FAIL: bar=%p titles=%zu",
             static_cast<void*>(bar), bar ? bar->GetMenuCount() : 0);
        if ( bar && bar->GetMenuCount() > 0 )
            CacheMainMenuBar(bar);
        return;
    }

    frame->SetMenuBar(bar);
    CacheMainMenuBar(bar);
    LogF("[R-4] host EnsureMainMenuBarAttached SetMenuBar ok titles=%zu",
         bar->GetMenuCount());
}

wxMenuBar* Fw2_GetMainMenuBarPtr()
{
    return GetMainMenuBar();
}

wxFrame* Fw2_GetMainFramePtr()
{
    return GetMainFrame();
}

void Fw2_ReattachMainMenuBarOnMainLoop()
{
    wxFrame* frame = GetMainFrame();
    if ( !frame )
    {
        LogF("[R-4] host ReattachMainMenuBar skip: no frame");
        return;
    }

    wxMenuBar* bar = static_cast<wxMenuBar*>(g_mainMenuBarCache.load(std::memory_order_acquire));
    if ( !bar )
        bar = LookupMainMenuBarFromWx();

    if ( !bar )
    {
        LogF("[R-4] host ReattachMainMenuBar skip: no bar");
        return;
    }

    CacheMainMenuBar(bar);

    if ( !frame->GetMenuBar() )
    {
        frame->SetMenuBar(bar);
        LogF("[R-4] host ReattachMainMenuBar SetMenuBar ok titles=%zu", bar->GetMenuCount());
    }
    else
    {
        LogF("[R-4] host ReattachMainMenuBar ok: frame already has menu bar");
    }

    // R-4: clMainFrame's SetMenuBar does not dispatch to the wxOHOS MenuBar::Attach
    // (vtable drift against older wx headers), so the real child window is never
    // created and the menubar stays invisible. Call Attach explicitly to force
    // Create() + Show().
    //
    // R-5: do NOT call Refresh/Update on the MenuBar — the wxPaintDC path busy-spins
    // on the patched DC bind when MenuBar sits at y=-28 (above client area).
    // Instead, paint the menu bar straight into the TLW backing pixel buffer,
    // bypassing the entire BeginPaint/Bind chain.
    bar->Attach(frame);
    bar->Show(true);
    LogF("[R-5] host ReattachMainMenuBar Attach+Show done titles=%zu", bar->GetMenuCount());

    if ( wxTopLevelWindowOHOS* tlw = wxDynamicCast(frame, wxTopLevelWindowOHOS) )
    {
        // [R-6] Let wxWidgets paint the MenuBar (background + text via DrawText)
        // into the TLW backing store, then present it to the screen.
        // Previously we tried DirectPaint which overwrote the wxWidgets text.
        // Now we just ensure the backing store is valid and present after the
        // wxWidgets paint cycle has completed (triggered by Attach+Show above).
        if ( tlw->EnsureBackingStore() )
        {
            // Attach+Show above triggers wxWidgets to paint the MenuBar
            // synchronously (confirmed by timing logs: paint <1ms after Show).
            // Present the backing store to flush the painted pixels to screen.
            tlw->PresentBackingStore();
            LogF("[R-6] PresentBackingStore after Attach+Show done titles=%zu",
                 bar->GetMenuCount());
        }
        else
        {
            LogF("[R-6] WARN: EnsureBackingStore failed");
        }
    }
    else
    {
        LogF("[R-6] WARN: frame not wxTopLevelWindowOHOS, menu bar NOT presented");
    }
}

void Fw2_InstallHarmonyMenuIfNeeded()
{
    wxFrame* frame = GetMainFrame();
    wxMenuBar* bar = GetMainMenuBar();
    if ( !bar || !frame )
    {
        LogF("[F-5.6.5] WARN menu bar missing frame=%p bar=%p",
             static_cast<void*>(frame), static_cast<void*>(bar));
        return;
    }
    if ( !frame->GetMenuBar() )
        frame->SetMenuBar(bar);
    if ( FindHarmonyMenuIndex(bar) >= 0 )
    {
        LogF("[F-5.6.5] Harmony menu already present (frame.cpp)");
        return;
    }

    wxMenu* harmonyMenu = new wxMenu;
    harmonyMenu->Append(kHarmonyBuildRemote, wxString("Build Remote"));
    harmonyMenu->Append(kHarmonyRunRemote, wxString("Run Remote"));
    harmonyMenu->Bind(wxEVT_MENU_OPEN, [](wxMenuEvent& event) {
        wxUnusedVar(event);
        LogF("[F-5.6.5] Harmony menu opened");
    });
    harmonyMenu->Bind(wxEVT_MENU, [](wxCommandEvent& event) {
        if ( event.GetId() == kHarmonyBuildRemote )
            Fw2_BuildRemote(nullptr);
    }, kHarmonyBuildRemote);
    harmonyMenu->Bind(wxEVT_MENU, [](wxCommandEvent& event) {
        if ( event.GetId() == kHarmonyRunRemote )
            Fw2_RunRemote();
    }, kHarmonyRunRemote);
    bar->Append(harmonyMenu, wxString("Harmony"));
    bar->Refresh();
    frame->Refresh();
    LogF("[F-5.6.5] Harmony menu installed (host bridge)");
    LogF("[F-5.6.5] Build Remote id=%d", kHarmonyBuildRemote);
    LogF("[F-5.6.5] Run Remote id=%d", kHarmonyRunRemote);
}

void Fw2_BuildRemote(const char* sourceFile)
{
    LogF("[F-5.6.5] command=BUILD_REMOTE");

    const std::string installDir = Fw2_GetInstallDir();
    if ( installDir.empty() )
    {
        LogF("[F-5.6.5] FAIL install dir unset");
        return;
    }

    std::string file = sourceFile ? sourceFile : std::string();
    ProjectBuildConfig config = LoadBuildConfig(installDir, file);
    if ( file.empty() )
        file = DefaultSourceFile(installDir, config);

    BuildController controller(installDir, config);
    const BuildResult result = controller.BuildAndRun(file);
    LogBuildRunResult(controller, result);

    if ( controller.LastCompileResult().success && result.success )
        LogF("[F-5.6.5] OK");
    else
        LogF("[F-5.6.5] FAIL");
}

void Fw2_RunRemote()
{
    LogF("[F-5.6.5] command=RUN_REMOTE");

    const std::string installDir = Fw2_GetInstallDir();
    if ( installDir.empty() )
    {
        LogF("[F-5.6.5] FAIL install dir unset");
        return;
    }

    ProjectBuildConfig config = LoadBuildConfig(installDir, {});
    RemoteRunnerBackend runner(JoinPath(installDir, config.runWorkspace));
    const BuildResult result = runner.Run(DefaultBinary(installDir, config));

    LogF("[F-5.6] runner=%s", runner.Name());
    LogF("[F-5.6] run success=%d", result.success ? 1 : 0);
    LogF("[F-5.6.5] stdout:");
    if ( result.stdoutText.empty() )
        LogF("[F-5.6.5] (no output)");
    else
        LogF("[F-5.6.5] %s", result.stdoutText.c_str());
    LogF("[F-5.6] exit=%d", result.exitCode);

    if ( result.success )
        LogF("[F-5.6.5] OK");
    else
        LogF("[F-5.6.5] FAIL");
}

void Fw2_RegisterBuildBridge()
{
    if ( HarmonyCodeLite_F565_RegisterBuildBridge )
        HarmonyCodeLite_F565_RegisterBuildBridge(&Fw2_BuildRemote, &Fw2_RunRemote);
    Fw2_InstallHarmonyMenuIfNeeded();
    Fw2_InstallHarmonyProjectMenuIfNeeded();
    LogF("[F-5.6.5] RegisterBuildBridge OK");
}

void Fw2_ProbeHarmonyBuildMenu()
{
    LogF("[F-5.6.5] probe enter");
    Fw2_InstallHarmonyMenuIfNeeded();

    int menuOpen = -1;
    if ( HarmonyCodeLite_F565_ProbeHarmonyMenuOpen )
        menuOpen = HarmonyCodeLite_F565_ProbeHarmonyMenuOpen();
    else if ( wxMenuBar* bar = GetMainMenuBar() )
    {
        const int idx = FindHarmonyMenuIndex(bar);
        if ( idx >= 0 && bar->GetMenu(static_cast<size_t>(idx)) )
        {
            wxMenuEvent evt(wxEVT_MENU_OPEN, idx, bar->GetMenu(static_cast<size_t>(idx)));
            if ( clMainFrame* frame = clMainFrame::Get() )
                frame->GetEventHandler()->ProcessEvent(evt);
            menuOpen = 1;
        }
    }
    LogF("[F-5.6.5] ProbeHarmonyMenuOpen=%d", menuOpen);

    int build = -1;
    if ( HarmonyCodeLite_F565_TriggerBuildRemote )
        build = HarmonyCodeLite_F565_TriggerBuildRemote();
    else
    {
        Fw2_BuildRemote(nullptr);
        build = 1;
    }
    LogF("[F-5.6.5] TriggerBuildRemote=%d", build);
}
