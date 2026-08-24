#include "wx/wxprec.h"

#if wxUSE_CLIPBOARD

#include "wx/clipbrd.h"

#ifndef WX_PRECOMP
    #include "wx/dataobj.h"
#endif

wxClipboard* wxClipboard::Get()
{
    static wxClipboard s_clipboard;
    return &s_clipboard;
}

bool wxClipboard::SetData(wxDataObject* data)
{
    wxUnusedVar(data);
    return false;
}

bool wxClipboard::GetData(wxDataObject& data)
{
    wxUnusedVar(data);
    return false;
}

bool wxClipboard::IsSupported(const wxDataFormat& format) const
{
    wxUnusedVar(format);
    return false;
}

#endif // wxUSE_CLIPBOARD
