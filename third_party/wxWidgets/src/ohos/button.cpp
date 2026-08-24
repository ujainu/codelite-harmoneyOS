/////////////////////////////////////////////////////////////////////////////
// Minimal wxButton for OHOS
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_BUTTON

#ifndef WX_PRECOMP
    #include "wx/button.h"
#endif

wxIMPLEMENT_DYNAMIC_CLASS(wxButton, wxControl);

bool wxButton::Create(wxWindow *parent,
                      wxWindowID id,
                      const wxString& label,
                      const wxPoint& pos,
                      const wxSize& size,
                      long style,
                      const wxValidator& validator,
                      const wxString& name)
{
    if ( !CreateControl(parent, id, pos, size, style, validator, name) )
        return false;

    SetLabel(label);
    return true;
}

wxWindow *wxButton::SetDefault()
{
    return wxButtonBase::SetDefault();
}

wxSize wxButton::DoGetBestSize() const
{
    return wxSize(80, 24);
}

#endif // wxUSE_BUTTON
