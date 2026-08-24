/////////////////////////////////////////////////////////////////////////////
// Minimal wxBitmapButton for OHOS
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_BMPBUTTON

#ifndef WX_PRECOMP
    #include "wx/bmpbuttn.h"
#endif

wxIMPLEMENT_DYNAMIC_CLASS(wxBitmapButton, wxButton);

bool wxBitmapButton::Create(wxWindow *parent,
                            wxWindowID id,
                            const wxBitmapBundle& bitmap,
                            const wxPoint& pos,
                            const wxSize& size,
                            long style,
                            const wxValidator& validator,
                            const wxString& name)
{
    if ( !wxBitmapButtonBase::Create(parent, id, pos, size, style, validator, name) )
        return false;

    if ( bitmap.IsOk() )
        SetBitmapLabel(bitmap);

    return true;
}

#endif // wxUSE_BMPBUTTON
