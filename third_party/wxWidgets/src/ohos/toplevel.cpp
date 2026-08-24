/////////////////////////////////////////////////////////////////////////////
// wxTopLevelWindowOHOS — FW-2 NativeWindow Attach (Spike gui-b reuse)
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/toplevel.h"
    #include "wx/frame.h"
    #include "wx/menu.h"
    #include "wx/toolbar.h"
    #include "wx/bitmap.h"
    #include "wx/settings.h"
    #include "wx/iconbndl.h"
#endif

#include <cstdio>
#include <unordered_map>

#include "wx/ohos/nativewindow.h"

#include <hilog/log.h>
#include <native_window/external_window.h>
#include <native_buffer/native_buffer.h>

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xF004
#define LOG_TAG "wxOHOS"

namespace {

// Out-of-line backing store — keeps wxTopLevelWindowOHOS layout stable for
// CodeLite binaries built against older wx headers.
std::unordered_map<wxTopLevelWindowOHOS*, wxBitmap> g_tlwBacking;
// Attach-time surface size — clMainFrame GetSize/GetClientSize vtable drift.
std::unordered_map<wxTopLevelWindowOHOS*, wxSize> g_tlwSurfaceSize;

// R-4: TLW Show only painted itself; MenuBar/dock children stay dirty until
// each gets Update(). Parent first already ran; paint shown descendants next.
void OhosRefreshUpdateShownChildren(wxWindow* parent)
{
    if ( !parent )
        return;
    const wxWindowList& children = parent->GetChildren();
    for ( wxWindowList::compatibility_iterator node = children.GetFirst();
          node;
          node = node->GetNext() )
    {
        wxWindow* child = node->GetData();
        if ( !child || !child->IsShown() )
            continue;
        // Chrome painted last — children Clear at TLW (0,0) wipe strips.
        if ( wxDynamicCast(child, wxMenuBar) || wxDynamicCast(child, wxToolBar) )
            continue;
        child->Refresh(true);
        child->Update();
        OhosRefreshUpdateShownChildren(child);
    }
}

} // namespace

void wxTopLevelWindowOHOS::PaintMenuBarOverChildren()
{
    // Re-entrancy: chrome/dock Update → must not Present/re-enter overlay.
    static bool s_busy = false;
    if ( s_busy )
        return;
    wxFrame* frame = wxDynamicCast(this, wxFrame);
    if ( !frame )
        return;
    s_busy = true;

    // R-5.2: narrow AUI docks before chrome. Center/Welcome Clears often bind
    // at stale offset x=0 and erase the left Workspace pane otherwise.
    {
        const wxWindowList& children = GetChildren();
        for ( wxWindowList::compatibility_iterator node = children.GetFirst();
              node;
              node = node->GetNext() )
        {
            wxWindow* child = node->GetData();
            if ( !child || !child->IsShown() )
                continue;
            if ( wxDynamicCast(child, wxMenuBar) || wxDynamicCast(child, wxToolBar) )
                continue;
            const wxSize cs = child->GetClientSize();
            if ( cs.x > 0 && cs.x <= 280 && cs.y > 200 )
            {
                child->Refresh(true);
                child->Update();
                OhosRefreshUpdateShownChildren(child);
            }
        }
    }

    if ( wxMenuBar* bar = frame->GetMenuBar() )
    {
        if ( bar->IsShown() )
        {
            bar->Refresh(true);
            bar->Update();
        }
    }
#if wxUSE_TOOLBAR
    if ( wxToolBar* tbar = frame->GetToolBar() )
    {
        if ( tbar->IsShown() )
        {
            tbar->Refresh(true);
            tbar->Update();
        }
    }
#endif
    // Single flush after docks+chrome are in the backing (avoid per-child Present).
    (void)PresentBackingStore();
    s_busy = false;
}

void wxTopLevelWindowOHOS::RepaintShownChildrenOverBacking()
{
    // Same overlay path — kept as a named entry for large-wipe call sites.
    PaintMenuBarOverChildren();
}

void wxTopLevelWindowOHOS::Init()
{
    m_fsIsShowing = false;
    m_isMaximized = false;
    m_nativeWindow = nullptr;
    m_nativeWindowReferenced = false;
    m_nativeWindowPrepared = false;
}

wxBitmap* wxTopLevelWindowOHOS::GetBackingBitmap()
{
    auto it = g_tlwBacking.find(this);
    if ( it == g_tlwBacking.end() )
        return nullptr;
    return &it->second;
}

