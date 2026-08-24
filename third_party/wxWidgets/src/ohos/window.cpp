/////////////////////////////////////////////////////////////////////////////
// Name:        src/ohos/window.cpp
// Purpose:     Minimal wxWindow for OHOS GUI compile path
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/window.h"
    #include "wx/dcclient.h"
    #include "wx/toplevel.h"
#endif

#include "wx/ohos/nativewindow.h"
#include "wx/ohos/toplevel.h"

#include <cstdio>

#include <hilog/log.h>

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xF004
#define LOG_TAG "wxOHOS"

// DFB-style global mouse capture (DoCaptureMouse sets; GetCapture reads).
static wxWindow* gs_mouseCapture = nullptr;

static int g_p1_beginPaintCount = 0;

wxIMPLEMENT_DYNAMIC_CLASS(wxWindow, wxWindowBase);

void wxWindow::Init()
{
    m_rect = wxRect(0, 0, 0, 0);
}

wxWindow::~wxWindow()
{
    if ( gs_mouseCapture == this )
        ReleaseMouse();

    SendDestroyEvent();
    DestroyChildren();
}

// Methods below are on wxWindow (== wxWindowOHOS via macro in the header).
bool wxWindow::Create(wxWindow *parent,
                      wxWindowID id,
                      const wxPoint& pos,
                      const wxSize& size,
                      long style,
                      const wxString& name)
{
    if ( !CreateBase(parent, id, pos, size, style, wxDefaultValidator, name) )
        return false;

    if ( parent )
    {
        parent->AddChild(this);
    }
    else
    {
        // APP-002: TLWs must appear in wxTopLevelWindows so
        // wxApp::GetTopWindow() can find the main frame before SetTopWindow().
        wxTopLevelWindows.Append(this);
    }

    int w = size.x == wxDefaultCoord ? 20 : size.x;
    int h = size.y == wxDefaultCoord ? 20 : size.y;
    int x = pos.x == wxDefaultCoord ? 0 : pos.x;
    int y = pos.y == wxDefaultCoord ? 0 : pos.y;
    m_rect = wxRect(x, y, w, h);

    return true;
}

bool wxWindow::Show(bool show)
{
    const bool ok = wxWindowBase::Show(show);
    // First Window: TLW Show is still a base-class flag until NativeWindow exists.
    if ( IsTopLevel() )
    {
        std::fprintf(stderr,
                     "[FW-3] Show(%d) logical-only IsShown=%d handle=%p (no native map yet)\n",
                     show ? 1 : 0, IsShown() ? 1 : 0, (void*)GetHandle());
        std::fprintf(stderr, "[FW-4] pending: user-visible window requires FW-2 NativeWindow\n");
        std::fflush(stderr);
    }
    return ok;
}

void wxWindow::SetFocus()
{
    // no-op
}

void wxWindow::WarpPointer(int WXUNUSED(x), int WXUNUSED(y)) {}

void wxWindow::Refresh(bool WXUNUSED(eraseBackground), const wxRect *rect)
{
    // MV-4.5 P-1: was a no-op — Show/Invalidate never queued paint.
    if ( IsFrozen() )
        return;

    wxRect r = rect ? *rect : wxRect(GetClientSize());
    if ( r.width <= 0 || r.height <= 0 )
        r = wxRect(GetSize());
    if ( r.width <= 0 || r.height <= 0 )
        r = wxRect(0, 0, 800, 600); // last resort so Update is not skipped

    m_updateRegion.Union(r);
}

