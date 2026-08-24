#include "wx/wxprec.h"

#if wxUSE_CHECKBOX

#ifndef WX_PRECOMP
    #include "wx/checkbox.h"
#endif

wxIMPLEMENT_DYNAMIC_CLASS(wxCheckBox, wxControl);

bool wxCheckBox::Create(wxWindow* parent, wxWindowID id, const wxString& label,
                        const wxPoint& pos, const wxSize& size, long style,
                        const wxValidator& validator, const wxString& name)
{
    if ( !CreateControl(parent, id, pos, size, style, validator, name) )
        return false;

    SetLabel(label);
    return true;
}

#endif // wxUSE_CHECKBOX
