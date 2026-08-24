/////////////////////////////////////////////////////////////////////////////
// F-8.1 / F-8.2: New Harmony Project — host-only (libentry.so).
/////////////////////////////////////////////////////////////////////////////

#include "fw2_project_bridge.h"

#include "compiler_run_util.h"
#include "fw2_build_bridge.h"
#include "fw2_project_dialog.h"
#include "fw2_wx_host.h"

#include <hilog/log.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/frame.h"
    #include "wx/menu.h"
    #include "wx/toplevel.h"
    #include "wx/xrc/xmlres.h"
#endif

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xF002
#define LOG_TAG "FW2Host"

class CodeLiteApp : public wxApp
{
public:
    void OpenFile(const wxString& path, long lineNumber = 0);
};

class clMainFrame : public wxFrame
{
public:
    static clMainFrame* Get();
};

namespace {

static const int kNewHarmonyProjectDialog = 59010;

static const char* kTemplateRel = "templates/f8/HelloWorld";
static const char* kTemplateFiles[] = {"main.cpp", "project.json", "build.json"};

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

bool EnsureDir(const std::string& dir)
{
    if ( dir.empty() )
        return false;
    if ( CompilerEnsureDir(dir) )
        return true;
    struct stat st {};
    return stat(dir.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

std::string DefaultProjectsLocation()
{
    const char* home = std::getenv("HOME");
    if ( !home || !home[0] )
        return {};
    return JoinPath(home, "Projects");
}

std::string UserProjectsRoot()
{
    const char* home = std::getenv("HOME");
    if ( !home || !home[0] )
        return {};
    return JoinPath(home, ".codelite/projects");
}

std::string DefaultMainCpp(const std::string& projectType)
{
    return std::string("// F-8.2: ") + projectType + R"(
#include <iostream>

int main()
{
    std::cout << "Hello HarmonyCodeLite" << std::endl;
    return 0;
}
)";
}

std::string DefaultProjectJson(const std::string& name, const std::string& type)
{
    return std::string("{\n  \"name\": \"") + name + "\",\n  \"type\": \"" + type
        + "\",\n  \"version\": \"1.0.0\",\n  \"language\": \"cpp\",\n  \"entry\": \"main.cpp\"\n}\n";
}

std::string DefaultBuildJson()
{
    return R"({
  "profile": "remote-ohos-aarch64",
  "compiler": "RemoteCompiler",
  "runner": "RemoteRunner",
  "mode": "release",
  "target": "arm64",
  "source": "main.cpp",
  "output": "main",
  "flags": [
    "-std=c++17"
  ],
  "remoteWorkspace": "build/remote",
  "runWorkspace": "build/run"
}
)";
}

std::string TemplateProjectType(HarmonyProjectTemplate templ)
{
    switch ( templ )
    {
    case HarmonyProjectTemplate::HarmonyConsole:
        return "harmony-console";
    default:
        return "harmony-cpp-console";
    }
}

std::string TemplateLabel(HarmonyProjectTemplate templ)
{
    switch ( templ )
    {
    case HarmonyProjectTemplate::HarmonyConsole:
        return "Harmony Console App";
    default:
        return "Harmony C++ Console App";
    }
}

bool CopyTemplateFile(const std::string& installDir, const char* name, const std::string& dest,
    const HarmonyProjectCreateParams& params)
{
    const std::string src = JoinPath(JoinPath(installDir, kTemplateRel), name);
    struct stat st {};
    if ( stat(src.c_str(), &st) == 0 && S_ISREG(st.st_mode) )
        return CompilerCopyFile(src, dest);

    const std::string projectType = TemplateProjectType(params.templ);
    std::string text;
    if ( std::string(name) == "main.cpp" )
        text = DefaultMainCpp(TemplateLabel(params.templ));
    else if ( std::string(name) == "project.json" )
        text = DefaultProjectJson(params.name, projectType);
    else if ( std::string(name) == "build.json" )
        text = DefaultBuildJson();
    else
        return false;

    return CompilerWriteTextFile(dest, text);
}

bool PatchProjectJsonName(const std::string& path, const std::string& name)
{
    std::string json;
    if ( !CompilerReadTextFile(path, json) )
        return false;
    const std::string marker = "\"name\": \"HelloWorld\"";
    const std::string replacement = "\"name\": \"" + name + "\"";
    const std::size_t pos = json.find(marker);
    if ( pos == std::string::npos )
        return true;
    json.replace(pos, marker.size(), replacement);
    return CompilerWriteTextFile(path, json);
}

bool OpenEditorFile(const std::string& path)
{
    // MainBook notebook panes are not wired while AddPane is binary-patched out.
    LogF("[F-8.2] editor open deferred (MainBook layout) path=%s", path.c_str());
    (void)path;
    return false;
}

int FindFileMenuIndex(wxMenuBar* bar)
{
    if ( !bar )
        return -1;
    for ( size_t i = 0; i < bar->GetMenuCount(); ++i )
    {
        wxString label = bar->GetMenuLabelText(i);
        label.Replace("&", "", true);
        if ( label == wxString("File") )
            return static_cast<int>(i);
    }
    return -1;
}

bool FileMenuHasNewHarmonyProject(wxMenu* fileMenu)
{
    if ( !fileMenu )
        return false;
    for ( size_t i = 0; i < fileMenu->GetMenuItemCount(); ++i )
    {
        wxMenuItem* item = fileMenu->FindItemByPosition(i);
        if ( !item )
            continue;
        wxString label = item->GetItemLabelText();
        label.Replace("&", "", true);
        if ( label == wxString("New Harmony Project...") )
            return true;
        if ( item->IsSubMenu() && label == wxString("New Harmony Project") )
            return true;
    }
    return false;
}

} // namespace