bool wxTopLevelWindowOHOS::EnsureBackingStore()
{
    if ( !m_nativeWindow )
        return false;

    // Full window size (includes MenuBar strip above client origin).
    int w = 0;
    int h = 0;
    GetSize(&w, &h);
    if ( w <= 0 || h <= 0 )
        GetClientSize(&w, &h);
    if ( w <= 0 || h <= 0 )
    {
        auto it = g_tlwSurfaceSize.find(this);
        if ( it != g_tlwSurfaceSize.end() )
        {
            w = it->second.x;
            h = it->second.y;
        }
    }
    if ( (w <= 0 || h <= 0) && m_nativeWindow )
    {
        auto* nw = reinterpret_cast<OHNativeWindow*>(m_nativeWindow);
        int32_t geoW = 0;
        int32_t geoH = 0;
        if ( OH_NativeWindow_NativeWindowHandleOpt(
                 nw, GET_BUFFER_GEOMETRY, &geoW, &geoH) == 0 &&
             geoW > 0 && geoH > 0 )
        {
            w = geoW;
            h = geoH;
        }
    }
    if ( w <= 0 || h <= 0 )
        return false;
    // Ignore placeholder sizes from pre-Show Create (default 20×20).
    if ( w < 64 || h < 64 )
        return false;

    wxBitmap& bmp = g_tlwBacking[this];
    if ( bmp.IsOk() &&
         bmp.GetWidth() == w &&
         bmp.GetHeight() == h &&
         bmp.GetOhosPixels() )
        return true;

    bmp.Create(w, h, 32);
    unsigned char* pixels = bmp.GetOhosPixels();
    if ( !bmp.IsOk() || !pixels )
    {
        OH_LOG_ERROR(LOG_APP, "[R-paint] FAIL EnsureBackingStore %dx%d", w, h);
        return false;
    }

    wxColour bg = GetBackgroundColour();
    if ( !bg.IsOk() )
        bg = wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE);

    // Direct fill — do not MemoryDC-Select the live backing (avoids COW/UAF).
    const size_t n = bmp.GetOhosByteCount();
    for ( size_t i = 0; i + 3 < n; i += 4 )
    {
        pixels[i + 0] = bg.Red();
        pixels[i + 1] = bg.Green();
        pixels[i + 2] = bg.Blue();
        pixels[i + 3] = bg.Alpha();
    }

    OH_LOG_INFO(LOG_APP,
                "[R-paint] OK TLW backing %{public}dx%{public}d stride=%{public}d "
                "buffer=%{public}p bg=%{public}02x%{public}02x%{public}02x",
                w, h, bmp.GetOhosStride(),
                static_cast<void*>(pixels),
                bg.Red(), bg.Green(), bg.Blue());
    return true;
}

bool wxTopLevelWindowOHOS::PresentBackingStore()
{
    if ( !m_nativeWindow )
        return false;
    if ( !EnsureBackingStore() )
        return false;
    wxBitmap* bmp = GetBackingBitmap();
    if ( !bmp )
        return false;

    // R-5.2 diagnostics: is Workspace ink still in TLW backing at flush?
    static int s_dockSample = 0;
    // Sample longer — Workspace paints after early chrome Presents.
    if ( s_dockSample < 80 && bmp->GetOhosPixels() )
    {
        ++s_dockSample;
        const unsigned char* base = bmp->GetOhosPixels();
        const int stride = bmp->GetOhosStride();
        const int bw = bmp->GetWidth();
        const int bh = bmp->GetHeight();
        size_t ink = 0;
        const int x0 = 60, y0 = 90, x1 = 200, y1 = 200;
        for ( int y = y0; y < y1 && y < bh; ++y )
        {
            const unsigned char* row = base + static_cast<size_t>(y) * stride;
            for ( int x = x0; x < x1 && x < bw; ++x )
            {
                const unsigned char* p = row + static_cast<size_t>(x) * 4;
                if ( p[0] < 180 || p[1] < 180 || p[2] < 180 )
                    ++ink;
            }
        }
        OH_LOG_INFO(LOG_APP,
                    "[WS-present] backing dock-sample ink=%{public}zu "
                    "bmp=%{public}dx%{public}d",
                    ink, bw, bh);
    }

    return wxOhos_PresentBitmap(m_nativeWindow, *bmp);
}

wxTopLevelWindowOHOS::~wxTopLevelWindowOHOS()
{
    DetachNativeWindow();
}

