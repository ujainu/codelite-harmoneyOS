/////////////////////////////////////////////////////////////////////////////
// Minimal wxDialog for OHOS
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/dialog.h"
#endif

wxIMPLEMENT_DYNAMIC_CLASS(wxDialog, wxTopLevelWindow);

wxDialog::wxDialog(wxWindow *parent, wxWindowID id, const wxString& title,
                   const wxPoint& pos, const wxSize& size, long style,
                   const wxString& name)
{
    Create(parent, id, title, pos, size, style, name);
}

bool wxDialog::Create(wxWindow *parent, wxWindowID id, const wxString& title,
                      const wxPoint& pos, const wxSize& size, long style,
                      const wxString& name)
{
    return wxTopLevelWindow::Create(parent, id, title, pos, size, style, name);
}

int wxDialog::ShowModal()
{
    m_isModalShowing = true;
    Show(true);
    // no real event loop modal nesting yet
    m_isModalShowing = false;
    return GetReturnCode();
}

void wxDialog::EndModal(int retCode)
{
    SetReturnCode(retCode);
    m_isModalShowing = false;
    Show(false);
}

bool wxDialog::IsModal() const
{
    return m_isModalShowing;
}
