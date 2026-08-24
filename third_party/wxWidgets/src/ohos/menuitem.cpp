/////////////////////////////////////////////////////////////////////////////
// Minimal wxMenuItem for OHOS
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/menu.h"
    #include "wx/menuitem.h"
#endif

wxIMPLEMENT_DYNAMIC_CLASS(wxMenuItem, wxObject);

wxMenuItem::wxMenuItem(wxMenu *parentMenu, int id, const wxString& text,
                       const wxString& help, wxItemKind kind, wxMenu *subMenu)
    : wxMenuItemBase(parentMenu, id, text, help, kind, subMenu)
{
}

wxMenuItem::~wxMenuItem() = default;

/* static */ wxMenuItem *wxMenuItemBase::New(wxMenu *parentMenu, int itemid,
                                             const wxString& text,
                                             const wxString& help,
                                             wxItemKind kind, wxMenu *subMenu)
{
    return new wxMenuItem(parentMenu, itemid, text, help, kind, subMenu);
}

void wxMenuItem::SetItemLabel(const wxString& str)
{
    wxMenuItemBase::SetItemLabel(str);
}

void wxMenuItem::Enable(bool enable)
{
    wxMenuItemBase::Enable(enable);
}

void wxMenuItem::Check(bool check)
{
    wxMenuItemBase::Check(check);
}