bool wxTopLevelWindowOHOS::Create(wxWindow *parent,
                                  wxWindowID id,
                                  const wxString& title,
                                  const wxPoint& pos,
                                  const wxSize& size,
                                  long style,
                                  const wxString& name)
{
    OH_LOG_INFO(LOG_APP, "[FULL_UI] TLW Create OHOS enter");

    const wxPoint posCopy = pos;
    const wxSize sizeCopy = size;
    const wxString titleCopy = title;

    if ( !CreateBase(parent, id, posCopy, sizeCopy, style, name) )
        return false;

    OH_LOG_INFO(LOG_APP, "[FULL_UI] TLW CreateBase OK");

    if ( parent )
        parent->AddChild(this);
    else
        wxTopLevelWindows.Append(this);

    m_title = titleCopy;

    int w = sizeCopy.x == wxDefaultCoord ? 20 : sizeCopy.x;
    int h = sizeCopy.y == wxDefaultCoord ? 20 : sizeCopy.y;
    int x = posCopy.x == wxDefaultCoord ? 0 : posCopy.x;
    int y = posCopy.y == wxDefaultCoord ? 0 : posCopy.y;
    SetSize(x, y, w, h);

    OH_LOG_INFO(LOG_APP, "[FULL_UI] TLW Create OHOS OK");

    std::fprintf(stderr,
                 "[FW-1] Create(wxTLW) ok title='%s' size=%dx%d handle=%p (C++ this*; awaiting Attach)\n",
                 (const char*)titleCopy.utf8_str(), w, h, (void*)this);
    if ( !m_nativeWindow )
    {
        std::fprintf(stderr,
                     "[FW-2] pending NativeWindow Attach: call wxOhos_AttachToTopWindow "
                     "from XComponent OnSurfaceCreated (Spike gui-b)\n");
    }
    std::fflush(stderr);

    return true;
}

bool wxTopLevelWindowOHOS::PrepareBoundNativeWindow(bool forCpu)
{
    auto* nw = reinterpret_cast<OHNativeWindow*>(m_nativeWindow);
    if ( !nw )
        return false;

    // Spike: SpikeRender::PrepareNativeWindow
    if ( !m_nativeWindowReferenced )
    {
        if ( OH_NativeWindow_NativeObjectReference(nw) == 0 )
            m_nativeWindowReferenced = true;
    }

    int w = 0, h = 0;
    GetSize(&w, &h);
    if ( w <= 0 || h <= 0 )
    {
        w = 800;
        h = 600;
    }

    const int32_t geoRet =
        OH_NativeWindow_NativeWindowHandleOpt(nw, SET_BUFFER_GEOMETRY, w, h);
    const int32_t fmtRet = OH_NativeWindow_NativeWindowHandleOpt(
        nw, SET_FORMAT, static_cast<int32_t>(NATIVEBUFFER_PIXEL_FMT_RGBA_8888));

    uint64_t usage = NATIVEBUFFER_USAGE_CPU_READ | NATIVEBUFFER_USAGE_CPU_WRITE |
                     NATIVEBUFFER_USAGE_MEM_DMA;
    if ( !forCpu )
    {
        usage |= NATIVEBUFFER_USAGE_HW_RENDER | NATIVEBUFFER_USAGE_HW_TEXTURE;
    }
    const int32_t usageRet =
        OH_NativeWindow_NativeWindowHandleOpt(nw, SET_USAGE, usage);

    std::fprintf(stderr,
                 "[FW-2] PrepareNativeWindow geo=%d fmt=%d usage=%d size=%dx%d cpu=%d\n",
                 (int)geoRet, (int)fmtRet, (int)usageRet, w, h, forCpu ? 1 : 0);
    std::fflush(stderr);

    m_nativeWindowPrepared = true;
    return true;
}

bool wxTopLevelWindowOHOS::AttachNativeWindow(void* ohNativeWindow, int width, int height)
{
    if ( !ohNativeWindow )
    {
        std::fprintf(stderr, "[FW-2.1] FAIL AttachNativeWindow: null\n");
        std::fflush(stderr);
        return false;
    }

    if ( m_nativeWindow && m_nativeWindow != ohNativeWindow )
        DetachNativeWindow();

    m_nativeWindow = ohNativeWindow;

    if ( width > 0 && height > 0 )
    {
        g_tlwSurfaceSize[this] = wxSize(width, height);
        SetSize(width, height);
    }

    // Spike-proven configure (CPU path default — same as gui-b PC emulator).
    PrepareBoundNativeWindow(/*forCpu=*/true);
    (void)EnsureBackingStore();

    // Tightened device evidence: TLW stored the pointer.
    std::fprintf(stderr, "[FW-2.3] m_nativeWindow=%p\n", m_nativeWindow);
    std::fflush(stderr);

    return m_nativeWindow != nullptr;
}