void wxWindow::Update()
{
    if ( IsFrozen() )
        return;
    if ( m_updateRegion.IsEmpty() )
        return;

    // Hidden notebook/AUI pages often keep child IsShown=true. Painting those
    // children still Clears into the shared TLW backing at stale geometry and
    // wipes the visible Workspace page (ink written, then overwritten).
    for ( const wxWindow* w = this; w; w = w->GetParent() )
    {
        if ( !w->IsShown() )
        {
            m_updateRegion.Clear();
            return;
        }
        if ( w->IsTopLevel() )
            break;
    }

    // Gate: off until Attach enables delivery (see wxOhos_SetPaintDeliveryEnabled).
    // CreateGUIControls → AuiManager::Update must not BeginPaint yet.
    // Keep m_updateRegion — clearing here was wiping MenuBar dirty before Show.
    if ( !wxOhos_IsPaintDeliveryEnabled() )
        return;

    // DFB-style: deliver Erase + Paint once NativeWindow exists (MV-4.5 P-1).
    const wxRect box = m_updateRegion.GetBox();

    // Containers with shown children must not Clear the full client rect:
    // Clear writes into the shared TLW backing and wipes already-painted
    // descendants (Workspace page ink disappeared while MenuBar survived
    // only because PaintMenuBarOverChildren repaints chrome).
    bool hasShownChild = false;
    {
        const wxWindowList& kids = GetChildren();
        for ( wxWindowList::compatibility_iterator node = kids.GetFirst();
              node;
              node = node->GetNext() )
        {
            wxWindow* ch = node->GetData();
            if ( ch && ch->IsShown() )
            {
                hasShownChild = true;
                break;
            }
        }
    }

    {
        static char s_cls[64] = "?";
        if ( GetClassInfo() && GetClassInfo()->GetClassName() )
        {
            const wxString name = GetClassInfo()->GetClassName();
            const wxScopedCharBuffer utf8 = name.utf8_str();
            std::snprintf(s_cls, sizeof(s_cls), "%s", utf8.data() ? utf8.data() : "?");
        }
        OH_LOG_INFO(LOG_APP,
                    "[P-1] BeginPaint win=%{public}p class=%{public}s "
                    "dirty=%{public}dx%{public}d+%{public}d,%{public}d clear=%{public}d",
                    static_cast<void*>(this), s_cls,
                    box.width, box.height, box.x, box.y,
                    hasShownChild ? 0 : 1);
    }
    std::fprintf(stderr,
                 "[P-1] BeginPaint win=%p dirty=%dx%d+%d,%d\n",
                 static_cast<void*>(this),
                 box.width, box.height, box.x, box.y);
    std::fflush(stderr);
    ++g_p1_beginPaintCount;

    {
        // Full-client empty panels (AUI/client fillers) Clear at offset covering
        // sibling docks and wipe Workspace ink. Skip Clear — parent/TLW bg
        // already filled; docks paint their own rects.
        bool fullClientFiller = false;
        if ( !hasShownChild )
        {
            if ( wxWindow* parent = GetParent() )
            {
                const wxSize pcs = parent->GetClientSize();
                const wxSize cs = GetClientSize();
                if ( pcs.x > 64 && pcs.y > 64 && cs.x > 0 && cs.y > 0 &&
                     std::abs(cs.x - pcs.x) <= 2 && std::abs(cs.y - pcs.y) <= 2 )
                    fullClientFiller = true;
            }
        }

        wxWindowDC dc(this);
        if ( !hasShownChild && !fullClientFiller )
        {
            // Platform default erase into TLW backing (bg colour). Without this,
            // GCDC-no-op paints leave the white EnsureBackingStore fill.
            wxColour bg = GetBackgroundColour();
            if ( !bg.IsOk() )
                bg = *wxWHITE;
            dc.SetBackground(wxBrush(bg));
            dc.Clear();

            wxEraseEvent erase(GetId(), &dc);
            erase.SetEventObject(this);
            HandleWindowEvent(erase);
        }

        wxPaintEvent paint(this);
        HandleWindowEvent(paint);
    }

    {
        const wxSize cs = GetClientSize();
        OH_LOG_INFO(LOG_APP,
                    "[P-2] after Paint clientSize=%{public}dx%{public}d updateWas=%{public}dx%{public}d",
                    cs.x, cs.y, box.width, box.height);
    }

    m_updateRegion.Clear();

    if ( g_p1_beginPaintCount == 1 )
        OH_LOG_INFO(LOG_APP, "[P-1] OK first BeginPaint/PaintEvent delivered");

    // P-4 / R-5.2: Present only from the TLW (or chrome overlay tail). Presenting
    // on every child Update exhausts NativeWindow buffers (waitfence timeout) and
    // leaves the screen stuck on an early frame without Workspace ink.
    if ( !IsTopLevel() )
        return;

    auto* tlw = static_cast<wxTopLevelWindowOHOS*>(this);
    if ( tlw->PresentBackingStore() )
    {
        static int s_presentOk = 0;
        if ( s_presentOk < 3 )
        {
            ++s_presentOk;
            OH_LOG_INFO(LOG_APP, "[R-present] OK TLW backing → NativeWindow flush");
        }

        // R-3: MainFrame backing not uniform white ⇒ official paint wrote pixels.
        static int s_r3 = 0;
        if ( s_r3 == 0 )
        {
            wxBitmap* bmp = tlw->GetBackingBitmap();
            if ( bmp && bmp->GetOhosPixels() )
            {
                const unsigned char* p = bmp->GetOhosPixels();
                const size_t n = bmp->GetOhosByteCount();
                size_t nonWhite = 0;
                for ( size_t i = 0; i + 3 < n; i += 16 ) // stride sample
                {
                    if ( p[i] != 255 || p[i + 1] != 255 || p[i + 2] != 255 )
                        ++nonWhite;
                }
                if ( nonWhite > 64 )
                {
                    s_r3 = 1;
                    OH_LOG_INFO(LOG_APP,
                                "[R-3] OK MainFrame backing non-white samples=%{public}zu",
                                nonWhite);
                }
            }
        }

        // R-4/R-5.1/R-5.2: restore narrow docks + chrome after TLW paint.
        tlw->PaintMenuBarOverChildren();
    }
}

