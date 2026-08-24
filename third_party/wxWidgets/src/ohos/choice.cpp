#include "wx/wxprec.h"

#if wxUSE_CHOICE

#ifndef WX_PRECOMP
    #include "wx/choice.h"
#endif

wxIMPLEMENT_DYNAMIC_CLASS(wxChoice, wxControl);

bool wxChoice::Create(wxWindow* parent, wxWindowID id, const wxPoint& pos,
                      const wxSize& size, int n, const wxString choices[],
                      long style, const wxString& name)
{
    wxUnusedVar(n);
    wxUnusedVar(choices);
    if ( !CreateControl(parent, id, pos, size, style, wxDefaultValidator, name) )
        return false;

    return true;
}

#endif // wxUSE_CHOICE