void wxTopLevelWindowOHOS::DetachNativeWindow()
{
    if ( !m_nativeWindow )
        return;

    auto* nw = reinterpret_cast<OHNativeWindow*>(m_nativeWindow);
    std::fprintf(stderr, "[FW-2] DetachNativeWindow %p referenced=%d\n",
                 m_nativeWindow, m_nativeWindowReferenced ? 1 : 0);

    if ( m_nativeWindowReferenced )
    {
        OH_NativeWindow_NativeObjectUnreference(nw);
        m_nativeWindowReferenced = false;
    }

    m_nativeWindow = nullptr;
    m_nativeWindowPrepared = false;
    g_tlwBacking.erase(this);
    g_tlwSurfaceSize.erase(this);
    std::fprintf(stderr, "[FW-2] Detach unbound (no crash)\n");
    std::fflush(stderr);
}

WXWidget wxTopLevelWindowOHOS::GetHandle() const
{
    if ( m_nativeWindow )
        return (WXWidget)m_nativeWindow;
    return (WXWidget)this;
}

bool wxTopLevelWindowOHOS::Show(bool show)
{
    const bool ok = wxWindowBase::Show(show);

    if ( m_nativeWindow )
    {
        // FW-2.4: operate on the bound NativeWindow, not flag-only.
        if ( show )
        {
            PrepareBoundNativeWindow(/*forCpu=*/true);
            // MV-4.5 P-1: Show must queue + deliver first paint (was missing).
            int tw = 0, th = 0;
            GetSize(&tw, &th);
            std::fprintf(stderr,
                         "[P-1] TLW Show→Refresh/Update size=%dx%d native=%p\n",
                         tw, th, m_nativeWindow);
            std::fflush(stderr);
            (void)EnsureBackingStore();
            Refresh(true);
            Update(); // paints into TLW backing + transparent Present
            // AUI panes / controls first; MenuBar last so Clear(0,0) cannot wipe it.
            OhosRefreshUpdateShownChildren(this);
            PaintMenuBarOverChildren();

            // R-1: prove wxRendererNative writes pixels (not CodeLite UI).
            static bool s_r1Probed = false;
            if ( !s_r1Probed )
            {
                s_r1Probed = true;
                (void)wxOhos_R1_ProbeRendererNative();
            }
        }

        std::fprintf(stderr,
                     "[MV-3] wxTLWOHOS::Show(%d) m_nativeWindow=%p attached=%d\n",
                     show ? 1 : 0, m_nativeWindow, (m_nativeWindow && show) ? 1 : 0);
        std::fflush(stderr);
    }
    else if ( IsTopLevel() )
    {
        std::fprintf(stderr,
                     "[FW-3] Show(%d) logical-only IsShown=%d handle=%p (no native map yet)\n",
                     show ? 1 : 0, IsShown() ? 1 : 0, (void*)GetHandle());
        std::fprintf(stderr,
                     "[FW-4] pending: user-visible window requires FW-2 NativeWindow\n");
        std::fflush(stderr);
    }

    return ok;
}

bool wxTopLevelWindowOHOS::Destroy()
{
    DetachNativeWindow();
    return wxTopLevelWindowBase::Destroy();
}

void wxTopLevelWindowOHOS::Maximize(bool maximize) { m_isMaximized = maximize; }
bool wxTopLevelWindowOHOS::IsMaximized() const { return m_isMaximized; }
void wxTopLevelWindowOHOS::Iconize(bool WXUNUSED(iconize)) {}
bool wxTopLevelWindowOHOS::IsIconized() const { return false; }
void wxTopLevelWindowOHOS::Restore() { m_isMaximized = false; }

bool wxTopLevelWindowOHOS::ShowFullScreen(bool show, long WXUNUSED(style))
{
    m_fsIsShowing = show;
    return true;
}

void wxTopLevelWindowOHOS::SetIcons(const wxIconBundle& WXUNUSED(icons))
{
    // OHOS desktop window icons are unsupported — skip ref-counted copy.
    OH_LOG_INFO(LOG_APP, "[FULL_UI] SetIcons bypass OHOS");
}

void wxTopLevelWindowOHOS::DoCentre(int WXUNUSED(dir))
{
    // Skip wxDisplay / screen geometry — embedded NativeWindow owns placement.
    OH_LOG_INFO(LOG_APP, "[FULL_UI] Centre bypass OHOS");
}
