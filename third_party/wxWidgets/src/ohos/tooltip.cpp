/////////////////////////////////////////////////////////////////////////////
// Minimal wxToolTip for OHOS — boot-safe stub (no native tooltip UI yet)
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_TOOLTIPS

#include "wx/tooltip.h"

#ifndef WX_PRECOMP
    #include "wx/window.h"
#endif

/* static */ void wxToolTip::Enable(bool WXUNUSED(flag))
{
}

/* static */ void wxToolTip::SetDelay(long WXUNUSED(milliseconds))
{
}

/* static */ void wxToolTip::SetAutoPop(long WXUNUSED(milliseconds))
{
}

/* static */ void wxToolTip::SetReshow(long WXUNUSED(milliseconds))
{
}

wxToolTip::wxToolTip(const wxString& tip)
    : m_text(tip),
      m_window(nullptr)
{
}

void wxToolTip::SetTip(const wxString& tip)
{
    m_text = tip;
}

void wxToolTip::SetWindow(wxWindow *win)
{
    m_window = win;
}

#endif // wxUSE_TOOLTIPS