bool wxWindow::SetCursor(const wxCursor& cursor)
{
    return wxWindowBase::SetCursor(cursor);
}

int wxWindow::GetCharHeight() const { return 8; }
int wxWindow::GetCharWidth() const { return 6; }

#if wxUSE_DRAG_AND_DROP
void wxWindow::SetDropTarget(wxDropTarget *dropTarget)
{
    delete m_dropTarget;
    m_dropTarget = dropTarget;
}
#endif

void wxWindow::SetScrollbar(int WXUNUSED(orient), int WXUNUSED(pos),
                            int WXUNUSED(thumbVisible), int WXUNUSED(range),
                            bool WXUNUSED(refresh)) {}
void wxWindow::SetScrollPos(int WXUNUSED(orient), int WXUNUSED(pos), bool WXUNUSED(refresh)) {}
int wxWindow::GetScrollPos(int WXUNUSED(orient)) const { return 0; }
int wxWindow::GetScrollThumb(int WXUNUSED(orient)) const { return 0; }
int wxWindow::GetScrollRange(int WXUNUSED(orient)) const { return 0; }
void wxWindow::ScrollWindow(int WXUNUSED(dx), int WXUNUSED(dy), const wxRect* WXUNUSED(rect)) {}

void wxWindow::DoGetTextExtent(const wxString& string,
                               int *x, int *y,
                               int *descent,
                               int *externalLeading,
                               const wxFont *WXUNUSED(font)) const
{
    if ( x ) *x = int(string.length()) * GetCharWidth();
    if ( y ) *y = GetCharHeight();
    if ( descent ) *descent = 0;
    if ( externalLeading ) *externalLeading = 0;
}

namespace {

// DFB-style: position of this window's client origin in "screen" (TLW) coords.
wxPoint OhosClientOriginOnScreen(const wxWindow* win)
{
    wxPoint pt(0, 0);
    const wxWindow* w = win;
    while ( w )
    {
        int px = 0, py = 0;
        w->GetPosition(&px, &py);
        const wxPoint origin = w->GetClientAreaOrigin();
        pt.x += px + origin.x;
        pt.y += py + origin.y;
        if ( w->IsTopLevel() )
            break;
        w = w->GetParent();
    }
    return pt;
}

} // namespace

