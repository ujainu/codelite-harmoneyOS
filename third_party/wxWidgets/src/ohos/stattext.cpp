/////////////////////////////////////////////////////////////////////////////
// Name:        src/ohos/stattext.cpp
// Purpose:     Minimal wxStaticText for OHOS — avoid generic markup/DC paths
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_STATTEXT

#include "wx/stattext.h"

#ifndef WX_PRECOMP
    #include "wx/control.h"
    #include "wx/gdicmn.h"
#endif

#if wxUSE_MARKUP
    #include "wx/private/markupparser.h"
#endif

#include <hilog/log.h>

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xF004
#define LOG_TAG "wxOHOS"

namespace
{

wxSize OhosEstimateTextSize(const wxString& text)
{
    static const int charW = 8;
    static const int charH = 16;

    if ( text.empty() )
        return wxSize(charW, charH);

    const int lines = text.Freq('\n') + 1;
    int maxLineLen = 0;
    int lineLen = 0;

    for ( wxString::const_iterator it = text.begin(); it != text.end(); ++it )
    {
        if ( *it == '\n' )
        {
            if ( lineLen > maxLineLen )
                maxLineLen = lineLen;
            lineLen = 0;
        }
        else
        {
            ++lineLen;
        }
    }

    if ( lineLen > maxLineLen )
        maxLineLen = lineLen;

    return wxSize(wxMax(1, maxLineLen) * charW,
                  wxMax(1, lines) * charH);
}

} // namespace

bool wxStaticText::Create(wxWindow *parent,
                          wxWindowID id,
                          const wxString& label,
                          const wxPoint& pos,
                          const wxSize& size,
                          long style,
                          const wxString& name)
{
    if ( !CreateControl(parent, id, pos, size, style, wxDefaultValidator, name) )
        return false;

    SetLabel(label);
    SetInitialSize(size);

    OH_LOG_INFO(LOG_APP, "[FULL_UI] wxStaticText OK");

    return true;
}

void wxStaticText::SetLabel(const wxString& label)
{
    if ( !UpdateLabelOrig(label) )
        return;

    WXSetVisibleLabel(GetEllipsizedLabel());
    AutoResizeIfNecessary();
}

#if wxUSE_MARKUP
bool wxStaticText::DoSetLabelMarkup(const wxString& markup)
{
    wxString plain(markup);
    wxMarkupParser::Strip(plain);
    SetLabel(plain);
    return true;
}
#endif // wxUSE_MARKUP

wxSize wxStaticText::DoGetBestClientSize() const
{
    return OhosEstimateTextSize(WXGetVisibleLabel());
}

wxString wxStaticText::WXGetVisibleLabel() const
{
    return m_visibleLabel;
}

void wxStaticText::WXSetVisibleLabel(const wxString& str)
{
    m_visibleLabel = str;
}

#endif // wxUSE_STATTEXT
