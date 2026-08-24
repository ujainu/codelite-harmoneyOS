/////////////////////////////////////////////////////////////////////////////
// Name:        src/ohos/nativewindow.cpp
// Purpose:     C host bridge — XComponent surface → wxTopLevelWindowOHOS
//
// Spike path (verified): archive/spikes/gui-b OnSurfaceCreated → OHNativeWindow
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/toplevel.h"
#endif

#include "wx/ohos/nativewindow.h"
#include "wx/ohos/toplevel.h"

#include <cstdio>

namespace {
int g_paintDeliveryEnabled = 0;
} // namespace

extern "C" {

void wxOhos_SetPaintDeliveryEnabled(int enabled)
{
    g_paintDeliveryEnabled = enabled ? 1 : 0;
    std::fprintf(stderr, "[P-1] paintDelivery=%d\n", g_paintDeliveryEnabled);
    std::fflush(stderr);
}

int wxOhos_IsPaintDeliveryEnabled(void)
{
    return g_paintDeliveryEnabled;
}

int wxOhos_AttachToTopWindow(void* ohNativeWindow, int width, int height)
{
    if ( !ohNativeWindow )
    {
        std::fprintf(stderr, "[FW-2.2] FAIL AttachToTopWindow native=(nil)\n");
        std::fflush(stderr);
        return 0;
    }

    if ( !wxTheApp )
    {
        std::fprintf(stderr, "[FW-2.2] FAIL AttachToTopWindow: wxTheApp null\n");
        std::fflush(stderr);
        return 0;
    }

    wxWindow* top = wxTheApp->GetTopWindow();
    if ( !top )
    {
        // Fall back to first registered TLW (Create may precede SetTopWindow).
        if ( !wxTopLevelWindows.empty() )
            top = wxTopLevelWindows.GetFirst()->GetData();
    }

    // On OHOS, wxTopLevelWindow inherits wxTopLevelWindowOHOS; avoid wxDynamicCast.
    if ( !top || !top->IsTopLevel() )
    {
        std::fprintf(stderr,
                     "[FW-2.2] FAIL AttachToTopWindow: no top-level frame (top=%p)\n",
                     (void*)top);
        std::fflush(stderr);
        return 0;
    }

    auto* tlw = static_cast<wxTopLevelWindow*>(top);

    // MV-2: single-hop bind — OHNativeWindow → this TopLevel only (no Demo layer).
    std::fprintf(stderr,
                 "[MV-2] wxOhos_AttachToTopWindow native=%p tlw=%p IsTopLevel=%d\n",
                 ohNativeWindow, (void*)tlw, tlw->IsTopLevel() ? 1 : 0);
    std::fflush(stderr);

    wxOhos_SetPaintDeliveryEnabled(1);
    const int ok = tlw->AttachNativeWindow(ohNativeWindow, width, height) ? 1 : 0;
    if ( !ok )
        wxOhos_SetPaintDeliveryEnabled(0);
    return ok;
}

int wxOhos_DetachFromTopWindow(void)
{
    if ( !wxTheApp )
        return 0;

    wxWindow* top = wxTheApp->GetTopWindow();
    if ( !top && !wxTopLevelWindows.empty() )
        top = wxTopLevelWindows.GetFirst()->GetData();

    if ( !top || !top->IsTopLevel() )
        return 0;

    static_cast<wxTopLevelWindow*>(top)->DetachNativeWindow();
    return 1;
}

int wxOhos_TopWindowHasNativeWindow(void)
{
    if ( !wxTheApp )
        return 0;

    wxWindow* top = wxTheApp->GetTopWindow();
    if ( !top && !wxTopLevelWindows.empty() )
        top = wxTopLevelWindows.GetFirst()->GetData();

    if ( !top || !top->IsTopLevel() )
        return 0;

    return static_cast<wxTopLevelWindow*>(top)->GetNativeWindow() ? 1 : 0;
}

} // extern "C"
