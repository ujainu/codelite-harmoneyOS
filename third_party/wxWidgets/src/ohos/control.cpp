/////////////////////////////////////////////////////////////////////////////
// Minimal wxControl for OHOS
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/control.h"
#endif

wxIMPLEMENT_DYNAMIC_CLASS(wxControl, wxWindow);

bool wxControl::Create(wxWindow *parent,
                       wxWindowID id,
                       const wxPoint& pos,
                       const wxSize& size,
                       long style,
                       const wxValidator& validator,
                       const wxString& name)
{
    if ( !CreateBase(parent, id, pos, size, style, validator, name) )
        return false;

    if ( parent )
        parent->AddChild(this);

    return true;
}

wxSize wxControl::DoGetBestSize() const
{
    return wxSize(80, 20);
}

bool wxControl::ProcessCommand(wxCommandEvent& event)
{
    return HandleWindowEvent(event);
}
