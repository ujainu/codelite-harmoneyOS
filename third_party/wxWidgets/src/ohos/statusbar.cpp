/////////////////////////////////////////////////////////////////////////////
// Name:        src/ohos/statusbar.cpp
// Purpose:     Minimal wxStatusBar for OHOS — no generic paint/DC paths
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_STATUSBAR

#include "wx/statusbr.h"

#ifndef WX_PRECOMP
    #include "wx/control.h"
#endif

#include <hilog/log.h>

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xF004
#define LOG_TAG "wxOHOS"

wxIMPLEMENT_DYNAMIC_CLASS(wxStatusBar, wxStatusBarBase);

bool wxStatusBar::Create(wxWindow *parent,
                         wxWindowID winid,
                         long style,
                         const wxString& name)
{
    if ( !CreateControl(parent, winid, wxDefaultPosition, wxDefaultSize,
                         style, wxDefaultValidator, name) )
        return false;

    SetFieldsCount(1);
    SetMinHeight(22);
    SetSize(wxDefaultCoord, wxDefaultCoord, wxDefaultCoord, m_minHeight);

    OH_LOG_INFO(LOG_APP, "[FULL_UI] wxStatusBar Create OK");

    return true;
}

void wxStatusBar::SetFieldsCount(int number, const int *widths)
{
    wxStatusBarBase::SetFieldsCount(number, widths);
    OH_LOG_INFO(LOG_APP, "[FULL_UI] wxStatusBar SetFieldsCount OK");
}

void wxStatusBar::SetStatusWidths(int n, const int widths_field[])
{
    wxStatusBarBase::SetStatusWidths(n, widths_field);
}

bool wxStatusBar::GetFieldRect(int i, wxRect& rect) const
{
    if ( i < 0 || i >= GetFieldsCount() )
        return false;

    const int w = wxMax(GetClientSize().x, 100);
    const int h = wxMax(m_minHeight - 2, 20);
    const int fieldW = wxMax(w / wxMax(GetFieldsCount(), 1), 100);
    rect = wxRect(i * fieldW, 0, fieldW, h);
    return true;
}

void wxStatusBar::SetMinHeight(int height)
{
    m_minHeight = height > 0 ? height : 22;
}

void wxStatusBar::DoUpdateStatusText(int number)
{
    wxUnusedVar(number);
    OH_LOG_INFO(LOG_APP, "[FULL_UI] wxStatusBar SetStatusText OK");
}

#endif // wxUSE_STATUSBAR