void wxWindow::DoClientToScreen(int *x, int *y) const
{
    const wxPoint o = OhosClientOriginOnScreen(this);
    if ( x ) *x += o.x;
    if ( y ) *y += o.y;
}

void wxWindow::DoScreenToClient(int *x, int *y) const
{
    const wxPoint o = OhosClientOriginOnScreen(this);
    if ( x ) *x -= o.x;
    if ( y ) *y -= o.y;
}

void wxWindow::DoCaptureMouse()
{
    gs_mouseCapture = this;
}

void wxWindow::DoReleaseMouse()
{
    wxASSERT_MSG( gs_mouseCapture == this,
                  wxT("attempt to release mouse, but this window hasn't captured it") );
    gs_mouseCapture = nullptr;
}

/* static */ wxWindow* wxWindowBase::GetCapture()
{
    return gs_mouseCapture;
}

void wxWindow::DoGetPosition(int *x, int *y) const
{
    if ( x ) *x = m_rect.x;
    if ( y ) *y = m_rect.y;
}

void wxWindow::DoGetSize(int *width, int *height) const
{
    if ( width ) *width = m_rect.width;
    if ( height ) *height = m_rect.height;
}

void wxWindow::DoGetClientSize(int *width, int *height) const
{
    DoGetSize(width, height);
}

void wxWindow::DoSetSize(int x, int y, int width, int height, int sizeFlags)
{
    // DFB/MSW-style: update geometry AND deliver wxEVT_SIZE so sizers /
    // clSideBarCtrl::OnSize / notebook pages can cascade. Without this,
    // AUI can size SideBar (200×1264) while DefaultWorkspacePage stays 0×0.
    int currentX, currentY;
    GetPosition(&currentX, &currentY);
    int currentW, currentH;
    GetSize(&currentW, &currentH);

    if ( x == wxDefaultCoord && !(sizeFlags & wxSIZE_ALLOW_MINUS_ONE) )
        x = currentX;
    if ( y == wxDefaultCoord && !(sizeFlags & wxSIZE_ALLOW_MINUS_ONE) )
        y = currentY;

    wxSize best(-1, -1);
    if ( width == wxDefaultCoord )
    {
        if ( sizeFlags & wxSIZE_AUTO_WIDTH )
        {
            best = DoGetBestSize();
            width = best.x;
        }
        else
            width = currentW;
    }
    if ( height == wxDefaultCoord )
    {
        if ( sizeFlags & wxSIZE_AUTO_HEIGHT )
        {
            if ( best.x == -1 )
                best = DoGetBestSize();
            height = best.y;
        }
        else
            height = currentH;
    }

    const int minW = GetMinWidth();
    const int maxW = GetMaxWidth();
    const int minH = GetMinHeight();
    const int maxH = GetMaxHeight();
    if ( minW != wxDefaultCoord && width < minW )
        width = minW;
    if ( maxW != wxDefaultCoord && width > maxW )
        width = maxW;
    if ( minH != wxDefaultCoord && height < minH )
        height = minH;
    if ( maxH != wxDefaultCoord && height > maxH )
        height = maxH;

    if ( x == currentX && y == currentY && width == currentW && height == currentH )
        return;

    AdjustForParentClientOrigin(x, y, sizeFlags);
    DoMoveWindow(x, y, width, height);

    wxSizeEvent event(wxSize(width, height), GetId());
    event.SetEventObject(this);
    HandleWindowEvent(event);
}

void wxWindow::DoSetClientSize(int width, int height)
{
    SetSize(width, height);
}

void wxWindow::DoMoveWindow(int x, int y, int width, int height)
{
    m_rect = wxRect(x, y, width, height);
}

#if wxUSE_MENUS
bool wxWindow::DoPopupMenu(wxMenu *WXUNUSED(menu), int WXUNUSED(x), int WXUNUSED(y))
{
    return false;
}
#endif
