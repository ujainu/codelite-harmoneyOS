/////////////////////////////////////////////////////////////////////////////
// wxFrame OHOS — SetMenuBar routes through wxFrameBase (F-UI-3.3e)
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/frame.h"
    #include "wx/menu.h"
#endif

#include "wx/ohos/frame.h"

#include <hilog/log.h>

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xF004
#define LOG_TAG "wxOHOS"

void wxFrame::SetMenuBar(wxMenuBar *menubar)
{
    // clMainFrame vtable slots for DetachMenuBar/AttachMenuBar differ from wxFrame;
    // wxFrameBase::SetMenuBar virtual calls hang before DetachMenuBar body (F-UI-3.3d).
    if ( menubar == wxFrameBase::GetMenuBar() )
        return;

    wxFrameBase::DetachMenuBar();
    wxFrameBase::AttachMenuBar(menubar);

    OH_LOG_INFO(LOG_APP, "[FUI_FRAME] SetMenuBar menubar=%{public}p", menubar);
}

wxPoint wxFrame::GetClientAreaOrigin() const
{
    // Route through wxFrameBase then apply menu/toolbar offsets via members —
    // not virtual GetMenuBar/GetToolBar (clMainFrame vtable slot drift, F-UI-3.4).
    wxPoint pt = wxFrameBase::GetClientAreaOrigin();

#if wxUSE_MENUS
    if ( m_frameMenuBar && m_frameMenuBar->IsShown() )
        pt.y += m_frameMenuBar->GetSize().y;
#endif

#if wxUSE_TOOLBAR
    if ( m_frameToolBar && m_frameToolBar->IsShown() )
    {
        if ( m_frameToolBar->GetWindowStyleFlag() & wxTB_VERTICAL )
            pt.x += m_frameToolBar->GetSize().x;
        else
            pt.y += m_frameToolBar->GetSize().y;
    }
#endif

    return pt;
}
