/////////////////////////////////////////////////////////////////////////////
// I-1…I-4: OHOS surface pointer → wxMouseEvent dispatch
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/frame.h"
    #include "wx/toplevel.h"
    #include "wx/utils.h"
#if wxUSE_MENUS
    #include "wx/menu.h"
#endif
#if wxUSE_TOOLBAR
    #include "wx/toolbar.h"
#endif
#endif

#include "wx/ohos/nativewindow.h"
#include "wx/ohos/toplevel.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>

#include <hilog/log.h>

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xF005
#define LOG_TAG "wxInput"

namespace {

enum OhosPointerAction
{
    OhosPointerMove = 0,
    OhosPointerDown,
    OhosPointerUp,
    OhosPointerCancel
};

int g_lastMouseX = 0;
int g_lastMouseY = 0;

wxWindow* OhosGetTopWindow()
{
    if ( !wxTheApp )
        return nullptr;

    wxWindow* top = wxTheApp->GetTopWindow();
    if ( !top && !wxTopLevelWindows.empty() )
        top = wxTopLevelWindows.GetFirst()->GetData();
    return top;
}

wxWindow* OhosFindTargetAtSurfacePoint(wxWindow* tlw, int x, int y)
{
    const wxPoint screen(x, y);
    wxWindow* hit = wxGenericFindWindowAtPoint(screen);
    if ( hit )
        return hit;

    if ( wxFrame* frame = wxDynamicCast(tlw, wxFrame) )
    {
        if ( wxMenuBar* bar = frame->GetMenuBar() )
        {
            if ( bar->IsShown() && y >= 0 && y < bar->GetSize().y )
                return bar;
        }
#if wxUSE_TOOLBAR
        if ( wxToolBar* tbar = frame->GetToolBar() )
        {
            if ( tbar->IsShown() )
            {
                wxPoint tpt = tbar->GetPosition();
                wxSize tsz = tbar->GetSize();
                if ( x >= tpt.x && y >= tpt.y &&
                     x < tpt.x + tsz.x && y < tpt.y + tsz.y )
                    return tbar;
            }
        }
#endif
    }

    return tlw;
}

wxEventType OhosMapPointerEvent(OhosPointerAction action, int button)
{
    switch ( action )
    {
        case OhosPointerMove:
            return wxEVT_MOTION;
        case OhosPointerDown:
            if ( button == 2 )
                return wxEVT_MIDDLE_DOWN;
            if ( button == 1 )
                return wxEVT_RIGHT_DOWN;
            return wxEVT_LEFT_DOWN;
        case OhosPointerUp:
            if ( button == 2 )
                return wxEVT_MIDDLE_UP;
            if ( button == 1 )
                return wxEVT_RIGHT_UP;
            return wxEVT_LEFT_UP;
        case OhosPointerCancel:
            return wxEVT_LEFT_UP;
        default:
            return wxEVT_NULL;
    }
}

const char* OhosActionProbeTag(OhosPointerAction action)
{
    switch ( action )
    {
        case OhosPointerMove: return "MOVE";
        case OhosPointerDown: return "DOWN";
        case OhosPointerUp:   return "UP";
        case OhosPointerCancel: return "CANCEL";
        default: return "?";
    }
}

void OhosProbeOnce(const char* WXUNUSED(gate), bool& flag, const char* fmt, ...)
{
    if ( flag )
        return;
    flag = true;
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    OH_LOG_INFO(LOG_APP, "%{public}s", buf);
    std::fprintf(stderr, "%s\n", buf);
    std::fflush(stderr);
}

int OhosDispatchMouseEventImpl(int action, int button, float x, float y)
{
    static bool probedI1 = false;
    static bool probedI2 = false;
    static bool probedI3 = false;
    static bool probedI4 = false;

    const int ix = static_cast<int>(std::lround(x));
    const int iy = static_cast<int>(std::lround(y));
    g_lastMouseX = ix;
    g_lastMouseY = iy;

    OhosProbeOnce("I-1", probedI1,
                  "[I-1] OK pointer surface=(%d,%d) action=%s btn=%d",
                  ix, iy, OhosActionProbeTag(static_cast<OhosPointerAction>(action)), button);

    if ( action == OhosPointerDown )
    {
        OhosProbeOnce("I-2", probedI2,
                      "[I-2] OK pointer down at (%d,%d) btn=%d", ix, iy, button);
    }

    wxWindow* tlw = OhosGetTopWindow();
    if ( !tlw || !tlw->IsTopLevel() )
    {
        OH_LOG_ERROR(LOG_APP, "[I-3] FAIL dispatch: no TopWindow");
        return 0;
    }

    const wxEventType evType = OhosMapPointerEvent(static_cast<OhosPointerAction>(action), button);
    if ( evType == wxEVT_NULL )
        return 0;

    wxWindow* target = OhosFindTargetAtSurfacePoint(tlw, ix, iy);
    if ( !target )
        target = tlw;

    wxPoint local = target->ScreenToClient(wxPoint(ix, iy));

    wxMouseEvent event(evType);
    event.SetEventObject(target);
    event.SetId(target->GetId());
    event.SetTimestamp(static_cast<long>(wxGetLocalTimeMillis().GetValue()));
    event.m_x = local.x;
    event.m_y = local.y;
    event.m_leftDown = (action == OhosPointerDown && button == 0) ||
                       (action == OhosPointerMove && (evType == wxEVT_MOTION));
    event.m_rightDown = action == OhosPointerDown && button == 1;
    event.m_middleDown = action == OhosPointerDown && button == 2;

    OhosProbeOnce("I-3", probedI3,
                  "[I-3] OK wxMouseEvent type=%d local=(%d,%d) target=%p",
                  evType, local.x, local.y, static_cast<void*>(target));

    wxString clsName(wxT("?"));
    if ( target->GetClassInfo() )
        clsName = target->GetClassInfo()->GetClassName();
    const wxScopedCharBuffer clsUtf8 = clsName.utf8_str();
    OhosProbeOnce("I-4", probedI4,
                  "[I-4] OK hit target class=%s win=%p surface=(%d,%d)",
                  clsUtf8.data() ? clsUtf8.data() : "?", static_cast<void*>(target), ix, iy);

    const bool processed = target->HandleWindowEvent(event);

    if ( action != OhosPointerMove )
    {
        target->Refresh(false);
        target->Update();
        if ( tlw->IsTopLevel() )
        {
            auto* ohTlw = static_cast<wxTopLevelWindowOHOS*>(tlw);
            ohTlw->PresentBackingStore();
        }
    }

    return processed ? 1 : 0;
}

} // namespace

extern "C" {

void wxOhos_DispatchMouseEvent(int action, int button, float x, float y)
{
    if ( !wxTheApp )
    {
        std::fprintf(stderr, "[I-3] FAIL dispatch: wxTheApp null\n");
        std::fflush(stderr);
        return;
    }

    // XComponent callbacks may arrive off the wx run-loop thread — marshal in.
    wxTheApp->CallAfter([action, button, x, y]() {
        (void)OhosDispatchMouseEventImpl(action, button, x, y);
    });
    wxTheApp->WakeUpIdle();
}

int wxOhos_GetLastMouseX(void)
{
    return g_lastMouseX;
}

int wxOhos_GetLastMouseY(void)
{
    return g_lastMouseY;
}

} // extern "C"