int Fw2_CreateHarmonyProject(const HarmonyProjectCreateParams& params)
{
    LogF("[F-8.2] command=CREATE_HARMONY_PROJECT name=%s location=%s template=%d",
         params.name.c_str(), params.location.c_str(), static_cast<int>(params.templ));

    if ( params.name.empty() || params.location.empty() )
    {
        LogF("[F-8.2] FAIL missing name or location");
        return -1;
    }

    const std::string installDir = Fw2_GetInstallDir();
    if ( installDir.empty() )
    {
        LogF("[F-8.2] FAIL install dir unset");
        return -1;
    }

    const std::string projectDir = JoinPath(params.location, params.name);
    struct stat st {};
    if ( stat(projectDir.c_str(), &st) == 0 )
    {
        LogF("[F-8.2] FAIL project already exists dir=%s", projectDir.c_str());
        return -1;
    }

    if ( !EnsureDir(params.location) || !EnsureDir(projectDir) )
    {
        LogF("[F-8.2] FAIL mkdir project dir=%s", projectDir.c_str());
        return -1;
    }

    int written = 0;
    for ( const char* name : kTemplateFiles )
    {
        const std::string dest = JoinPath(projectDir, name);
        if ( !CopyTemplateFile(installDir, name, dest, params) )
        {
            LogF("[F-8.2] FAIL write %s", dest.c_str());
            return -1;
        }
        if ( std::string(name) == "project.json" )
            (void)PatchProjectJsonName(dest, params.name);
        ++written;
        LogF("[F-8.2] created %s", dest.c_str());
    }

    const std::string mainCpp = JoinPath(projectDir, "main.cpp");
    const std::string buildJson = JoinPath(projectDir, "build.json");
    const std::string remoteDir = JoinPath(installDir, "build/remote");
    const std::string stagedMain = JoinPath(remoteDir, "main.cpp");
    const std::string stagedBuildJson = JoinPath(remoteDir, "build.json");
    if ( !EnsureDir(remoteDir) || !CompilerCopyFile(mainCpp, stagedMain) )
    {
        LogF("[F-8.2] FAIL stage main.cpp → %s", stagedMain.c_str());
        return -1;
    }
    if ( !CompilerCopyFile(buildJson, stagedBuildJson) )
    {
        LogF("[F-8.2] FAIL stage build.json → %s", stagedBuildJson.c_str());
        return -1;
    }
    LogF("[F-8.2] staged remote source=%s", stagedMain.c_str());
    LogF("[F-8.2] staged build.json=%s", stagedBuildJson.c_str());
    LogF("[F-8.2] project=%s files=%d type=%s", projectDir.c_str(), written,
         TemplateProjectType(params.templ).c_str());

    if ( params.openEditor )
        (void)OpenEditorFile(mainCpp);

    if ( params.buildAfterCreate )
        Fw2_BuildRemote(mainCpp.c_str());

    LogF("[F-8.2] OK");
    return 1;
}

int Fw2_NewHarmonyConsoleProject()
{
    LogF("[F-8.1] command=NEW_HARMONY_CONSOLE (probe defaults)");

    HarmonyProjectCreateParams params;
    params.name = "HelloWorld";
    params.location = UserProjectsRoot().empty() ? DefaultProjectsLocation() : UserProjectsRoot();
    params.templ = HarmonyProjectTemplate::CppConsole;
    params.openEditor = false;
    params.buildAfterCreate = true;
    return Fw2_CreateHarmonyProject(params);
}

void Fw2_InstallHarmonyProjectMenuIfNeeded()
{
    wxMenuBar* bar = Fw2_GetMainMenuBarPtr();
    wxFrame* frame = Fw2_GetMainFramePtr();
    if ( !bar || !frame )
    {
        LogF("[F-8.2] WARN File menu install skipped frame=%p bar=%p",
             static_cast<void*>(frame), static_cast<void*>(bar));
        return;
    }
    if ( !frame->GetMenuBar() )
        frame->SetMenuBar(bar);

    const int fileIdx = FindFileMenuIndex(bar);
    if ( fileIdx < 0 )
    {
        LogF("[F-8.2] WARN File menu not found");
        return;
    }

    wxMenu* fileMenu = bar->GetMenu(static_cast<size_t>(fileIdx));
    if ( !fileMenu )
    {
        LogF("[F-8.2] WARN File menu null");
        return;
    }
    if ( FileMenuHasNewHarmonyProject(fileMenu) )
    {
        LogF("[F-8.2] New Harmony Project menu already present");
        return;
    }

    fileMenu->AppendSeparator();
    fileMenu->Append(kNewHarmonyProjectDialog, wxString("New Harmony Project..."));
    fileMenu->Bind(wxEVT_MENU, [](wxCommandEvent& event) {
        if ( event.GetId() == kNewHarmonyProjectDialog )
            Fw2_ShowNewHarmonyProjectDialog();
    }, kNewHarmonyProjectDialog);
    bar->Refresh();
    frame->Refresh();
    LogF("[F-8.2] File → New Harmony Project... id=%d", kNewHarmonyProjectDialog);
}

void Fw2_ProbeHarmonyNewProject()
{
    LogF("[F-8.2] probe enter (CallAfter menu only)");
    Fw2_InstallHarmonyProjectMenuIfNeeded();
}
