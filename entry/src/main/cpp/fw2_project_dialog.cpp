/////////////////////////////////////////////////////////////////////////////
// F-8.2: New Harmony Project dialog — host-only (libentry.so).
/////////////////////////////////////////////////////////////////////////////

#include "fw2_project_dialog.h"

#include "fw2_wx_host.h"

#include <hilog/log.h>

#include <cctype>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <string>

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/button.h"
    #include "wx/dialog.h"
    #include "wx/msgdlg.h"
    #include "wx/radiobox.h"
    #include "wx/sizer.h"
    #include "wx/stattext.h"
    #include "wx/textctrl.h"
#endif

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xF002
#define LOG_TAG "FW2Host"

static const int kDialogCreateId = 59020;

static void LogF(const char* fmt, ...)
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

static std::string JoinPath(const std::string& dir, const std::string& name)
{
    if ( dir.empty() )
        return name;
    if ( dir.back() == '/' )
        return dir + name;
    return dir + "/" + name;
}

static std::string DefaultProjectsLocation()
{
    const char* home = std::getenv("HOME");
    if ( !home || !home[0] )
        return {};
    return JoinPath(home, "Projects");
}

static bool IsValidProjectName(const wxString& name)
{
    if ( name.empty() )
        return false;
    for ( wxUniChar ch : name )
    {
        if ( wxIsalnum(ch) || ch == '_' || ch == '-' )
            continue;
        return false;
    }
    return true;
}

class HarmonyNewProjectDialog : public wxDialog
{
public:
    HarmonyNewProjectDialog(wxWindow* parent)
        : wxDialog(parent, wxID_ANY, wxString("New Harmony Project"), wxDefaultPosition, wxSize(480, 320),
            wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    {
        auto* root = new wxBoxSizer(wxVERTICAL);

        auto* nameRow = new wxBoxSizer(wxHORIZONTAL);
        nameRow->Add(new wxStaticText(this, wxID_ANY, wxString("Project Name:")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT,
            8);
        nameCtrl_ = new wxTextCtrl(this, wxID_ANY, wxString("MyApp"));
        nameRow->Add(nameCtrl_, 1, wxEXPAND);
        root->Add(nameRow, 0, wxEXPAND | wxALL, 12);

        auto* locRow = new wxBoxSizer(wxHORIZONTAL);
        locRow->Add(new wxStaticText(this, wxID_ANY, wxString("Location:")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
        const wxString defaultLoc = wxString::FromUTF8(DefaultProjectsLocation().c_str());
        locationCtrl_ = new wxTextCtrl(this, wxID_ANY, defaultLoc);
        locRow->Add(locationCtrl_, 1, wxEXPAND);
        root->Add(locRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

        const wxString choices[] = {wxString("C++ Console"), wxString("Harmony Console")};
        templateBox_ = new wxRadioBox(this, wxID_ANY, wxString("Template"), wxDefaultPosition, wxDefaultSize,
            WXSIZEOF(choices), choices, 1, wxRA_SPECIFY_ROWS);
        root->Add(templateBox_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

        auto* buttons = new wxBoxSizer(wxHORIZONTAL);
        buttons->AddStretchSpacer();
        auto* createBtn = new wxButton(this, kDialogCreateId, wxString("Create"));
        buttons->Add(createBtn, 0, wxRIGHT, 8);
        buttons->Add(new wxButton(this, wxID_CANCEL, wxString("Cancel")), 0);
        root->Add(buttons, 0, wxEXPAND | wxALL, 12);

        SetSizerAndFit(root);
        CentreOnParent();
        createBtn->SetDefault();

        Bind(wxEVT_BUTTON, &HarmonyNewProjectDialog::OnCreate, this, kDialogCreateId);
    }

    bool CollectParams(HarmonyProjectCreateParams& out, wxString& error)
    {
        const wxString name = nameCtrl_->GetValue().Trim(true).Trim(false);
        const wxString location = locationCtrl_->GetValue().Trim(true).Trim(false);
        if ( !IsValidProjectName(name) )
        {
            error = wxString("Project name must be alphanumeric (underscore/hyphen allowed).");
            return false;
        }
        if ( location.empty() )
        {
            error = wxString("Location is required.");
            return false;
        }
        out.name = name.ToUTF8().data();
        out.location = location.ToUTF8().data();
        out.templ = templateBox_->GetSelection() == 1 ? HarmonyProjectTemplate::HarmonyConsole
                                                      : HarmonyProjectTemplate::CppConsole;
        out.openEditor = true;
        out.buildAfterCreate = true;
        return true;
    }

    bool hasCollected_ = false;
    HarmonyProjectCreateParams collected_;

private:
    void OnCreate(wxCommandEvent& event)
    {
        wxUnusedVar(event);
        HarmonyProjectCreateParams params;
        wxString error;
        if ( !CollectParams(params, error) )
        {
            wxMessageBox(error, wxString("New Harmony Project"), wxOK | wxICON_WARNING, this);
            return;
        }
        collected_ = params;
        hasCollected_ = true;
        EndModal(wxID_OK);
    }

    wxTextCtrl* nameCtrl_ = nullptr;
    wxTextCtrl* locationCtrl_ = nullptr;
    wxRadioBox* templateBox_ = nullptr;
};

int Fw2_ShowNewHarmonyProjectDialog()
{
    LogF("[F-8.2] command=NEW_HARMONY_PROJECT_DIALOG");

    wxWindow* parent = wxDynamicCast(static_cast<wxWindow*>(Fw2_GetTopFrame()), wxWindow);
    HarmonyNewProjectDialog dlg(parent);
    const int rc = dlg.ShowModal();
    if ( rc != wxID_OK || !dlg.hasCollected_ )
    {
        LogF("[F-8.2] dialog cancelled");
        return 0;
    }

    return Fw2_CreateHarmonyProject(dlg.collected_);
}
