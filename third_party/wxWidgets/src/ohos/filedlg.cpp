/////////////////////////////////////////////////////////////////////////////
// wx/ohos/filedlg.cpp — DocumentPicker-backed wxFileDialog
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_FILEDLG

#include "wx/filedlg.h"
#include "wx/filename.h"
#include "wx/ohos/filepicker.h"

#ifdef __WXOHOS__
#include <hilog/log.h>
#ifndef LOG_DOMAIN
#define LOG_DOMAIN 0xF004
#endif
#ifndef LOG_TAG
#define LOG_TAG "wxOHOS"
#endif
#endif

wxIMPLEMENT_DYNAMIC_CLASS(wxFileDialog, wxDialog);

wxFileDialog::wxFileDialog(wxWindow* parent,
                           const wxString& message,
                           const wxString& defaultDir,
                           const wxString& defaultFile,
                           const wxString& wildCard,
                           long style,
                           const wxPoint& pos,
                           const wxSize& sz,
                           const wxString& name)
{
    Create(parent, message, defaultDir, defaultFile, wildCard, style, pos, sz, name);
}

bool wxFileDialog::Create(wxWindow* parent,
                          const wxString& message,
                          const wxString& defaultDir,
                          const wxString& defaultFile,
                          const wxString& wildCard,
                          long style,
                          const wxPoint& pos,
                          const wxSize& sz,
                          const wxString& name)
{
    return wxFileDialogBase::Create(parent, message, defaultDir, defaultFile,
                                    wildCard, style, pos, sz, name);
}

int wxFileDialog::ShowModal()
{
    char uri[2048] = {};
    char path[4096] = {};

    const wxString defaultDir = GetDirectory();
    const int pick = wxOhos_PickOpenFile(defaultDir.utf8_str().data(),
                                         HasFlag(wxFD_MULTIPLE) ? 1 : 0,
                                         path, static_cast<int>(sizeof(path)),
                                         uri, static_cast<int>(sizeof(uri)));

#ifdef __WXOHOS__
    if ( pick == 1 )
    {
        OH_LOG_INFO(LOG_APP,
                    "[F-1.4] return wxID_OK uri=%{public}s path=%{public}s",
                    uri, path);
    }
    else
    {
        OH_LOG_INFO(LOG_APP, "[F-1.4] picker cancelled or unavailable rc=%{public}d", pick);
    }
#endif

    if ( pick != 1 )
    {
        SetReturnCode(wxID_CANCEL);
        return wxID_CANCEL;
    }

    m_path = wxString::FromUTF8(path);
    m_fileName = wxFileName(m_path).GetFullName();
    m_pickedPaths.Clear();
    m_pickedPaths.Add(m_path);

    SetReturnCode(wxID_OK);
    return wxID_OK;
}

wxString wxFileDialog::GetPath() const
{
    wxCHECK_MSG(!HasFlag(wxFD_MULTIPLE), wxString(),
                "When using wxFD_MULTIPLE, must call GetPaths() instead");
    return m_path;
}

void wxFileDialog::GetPaths(wxArrayString& paths) const
{
    paths = m_pickedPaths;
}

#endif // wxUSE_FILEDLG
