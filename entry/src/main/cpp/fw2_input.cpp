/////////////////////////////////////////////////////////////////////////////
// I-1…I-6.4: XComponent pointer → wxMouseEvent (host-side until wx .so relink)
/////////////////////////////////////////////////////////////////////////////

#include "fw2_input.h"
#include "fw2_build_bridge.h"

#include <hilog/log.h>

#include <cmath>
#include <cstdarg>
#include <cstdio>

#include "wx/wxprec.h"

// 只有 WX_PRECOMP 没有开启时才手动 include 常用 wx 头（和 fw2_wx_host.cpp 保持一致）。
// 这保证 wxObject/wxWindowBase/wxAcceleratorTable 等基础类的内联成员函数 layout
// 永远和 libwx_ohosu_core-3.3-OHOS.so 编译时用的版本一致，避免 ODR 冲突导致
// wxObject::Ref 在 TabgroupsPane 构造里 SIGSEGV。
#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/window.h"
    #if wxUSE_TOOLBAR
        #include "wx/toolbar.h"
    #endif
    #if defined(__WXOHOS__)
        #include "wx/frame.h"
    #else
        #include "wx/univ/frame.h"
    #endif
    #include "wx/menu.h"
    #include "wx/menuitem.h"
    #include "wx/toplevel.h"
    #include "wx/utils.h"
    #include "wx/dcclient.h"
    #include "wx/evtloop.h"

    // OHOS 平台的 wx/ohos/dnd.h 是空文件（no-op），而 setup.h 里
    // wxUSE_DRAG_AND_DROP=1。STC 头会在链里 include wx/dnd.h，
    // 所以单独把 STC include 放在这里，并在局部 disable DnD guard，
    // 避免 ohos 下空 dnd.h 造成 wxTextDropTarget 从 forward decl
    // 继承导致 incomplete type 编译错误。
    #if defined(__WXOHOS__) && wxUSE_DRAG_AND_DROP
        #undef wxUSE_DRAG_AND_DROP
        #define wxUSE_DRAG_AND_DROP 0
    #endif
    #include "wx/stc/stc.h"
#endif

#if wxUSE_AUI
    #include "wx/aui/framemanager.h"
    #include "wx/aui/dockart.h"
#endif

// STC 用到（wxStyledTextCtrl dynamic_cast + 方法调用）需要完整类定义。
// 但上一步若 WX_PRECOMP 已 define（setup 里开了 PCH），则上面 block 被跳过，
// 此时 wxUSE_DRAG_AND_DROP 仍是 setup.h 的 1，这里再次先 force-disable DnD
//（在 include wx/dnd.h 之前），然后再 include STC 头。
#if defined(WX_PRECOMP)
    #include "wx/defs.h"
    // evtloop.h 不在 wx/wx.h 主头里，WX_PRECOMP 打开时必须手工 include。
    #include "wx/evtloop.h"
    #if defined(__WXOHOS__) && wxUSE_DRAG_AND_DROP
        #undef wxUSE_DRAG_AND_DROP
        #define wxUSE_DRAG_AND_DROP 0
    #endif
    #include "wx/stc/stc.h"
#endif

#include "wx/ohos/toplevel.h"

#include <ace/xcomponent/native_xcomponent_key_event.h>

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xF002
#define LOG_TAG "FW2Input"

static bool g_forwardAllMoves = false;
static wxWindow* g_hostKeyFocus = nullptr;

namespace {

enum PointerAction
{
    kMove = 0,
    kDown = 1,
    kUp = 2,
    kCancel = 3
};

int g_lastMouseX = 0;
int g_lastMouseY = 0;
bool g_leftDown = false;
bool g_rightDown = false;
bool g_middleDown = false;
int g_menuBarPressIndex = -1;
bool g_auiDragActive = false;
int g_auiDragStartX = 0;
int g_auiDragStartY = 0;
int g_auiDragStartPaneWidth = -1;
wxString g_auiDragPaneName;
#if wxUSE_AUI
bool g_probedI644 = false;
#endif

class Fw2MenuPopup;
Fw2MenuPopup* g_activeMenuPopup = nullptr;
int g_pendingMenuCmdId = 0;
wxFrame* g_pendingMenuFrame = nullptr;

wxWindow* GetTopWindow();
void PresentIfNeeded(wxWindow* tlw);
void FlushPendingMenuCommand();

int MenuPopupRowAtY(int localY)
{
    if ( localY < 4 )
        return -1;
    return (localY - 4) / 22;
}

wxMenuItem* MenuPopupItemAtRow(wxMenu* menu, int row)
{
    if ( !menu || row < 0 )
        return nullptr;
    int current = 0;
    for ( wxMenuItemList::compatibility_iterator node = menu->GetMenuItems().GetFirst();
          node;
          node = node->GetNext() )
    {
        wxMenuItem* item = node->GetData();
        if ( !item || item->IsSeparator() )
            continue;
        if ( current == row )
            return item;
        ++current;
    }
    return nullptr;
}

wxMenuItem* ResolveFileOpenDispatchItem(wxMenuItem* item)
{
    if ( !item )
        return nullptr;
    if ( wxMenu* sub = item->GetSubMenu() )
    {
        const wxString lbl = wxStripMenuCodes(item->GetItemLabel());
        if ( lbl.CmpNoCase("Open") == 0 )
        {
            for ( wxMenuItemList::compatibility_iterator node = sub->GetMenuItems().GetFirst();
                  node;
                  node = node->GetNext() )
            {
                wxMenuItem* child = node->GetData();
                if ( child && !child->IsSeparator() )
                    return child;
            }
        }
    }
    return item;
}

class Fw2MenuPopup : public wxWindow
{
public:
    Fw2MenuPopup(wxFrame* frame, wxMenu* menu, const wxPoint& posScreen)
        : m_frame(frame)
        , m_menu(menu)
    {
        int maxW = 120;
        const int rowH = 22;
        int rows = 0;
        for ( wxMenuItemList::compatibility_iterator node = menu->GetMenuItems().GetFirst();
              node;
              node = node->GetNext() )
        {
            wxMenuItem* item = node->GetData();
            if ( !item || item->IsSeparator() )
                continue;
            const wxString label = wxStripMenuCodes(item->GetItemLabel());
            maxW = std::max(maxW, int(label.length()) * 7 + 24);
            ++rows;
        }
        if ( rows <= 0 )
            rows = 1;

        Create(frame, wxID_ANY, posScreen, wxSize(maxW, rows * rowH + 8), wxBORDER_SIMPLE);
        SetBackgroundColour(*wxWHITE);
        Bind(wxEVT_PAINT, &Fw2MenuPopup::OnPaint, this);
        Bind(wxEVT_LEFT_UP, &Fw2MenuPopup::OnLeftUp, this);
        Show(true);
        Raise();
        Refresh();
        Update();
    }

    void Dismiss()
    {
        Destroy();
    }

private:
    void OnPaint(wxPaintEvent&)
    {
        wxPaintDC dc(this);
        dc.SetBackground(wxBrush(GetBackgroundColour()));
        dc.Clear();
        dc.SetTextForeground(*wxBLACK);

        int y = 4;
        const int rowH = 22;
        for ( wxMenuItemList::compatibility_iterator node = m_menu->GetMenuItems().GetFirst();
              node;
              node = node->GetNext() )
        {
            wxMenuItem* item = node->GetData();
            if ( !item || item->IsSeparator() )
                continue;
            dc.DrawText(wxStripMenuCodes(item->GetItemLabel()), 8, y);
            y += rowH;
        }
    }

    void OnLeftUp(wxMouseEvent& event)
    {
        if ( m_frame && m_menu )
        {
            const int row = MenuPopupRowAtY(event.GetY());
            if ( wxMenuItem* item = MenuPopupItemAtRow(m_menu, row) )
            {
                if ( wxMenuItem* dispatchItem = ResolveFileOpenDispatchItem(item) )
                    item = dispatchItem;
                if ( !item->IsSeparator() && item->IsEnabled() )
                {
                    const wxString label = wxStripMenuCodes(item->GetItemLabel());
                    OH_LOG_INFO(LOG_APP,
                                "[F-1.1] OK menu command dispatch id=%{public}d label=%{public}s",
                                item->GetId(), label.utf8_str().data());
                    if ( label.Contains(wxT("Save")) && !label.Contains(wxT("Save As")) )
                    {
                        OH_LOG_INFO(LOG_APP,
                                    "[F-2.1] OK menu command dispatch id=%{public}d label=%{public}s",
                                    item->GetId(), label.utf8_str().data());
                    }
                    if ( label.Contains(wxT("Open Workspace")) )
                    {
                        OH_LOG_INFO(LOG_APP,
                                    "[F-3.1] OK menu command dispatch id=%{public}d label=%{public}s",
                                    item->GetId(), label.utf8_str().data());
                    }
                    g_pendingMenuCmdId = item->GetId();
                    g_pendingMenuFrame = m_frame;
                    FlushPendingMenuCommand();
                    return;
                }
            }
        }
        Dismiss();
        g_activeMenuPopup = nullptr;
        event.Skip();
    }

    wxFrame* m_frame;
    wxMenu* m_menu;
};

void FlushPendingMenuCommand()
{
    if ( !g_pendingMenuCmdId || !g_pendingMenuFrame )
        return;

    const int cmdId = g_pendingMenuCmdId;
    wxFrame* menuFrame = g_pendingMenuFrame;
    g_pendingMenuCmdId = 0;
    g_pendingMenuFrame = nullptr;

    if ( g_activeMenuPopup )
    {
        g_activeMenuPopup->Dismiss();
        g_activeMenuPopup = nullptr;
    }

    wxFrame* frame = wxDynamicCast(GetTopWindow(), wxFrame);
    if ( !frame )
        frame = menuFrame;

    wxString frameClass(wxT("?"));
    if ( frame && frame->GetClassInfo() )
        frameClass = frame->GetClassInfo()->GetClassName();

    const bool handledCmd = frame->ProcessCommand(cmdId);
    OH_LOG_INFO(LOG_APP,
                "[F-1.dispatch] ProcessCommand frame=%{public}p class=%{public}s "
                "cmdId=%{public}d handled=%{public}d",
                static_cast<void*>(frame), frameClass.utf8_str().data(), cmdId,
                handledCmd ? 1 : 0);
    if ( wxWindow* tlw = GetTopWindow() )
        PresentIfNeeded(tlw);
}

wxWindow* GetTopWindow()
{
    if ( !wxTheApp )
        return nullptr;
    wxWindow* top = wxTheApp->GetTopWindow();
    if ( !top && !wxTopLevelWindows.empty() )
        top = wxTopLevelWindows.GetFirst()->GetData();
    return top;
}

wxPoint EventPointForTarget(wxWindow* target, int surfaceX, int surfaceY);
wxWindow* FindTargetAtSurfacePoint(wxWindow* tlw, int x, int y);
void ProbeOnce(const char* tag, bool& flag, const char* fmt, ...);

bool IsKeyRecipientClass(wxWindow* win)
{
    if ( !win || !win->GetClassInfo() )
        return false;
    const wxString cn = win->GetClassInfo()->GetClassName();
    return cn == wxT("wxStyledTextCtrl") || cn == wxT("clEditor");
}

wxWindow* FindKeyRecipientInSubtree(wxWindow* root)
{
    if ( !root )
        return nullptr;
    if ( IsKeyRecipientClass(root) )
        return root;
    for ( wxWindowList::compatibility_iterator it = root->GetChildren().GetFirst();
          it;
          it = it->GetNext() )
    {
        if ( wxWindow* found = FindKeyRecipientInSubtree(it->GetData()) )
            return found;
    }
    return nullptr;
}

bool SurfacePointInWindow(wxWindow* win, int sx, int sy)
{
    if ( !win || !win->IsShown() )
        return false;
    const wxPoint tl = win->GetScreenPosition();
    const wxSize sz = win->GetSize();
    if ( sz.x <= 0 || sz.y <= 0 )
        return false;
    return sx >= tl.x && sy >= tl.y && sx < tl.x + sz.x && sy < tl.y + sz.y;
}

wxWindow* FindKeyRecipientAtSurfacePoint(wxWindow* root, int sx, int sy)
{
    if ( !root || !root->IsShown() )
        return nullptr;

    for ( wxWindowList::compatibility_iterator it = root->GetChildren().GetFirst();
          it;
          it = it->GetNext() )
    {
        if ( wxWindow* found = FindKeyRecipientAtSurfacePoint(it->GetData(), sx, sy) )
            return found;
    }

    if ( !SurfacePointInWindow(root, sx, sy) )
        return nullptr;
    if ( IsKeyRecipientClass(root) )
        return root;
    return nullptr;
}

wxWindow* RefineKeyDispatchTarget(wxWindow* hit, int sx, int sy)
{
    if ( !hit )
        return nullptr;
    if ( IsKeyRecipientClass(hit) )
        return hit;
    if ( wxWindow* atPoint = FindKeyRecipientAtSurfacePoint(hit, sx, sy) )
        return atPoint;
    if ( wxWindow* sub = FindKeyRecipientInSubtree(hit) )
        return sub;
    if ( wxWindow* tlw = GetTopWindow() )
    {
        if ( wxWindow* atPoint = FindKeyRecipientAtSurfacePoint(tlw, sx, sy) )
            return atPoint;
    }
    return hit;
}

bool UsesSurfaceCoords(wxWindow* win)
{
    if ( !win )
        return false;
    if ( wxDynamicCast(win, wxMenuBar) )
        return true;
    const wxPoint pos = win->GetPosition();
    if ( pos.y < 0 )
        return true;
    if ( wxWindow* parent = win->GetParent() )
    {
        if ( parent->IsTopLevel() && pos.y >= 0 && pos.y < 64 )
            return true;
    }
    return false;
}

#if wxUSE_AUI
struct AuiSashHit
{
    bool hit = false;
    int partType = 0;
    wxString paneName;
};

int AuiSashSize(wxAuiManager* mgr)
{
    int sashSize = 6;
    if ( mgr && mgr->GetArtProvider() )
        sashSize = mgr->GetArtProvider()->GetMetric(wxAUI_DOCKART_SASH_SIZE);
    return sashSize > 0 ? sashSize : 6;
}

const char* AuiPartTypeName(int type)
{
    switch ( type )
    {
        case wxAuiDockUIPart::typeDockSizer: return "DockSizer";
        case wxAuiDockUIPart::typePaneSizer: return "PaneSizer";
        case wxAuiDockUIPart::typeCaption: return "Caption";
        case wxAuiDockUIPart::typeGripper: return "Gripper";
        case wxAuiDockUIPart::typePane: return "Pane";
        default: return "Other";
    }
}

AuiSashHit FindAuiSashHit(wxAuiManager* mgr, int cx, int cy)
{
    AuiSashHit result;
    if ( !mgr )
        return result;

    const int half = AuiSashSize(mgr) + 3;

    for ( const wxAuiPaneInfo& pane : mgr->GetAllPanes() )
    {
        if ( !pane.IsShown() || pane.IsFloating() )
            continue;

        const wxRect& r = pane.rect;
        if ( r.width <= 0 || r.height <= 0 )
            continue;

        switch ( pane.dock_direction )
        {
            case wxAUI_DOCK_LEFT:
                if ( cx >= r.x + r.width - half && cx < r.x + r.width + half
                     && cy >= r.y && cy < r.y + r.height )
                {
                    result.hit = true;
                    result.partType = wxAuiDockUIPart::typeDockSizer;
                    result.paneName = pane.name;
                    return result;
                }
                break;
            case wxAUI_DOCK_RIGHT:
                if ( cx >= r.x - half && cx < r.x
                     && cy >= r.y && cy < r.y + r.height )
                {
                    result.hit = true;
                    result.partType = wxAuiDockUIPart::typeDockSizer;
                    result.paneName = pane.name;
                    return result;
                }
                break;
            case wxAUI_DOCK_TOP:
                if ( cy >= r.y + r.height - half && cy <= r.y + r.height + half
                     && cx >= r.x && cx < r.x + r.width )
                {
                    result.hit = true;
                    result.partType = wxAuiDockUIPart::typeDockSizer;
                    result.paneName = pane.name;
                    return result;
                }
                break;
            case wxAUI_DOCK_BOTTOM:
                if ( cy >= r.y - half && cy <= r.y + half
                     && cx >= r.x && cx < r.x + r.width )
                {
                    result.hit = true;
                    result.partType = wxAuiDockUIPart::typeDockSizer;
                    result.paneName = pane.name;
                    return result;
                }
                break;
            default:
                break;
        }
    }

    return result;
}

int PaneWidthByName(wxAuiManager* mgr, const wxString& name)
{
    if ( !mgr )
        return -1;
    for ( const wxAuiPaneInfo& pane : mgr->GetAllPanes() )
    {
        if ( pane.name == name && pane.rect.width > 0 )
            return pane.rect.width;
    }
    return -1;
}

wxAuiManager* GetAuiManager(wxWindow* tlw)
{
    if ( !tlw )
        return nullptr;
    wxAuiManager* mgr = wxAuiManager::GetManager(tlw);
    if ( mgr )
        return mgr;
    for ( wxWindow* child : tlw->GetChildren() )
    {
        if ( (mgr = wxAuiManager::GetManager(child)) != nullptr )
            return mgr;
    }
    return nullptr;
}
#endif // wxUSE_AUI

struct DispatchTarget
{
    wxWindow* window = nullptr;
    wxPoint local;
#if wxUSE_AUI
    AuiSashHit sash;
    wxAuiManager* auiMgr = nullptr;
    wxWindow* auiManaged = nullptr;
#endif
};

void FillAuiDispatchContext(DispatchTarget& out, wxWindow* tlw, int ix, int iy)
{
#if wxUSE_AUI
    out.auiMgr = GetAuiManager(tlw);
    if ( !out.auiMgr )
        return;

    out.auiManaged = out.auiMgr->GetManagedWindow();
    if ( !out.auiManaged )
        return;

    const wxPoint managedLocal = out.auiManaged->ScreenToClient(wxPoint(ix, iy));
    out.sash = FindAuiSashHit(out.auiMgr, managedLocal.x, managedLocal.y);
#else
    wxUnusedVar(out);
    wxUnusedVar(tlw);
    wxUnusedVar(ix);
    wxUnusedVar(iy);
#endif
}

void TryBeginAuiDragEarly(int action, int ix, int iy)
{
#if wxUSE_AUI
    if ( action != kDown )
        return;

    wxWindow* tlw = GetTopWindow();
    if ( !tlw || !tlw->IsTopLevel() )
        return;

    DispatchTarget dt;
    FillAuiDispatchContext(dt, tlw, ix, iy);
    if ( !dt.auiMgr || !dt.auiManaged || !dt.sash.hit )
        return;

    g_auiDragActive = true;
    g_forwardAllMoves = true;
    g_auiDragStartX = ix;
    g_auiDragStartY = iy;
    g_auiDragPaneName = dt.sash.paneName;
    g_auiDragStartPaneWidth = PaneWidthByName(dt.auiMgr, dt.sash.paneName);
#endif
}

DispatchTarget ResolveDispatchTarget(wxWindow* tlw, int ix, int iy)
{
    DispatchTarget out;

    FillAuiDispatchContext(out, tlw, ix, iy);

    // Check menubar FIRST, before mouse capture. Otherwise kUp after a kDown
    // that set mouse capture would route to the captured window instead of
    // the menubar, and HandleMenuBarUp would never fire.
    wxWindow* chrome = FindTargetAtSurfacePoint(tlw, ix, iy);
    if ( chrome && UsesSurfaceCoords(chrome) )
    {
        out.window = chrome;
        out.local = wxPoint(ix, iy);
        return out;
    }

    if ( wxWindow* capture = wxWindow::GetCapture() )
    {
        out.window = capture;
        out.local = capture->ScreenToClient(wxPoint(ix, iy));
        return out;
    }

#if wxUSE_AUI
    if ( out.auiMgr && out.auiManaged )
    {
        const wxPoint managedLocal = out.auiManaged->ScreenToClient(wxPoint(ix, iy));
        if ( out.sash.hit || g_auiDragActive )
        {
            out.window = out.auiManaged;
            out.local = managedLocal;
            return out;
        }
    }
#endif

    wxWindow* hit = chrome ? chrome : tlw;
    if ( !hit )
        hit = tlw;
    out.window = hit;
    out.local = EventPointForTarget(hit, ix, iy);
    return out;
}

void UpdateHostKeyFocusSync(int action, int ix, int iy)
{
    if ( action != kDown )
        return;
    wxWindow* tlw = GetTopWindow();
    if ( !tlw )
        return;
    DispatchTarget dt = ResolveDispatchTarget(tlw, ix, iy);
    wxWindow* target = dt.window ? dt.window : tlw;
    g_hostKeyFocus = RefineKeyDispatchTarget(target, ix, iy);
}

void ProbeAuiDock(int action, const DispatchTarget& dt, int ix, int iy)
{
#if !wxUSE_AUI
    wxUnusedVar(action);
    wxUnusedVar(dt);
    wxUnusedVar(ix);
    wxUnusedVar(iy);
#else
    static bool probed641 = false;
    static bool probed642 = false;

    if ( !dt.auiMgr || !dt.auiManaged )
        return;

    if ( action == kDown && (dt.sash.hit || g_auiDragActive) )
    {
        ProbeOnce("I-6.4.1", probed641,
                  "[I-6.4.1] OK pointer surface=(%d,%d) managedLocal=(%d,%d)",
                  ix, iy, dt.local.x, dt.local.y);
    }

    if ( action == kDown && dt.sash.hit )
    {
        g_auiDragActive = true;
        g_forwardAllMoves = true;
        ProbeOnce("I-6.4.2", probed642,
                  "[I-6.4.2] OK target=wxAuiManager sash=%s pane=%s managed=%p mgr=%p",
                  AuiPartTypeName(dt.sash.partType),
                  dt.sash.paneName.utf8_str().data(),
                  static_cast<void*>(dt.auiManaged),
                  static_cast<void*>(dt.auiMgr));
        g_auiDragStartX = ix;
        g_auiDragStartY = iy;
        g_auiDragPaneName = dt.sash.paneName;
        g_auiDragStartPaneWidth = PaneWidthByName(dt.auiMgr, dt.sash.paneName);
    }

    if ( action == kMove && g_auiDragActive && !g_probedI644 )
    {
        const int dx = ix - g_auiDragStartX;
        const int dy = iy - g_auiDragStartY;
        if ( std::abs(dx) >= 100 || std::abs(dy) >= 100 )
        {
            ProbeOnce("I-6.4.4", g_probedI644,
                      "[I-6.4.4] OK move delta=(%d,%d) capture=%p pane=%s managed=%p",
                      dx, dy,
                      static_cast<void*>(wxWindow::GetCapture()),
                      g_auiDragPaneName.utf8_str().data(),
                      static_cast<void*>(dt.auiManaged));
        }
    }
#endif
}

void ProbeAuiDockPostDispatch(int action, const DispatchTarget& dt, int ix, int iy)
{
#if !wxUSE_AUI
    wxUnusedVar(action);
    wxUnusedVar(dt);
    wxUnusedVar(ix);
    wxUnusedVar(iy);
#else
    static bool probed645 = false;

    if ( !dt.auiMgr || !dt.auiManaged )
        return;

    if ( action == kUp && g_auiDragActive && !g_probedI644 )
    {
        const int dx = ix - g_auiDragStartX;
        const int dy = iy - g_auiDragStartY;
        ProbeOnce("I-6.4.4", g_probedI644,
                  "[I-6.4.4] OK move delta=(%d,%d) capture=%p pane=%s managed=%p",
                  dx, dy,
                  static_cast<void*>(wxWindow::GetCapture()),
                  g_auiDragPaneName.utf8_str().data(),
                  static_cast<void*>(dt.auiManaged));
    }

    if ( action == kUp && g_auiDragActive )
    {
        const int afterW = PaneWidthByName(dt.auiMgr, g_auiDragPaneName);
        const int dx = ix - g_auiDragStartX;
        const int dy = iy - g_auiDragStartY;
        if ( afterW >= 0 && afterW != g_auiDragStartPaneWidth )
        {
            ProbeOnce("I-6.4.5", probed645,
                      "[I-6.4.5] OK layout updated pane=%s width %d→%d "
                      "delta=(%d,%d) layout updated=1",
                      g_auiDragPaneName.utf8_str().data(),
                      g_auiDragStartPaneWidth, afterW, dx, dy);
        }
        else
        {
            OH_LOG_INFO(LOG_APP,
                        "[I-6.4.5] drag end pane=%{public}s width=%{public}d→%{public}d delta=(%{public}d,%{public}d)",
                        g_auiDragPaneName.utf8_str().data(),
                        g_auiDragStartPaneWidth, afterW, dx, dy);
        }
        g_auiDragActive = false;
        g_forwardAllMoves = g_leftDown;
        g_auiDragStartPaneWidth = -1;
        g_auiDragPaneName.clear();
    }
#endif
}

void UpdateAuiDragStateAfterDispatch(const DispatchTarget& dt, int action)
{
#if wxUSE_AUI
    if ( action == kDown && dt.auiManaged
         && wxWindow::GetCapture() == dt.auiManaged )
    {
        static bool probed643 = false;
        ProbeOnce("I-6.4.3", probed643,
                  "[I-6.4.3] OK begin drag capture=%p managed=%p pane=%s",
                  static_cast<void*>(wxWindow::GetCapture()),
                  static_cast<void*>(dt.auiManaged),
                  dt.sash.paneName.empty()
                      ? g_auiDragPaneName.utf8_str().data()
                      : dt.sash.paneName.utf8_str().data());
    }
    else if ( action == kDown && dt.sash.hit && dt.auiManaged )
    {
        OH_LOG_INFO(LOG_APP,
                    "[I-6.4.3] post-dispatch capture=%{public}p managed=%{public}p "
                    "local=(%{public}d,%{public}d)",
                    static_cast<void*>(wxWindow::GetCapture()),
                    static_cast<void*>(dt.auiManaged),
                    dt.local.x, dt.local.y);
    }
    if ( action == kUp || action == kCancel )
    {
        if ( !wxWindow::GetCapture() )
        {
            g_auiDragActive = false;
            g_forwardAllMoves = g_leftDown;
        }
    }
#else
    wxUnusedVar(dt);
    wxUnusedVar(action);
#endif
}

wxPoint EventPointForTarget(wxWindow* target, int surfaceX, int surfaceY)
{
    if ( UsesSurfaceCoords(target) )
        return wxPoint(surfaceX, surfaceY);
    return target->ScreenToClient(wxPoint(surfaceX, surfaceY));
}

wxWindow* FindTargetAtSurfacePoint(wxWindow* tlw, int x, int y)
{
    // Force probe: print tlw class + bar existence + bar rect for the first
    // 5 calls so we can see exactly why the menubar check misses.
    static int s_ftspCalls = 0;
    if ( s_ftspCalls < 5 )
    {
        s_ftspCalls++;
        wxString tlwCls2 = (tlw && tlw->GetClassInfo()) ? tlw->GetClassInfo()->GetClassName() : wxT("null");
        wxFrame* f2 = wxDynamicCast(tlw, wxFrame);
        wxMenuBar* b2frame = f2 ? f2->GetMenuBar() : nullptr;
        wxMenuBar* b2cache = Fw2_GetMainMenuBarPtr();
        wxMenuBar* b2 = b2frame ? b2frame : b2cache;
        wxSize sz2;
        bool shown2 = false;
        if ( b2 ) { sz2 = b2->GetSize(); shown2 = b2->IsShown(); }
        OH_LOG_INFO(LOG_APP,
                    "[FTSP] call#%{public}d ev=(%{public}d,%{public}d) tlw=%{public}p cls=%{public}s isFrame=%{public}d barFrame=%{public}p barCache=%{public}p barSz=%{public}dx%{public}d shown=%{public}d",
                    s_ftspCalls, x, y, static_cast<void*>(tlw), tlwCls2.utf8_str().data(),
                    f2 ? 1 : 0, static_cast<void*>(b2frame), static_cast<void*>(b2cache),
                    sz2.x, sz2.y, shown2 ? 1 : 0);
    }

    // On OHOS the wxMenuBar is NOT stored inside wxFrame::mMenuBar (the OHOS
    // port keeps it in a host-side cache). So frame->GetMenuBar() returns
    // nullptr. We must use Fw2_GetMainMenuBarPtr() to retrieve the real
    // menubar that the host attached in R-4.
    wxMenuBar* bar = Fw2_GetMainMenuBarPtr();
    if ( bar )
    {
        const int barH = bar->GetSize().y > 0 ? bar->GetSize().y : 28;
        const int barW = bar->GetSize().x > 0 ? bar->GetSize().x : 8192;
        if ( bar->IsShown() && y >= 0 && y < barH && x >= 0 && x < barW )
            return bar;
    }

    // Fall back: maybe a frame has GetMenuBar() working (non-OHOS or older
    // build). Scan all TLWs for one that has a menubar via GetMenuBar().
    if ( !bar )
    {
        wxFrame* frame = wxDynamicCast(tlw, wxFrame);
        if ( !frame || !frame->GetMenuBar() )
        {
            for ( wxWindowList::compatibility_iterator node = wxTopLevelWindows.GetFirst();
                  node; node = node->GetNext() )
            {
                wxWindow* w = static_cast<wxWindow*>(node->GetData());
                wxFrame* f = wxDynamicCast(w, wxFrame);
                if ( f && f->GetMenuBar() )
                {
                    frame = f;
                    break;
                }
            }
        }
        if ( frame )
        {
            if ( wxMenuBar* bar2 = frame->GetMenuBar() )
            {
                const int barH = bar2->GetSize().y > 0 ? bar2->GetSize().y : 28;
                const int barW = bar2->GetSize().x > 0 ? bar2->GetSize().x : 8192;
                if ( bar2->IsShown() && y >= 0 && y < barH && x >= 0 && x < barW )
                    return bar2;
            }
#if wxUSE_TOOLBAR
            if ( wxToolBar* tbarPtr = frame->GetToolBar() )
            {
                wxWindow* tbar = reinterpret_cast<wxWindow*>(tbarPtr);
                if ( tbar->IsShown() )
                {
                    const int menuH = frame->GetMenuBar()
                        ? (frame->GetMenuBar()->GetSize().y > 0
                            ? frame->GetMenuBar()->GetSize().y : 28)
                        : 0;
                    const wxPoint tpt = tbar->GetPosition();
                    const wxSize tsz = tbar->GetSize();
                    const int top = tpt.y >= 0 ? tpt.y : menuH;
                    const int bottom = top + (tsz.y > 0 ? tsz.y : 32);
                    if ( y >= top && y < bottom && x >= 0 && x < tsz.x )
                        return tbar;
                }
            }
#endif
        }
    }

    wxWindow* hit = wxGenericFindWindowAtPoint(wxPoint(x, y));
    if ( hit )
        return hit;

    return tlw;
}

int MenuBarIndexAt(wxMenuBar* bar, const wxPoint& pt)
{
    int x = 10;
    const int barH = bar->GetSize().y > 0 ? bar->GetSize().y : 28;
    if ( pt.y < 0 || pt.y >= barH )
        return -1;

    for ( size_t i = 0; i < bar->GetMenuCount(); ++i )
    {
        wxString label = wxStripMenuCodes(bar->GetMenuLabel(i));
        if ( label.empty() )
            continue;
        int tw = int(label.length()) * 6;
        if ( tw <= 0 )
            tw = 40;
        if ( pt.x >= x && pt.x < x + tw + 18 )
            return static_cast<int>(i);
        x += tw + 18;
    }
    return -1;
}

int NormalizeButton(int button)
{
    if ( button == 0 || button == 0x01 )
        return 0;
    if ( button == 1 || button == 0x02 )
        return 1;
    if ( button == 2 || button == 0x04 )
        return 2;
    return 0;
}

void ProbeOnce(const char* tag, bool& flag, const char* fmt, ...)
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
    wxUnusedVar(tag);
}

wxEventType MapAction(int action, int button)
{
    switch ( action )
    {
        case kMove: return wxEVT_MOTION;
        case kDown:
            if ( button == 2 ) return wxEVT_MIDDLE_DOWN;
            if ( button == 1 ) return wxEVT_RIGHT_DOWN;
            return wxEVT_LEFT_DOWN;
        case kUp:
            if ( button == 2 ) return wxEVT_MIDDLE_UP;
            if ( button == 1 ) return wxEVT_RIGHT_UP;
            return wxEVT_LEFT_UP;
        case kCancel: return wxEVT_LEFT_UP;
        default: return wxEVT_NULL;
    }
}

void UpdateButtonState(int action, int button)
{
    const int btn = NormalizeButton(button);
    if ( action == kDown )
    {
        if ( btn == 0 ) g_leftDown = true;
        else if ( btn == 1 ) g_rightDown = true;
        else if ( btn == 2 ) g_middleDown = true;
    }
    else if ( action == kUp || action == kCancel )
    {
        if ( btn == 0 ) g_leftDown = false;
        else if ( btn == 1 ) g_rightDown = false;
        else if ( btn == 2 ) g_middleDown = false;
    }
}

void ApplyButtonState(wxMouseEvent& event)
{
    event.m_leftDown = g_leftDown;
    event.m_rightDown = g_rightDown;
    event.m_middleDown = g_middleDown;
}

void PresentIfNeeded(wxWindow* tlw)
{
    if ( tlw && tlw->IsTopLevel() )
        static_cast<wxTopLevelWindowOHOS*>(tlw)->PresentBackingStore();
}

bool HandleMenuBarUp(wxMenuBar* bar, int ix, int iy)
{
    static bool probedI5 = false;

    const wxPoint menuPt(ix, iy);
    const int menuIndex = MenuBarIndexAt(bar, menuPt);
    OH_LOG_INFO(LOG_APP,
                "[I-5] MenuBar up index=%{public}d pressIndex=%{public}d menuPt=(%{public}d,%{public}d)",
                menuIndex, g_menuBarPressIndex, menuPt.x, menuPt.y);

    const int openIndex = (menuIndex >= 0 && menuIndex == g_menuBarPressIndex)
        ? menuIndex
        : -1;
    g_menuBarPressIndex = -1;

    if ( openIndex < 0 )
        return false;

    wxMenu* menu = bar->GetMenu(static_cast<size_t>(openIndex));
    if ( !menu )
        return false;

    if ( g_activeMenuPopup )
    {
        g_activeMenuPopup->Dismiss();
        g_activeMenuPopup = nullptr;
    }
    wxFrame* frame = bar->GetFrame();
    const int barH = bar->GetSize().y > 0 ? bar->GetSize().y : 28;
    const wxPoint popupPos(menuPt.x, barH);
    g_activeMenuPopup = new Fw2MenuPopup(frame, menu, popupPos);
    OH_LOG_INFO(LOG_APP, "[I-5] OK popup opened index=%{public}d items=%{public}zu",
                openIndex, menu->GetMenuItems().GetCount());
    ProbeOnce("I-5", probedI5, "[I-5] OK menu popup opened items=%zu",
              menu->GetMenuItems().GetCount());
    return true;
}

bool HandleMenuBarDown(wxMenuBar* bar, int ix, int iy)
{
    const wxPoint menuPt(ix, iy);
    g_menuBarPressIndex = MenuBarIndexAt(bar, menuPt);
    OH_LOG_INFO(LOG_APP,
                "[I-5] MenuBar down index=%{public}d menuPt=(%{public}d,%{public}d) surface=(%{public}d,%{public}d)",
                g_menuBarPressIndex, menuPt.x, menuPt.y, ix, iy);
    return g_menuBarPressIndex >= 0;
}

void DispatchMouseEventToTarget(wxWindow* target, wxWindow* tlw, const wxMouseEvent& event,
                                int action, bool present)
{
    (void)target->HandleWindowEvent(const_cast<wxMouseEvent&>(event));
    if ( present && action != kMove )
    {
        tlw->Refresh(false);
        tlw->Update();
        PresentIfNeeded(tlw);
    }
}

#if wxUSE_AUI
bool DispatchAuiSashLeftDownProbe(wxWindow* target, wxWindow* tlw, wxMouseEvent& event,
                                  const DispatchTarget& dt)
{
    static bool probedBefore = false;
    static bool probedAfter = false;
    static bool probedCaptureApi = false;

    wxWindow* captureBefore = wxWindow::GetCapture();
    ProbeOnce("I-6.4.3.before", probedBefore,
              "[I-6.4.3.before] LeftDown managed=%p local=(%d,%d) snap=(%d,%d) "
              "captureBefore=%p pane=%s",
              static_cast<void*>(dt.auiManaged),
              dt.local.x, dt.local.y,
              event.m_x, event.m_y,
              static_cast<void*>(captureBefore),
              dt.sash.paneName.utf8_str().data());

    const bool handled = target->HandleWindowEvent(event);

    wxWindow* captureAfter = wxWindow::GetCapture();
    const bool hasCapture = dt.auiManaged && dt.auiManaged->HasCapture();
    ProbeOnce("I-6.4.3.after", probedAfter,
              "[I-6.4.3.after] handled=%d captureAfter=%p hasCapture=%d managed=%p",
              handled ? 1 : 0,
              static_cast<void*>(captureAfter),
              hasCapture ? 1 : 0,
              static_cast<void*>(dt.auiManaged));

    if ( captureAfter != dt.auiManaged && !probedCaptureApi && dt.auiManaged )
    {
        probedCaptureApi = true;
        wxWindow* apiBefore = wxWindow::GetCapture();
        dt.auiManaged->CaptureMouse();
        wxWindow* apiAfter = wxWindow::GetCapture();
        OH_LOG_INFO(LOG_APP,
                    "[I-6.4.3.capture-api] CaptureMouse test before=%{public}p after=%{public}p "
                    "managed=%{public}p ok=%{public}d",
                    static_cast<void*>(apiBefore),
                    static_cast<void*>(apiAfter),
                    static_cast<void*>(dt.auiManaged),
                    apiAfter == dt.auiManaged ? 1 : 0);
        if ( apiAfter == dt.auiManaged )
            dt.auiManaged->ReleaseMouse();
        else
            OH_LOG_ERROR(LOG_APP,
                         "[I-6.4.3] FAIL wx CaptureMouse/GetCapture (wxOHOS DoCaptureMouse no-op?)");
    }

    tlw->Refresh(false);
    tlw->Update();
    PresentIfNeeded(tlw);
    return handled;
}
#endif

void DispatchImpl(int action, int button, float x, float y)
{
    static bool probedI1 = false;
    static bool probedI2 = false;
    static bool probedI3 = false;
    static bool probedI4 = false;
    static bool probedI61 = false;
    static bool probedI62 = false;

    const int btn = NormalizeButton(button);
    const int ix = static_cast<int>(std::lround(x));
    const int iy = static_cast<int>(std::lround(y));
    g_lastMouseX = ix;
    g_lastMouseY = iy;

    ProbeOnce("I-1", probedI1, "[I-1] OK pointer surface=(%d,%d) action=%d btn=%d",
              ix, iy, action, btn);
    if ( action == kDown )
        ProbeOnce("I-2", probedI2, "[I-2] OK pointer down at (%d,%d) btn=%d", ix, iy, btn);
    if ( action == kUp )
        ProbeOnce("I-6.2", probedI62, "[I-6.2] OK pointer up at (%d,%d) btn=%d", ix, iy, btn);
    if ( action == kMove )
        ProbeOnce("I-6.1", probedI61, "[I-6.1] OK pointer move at (%d,%d) leftDown=%d",
                  ix, iy, g_leftDown ? 1 : 0);

    // Diagnostic for the classic "menu bar not clickable" symptom: if an event
    // lands in the top ~60 surface rows it is almost certainly meant for the
    // menubar. Print the real wxMenuBar geometry so we can tune AUTO-click
    // coordinates (and/or fix ResolveDispatchTarget).
    static bool probedMenuBar = false;
    if ( !probedMenuBar && iy < 120 )
    {
        wxMenuBar* bar = Fw2_GetMainMenuBarPtr();
        if ( !bar )
        {
            wxWindow* tlw2 = GetTopWindow();
            if ( tlw2 && tlw2->IsTopLevel() )
            {
                wxFrame* frame = wxDynamicCast(tlw2, wxFrame);
                bar = frame ? frame->GetMenuBar() : nullptr;
            }
        }
        if ( bar )
        {
            const wxRect br = bar->GetRect();
            const wxRect sr = bar->GetScreenRect();
            const wxPoint pt = bar->ClientToScreen(wxPoint(0, 0));
            const wxSize bsz = bar->GetSize();
            OH_LOG_INFO(LOG_APP,
                        "[MENUBAR-GEOM] event=(%{public}d,%{public}d) bar rect=%{public}d,%{public}d %{public}dx%{public}d screen=%{public}d,%{public}d %{public}dx%{public}d "
                        "C2S(0,0)=(%{public}d,%{public}d) barSize=%{public}dx%{public}d titles=%{public}d shown=%{public}d",
                        ix, iy, br.x, br.y, br.width, br.height,
                        sr.x, sr.y, sr.width, sr.height,
                        pt.x, pt.y, bsz.GetWidth(), bsz.GetHeight(),
                        (int)bar->GetMenuCount(), bar->IsShown() ? 1 : 0);
            probedMenuBar = true;
        }
        else
        {
            OH_LOG_INFO(LOG_APP, "[MENUBAR-GEOM] event=(%{public}d,%{public}d) FAIL: bar is NULL from both cache and frame",
                        ix, iy);
            probedMenuBar = true;
        }
    }

    wxWindow* tlw = GetTopWindow();
    if ( !tlw || !tlw->IsTopLevel() )
    {
        OH_LOG_ERROR(LOG_APP, "[I-3] FAIL dispatch: no TopWindow");
        return;
    }

    const wxEventType evType = MapAction(action, btn);
    if ( evType == wxEVT_NULL )
        return;

    DispatchTarget dt = ResolveDispatchTarget(tlw, ix, iy);
    wxWindow* target = dt.window ? dt.window : tlw;
    const wxPoint local = dt.local;

    ProbeAuiDock(action, dt, ix, iy);

    wxMouseEvent event(evType);
    event.SetEventObject(target);
    event.SetId(target->GetId());
    event.m_x = local.x;
    event.m_y = local.y;

#if wxUSE_AUI
    if ( action == kDown && dt.sash.hit && dt.auiMgr && target == dt.auiManaged )
    {
        const int half = AuiSashSize(dt.auiMgr) + 3;
        for ( const wxAuiPaneInfo& pane : dt.auiMgr->GetAllPanes() )
        {
            if ( pane.name != dt.sash.paneName || pane.rect.width <= 0 )
                continue;
            const wxRect& r = pane.rect;
            if ( pane.dock_direction == wxAUI_DOCK_LEFT )
                event.m_x = r.x + r.width + half / 2;
            else if ( pane.dock_direction == wxAUI_DOCK_RIGHT )
                event.m_x = r.x - half / 2;
            else if ( pane.dock_direction == wxAUI_DOCK_TOP )
                event.m_y = r.y + r.height + half / 2;
            else if ( pane.dock_direction == wxAUI_DOCK_BOTTOM )
                event.m_y = r.y - half / 2;
            break;
        }
    }
#endif

    if ( action == kDown )
    {
        if ( wxMenuBar* bar = wxDynamicCast(target, wxMenuBar) )
            (void)HandleMenuBarDown(bar, ix, iy);
    }

    bool menuPopupOpened = false;
    if ( action == kUp )
    {
        if ( wxMenuBar* bar = wxDynamicCast(target, wxMenuBar) )
            menuPopupOpened = HandleMenuBarUp(bar, ix, iy);
    }
    if ( action == kCancel )
        g_menuBarPressIndex = -1;

    UpdateButtonState(action, btn);
    ApplyButtonState(event);

    if ( action == kDown && target )
        g_hostKeyFocus = RefineKeyDispatchTarget(target, ix, iy);

    ProbeOnce("I-3", probedI3,
              "[I-3] OK wxMouseEvent type=%d local=(%d,%d) target=%p",
              evType, local.x, local.y, static_cast<void*>(target));

    wxString clsName(wxT("?"));
    if ( target->GetClassInfo() )
        clsName = target->GetClassInfo()->GetClassName();
    ProbeOnce("I-4", probedI4,
              "[I-4] OK hit target class=%s win=%p surface=(%d,%d)",
              clsName.utf8_str().data(), static_cast<void*>(target), ix, iy);

    if ( g_activeMenuPopup && action == kUp && !menuPopupOpened )
    {
        const wxPoint popupPos = g_activeMenuPopup->GetPosition();
        const wxSize popupSz = g_activeMenuPopup->GetSize();
        if ( ix >= popupPos.x && iy >= popupPos.y &&
             ix < popupPos.x + popupSz.GetWidth() &&
             iy < popupPos.y + popupSz.GetHeight() )
        {
            wxMouseEvent upEvent(wxEVT_LEFT_UP);
            upEvent.SetEventObject(g_activeMenuPopup);
            upEvent.SetId(g_activeMenuPopup->GetId());
            upEvent.m_x = ix - popupPos.x;
            upEvent.m_y = iy - popupPos.y;
            ApplyButtonState(upEvent);
            (void)g_activeMenuPopup->HandleWindowEvent(upEvent);
            FlushPendingMenuCommand();
            ProbeAuiDockPostDispatch(action, dt, ix, iy);
            UpdateAuiDragStateAfterDispatch(dt, action);
            tlw->Refresh(false);
            tlw->Update();
            PresentIfNeeded(tlw);
            g_forwardAllMoves = g_leftDown || g_auiDragActive;
            return;
        }
    }

#if wxUSE_AUI
    if ( action == kDown && dt.sash.hit && target == dt.auiManaged )
        (void)DispatchAuiSashLeftDownProbe(target, tlw, event, dt);
    else
#endif
        DispatchMouseEventToTarget(target, tlw, event, action, true);

    ProbeAuiDockPostDispatch(action, dt, ix, iy);
    UpdateAuiDragStateAfterDispatch(dt, action);

    if ( menuPopupOpened )
    {
        tlw->Refresh(false);
        tlw->Update();
        PresentIfNeeded(tlw);
    }

    g_forwardAllMoves = g_leftDown || g_auiDragActive;
}

void DispatchWheelImpl(int rotation, float x, float y)
{
    static bool probedI63 = false;

    const int ix = static_cast<int>(std::lround(x));
    const int iy = static_cast<int>(std::lround(y));

    ProbeOnce("I-6.3", probedI63,
              "[I-6.3] OK wheel rotation=%d at surface=(%d,%d)",
              rotation, ix, iy);

    wxWindow* tlw = GetTopWindow();
    if ( !tlw || !tlw->IsTopLevel() )
        return;

    wxWindow* target = FindTargetAtSurfacePoint(tlw, ix, iy);
    if ( !target )
        target = tlw;

    const wxPoint local = EventPointForTarget(target, ix, iy);

    wxMouseEvent event(wxEVT_MOUSEWHEEL);
    event.SetEventObject(target);
    event.SetId(target->GetId());
    event.m_x = local.x;
    event.m_y = local.y;
    event.m_wheelRotation = rotation;
    event.m_wheelDelta = 120;
    event.m_linesPerAction = 3;
    ApplyButtonState(event);

    (void)target->HandleWindowEvent(event);
    tlw->Refresh(false);
    tlw->Update();
    PresentIfNeeded(tlw);
}

} // namespace

bool Fw2_ShouldForwardAllMoves()
{
    return g_forwardAllMoves || g_auiDragActive;
}

bool Fw2_IsAuiDragActive()
{
    return g_auiDragActive;
}

void Fw2_DispatchPointer(int action, int button, float x, float y)
{
    // Hardcoded entry probe (always fires, regardless of wxTheApp state). If
    // this line appears in hilog but the I-1..I-5 probes inside DispatchImpl
    // never do, either wxTheApp is null, CallAfter is broken on this port, or
    // we failed the TopWindow check.
    const int ix = static_cast<int>(std::lround(x));
    const int iy = static_cast<int>(std::lround(y));
    OH_LOG_INFO(LOG_APP,
                "[I-1.ENTRY] Fw2_DispatchPointer ENTER action=%d button=%d xyz=(%d,%d) wxTheApp=%p",
                action, button, ix, iy, static_cast<void*>(wxTheApp));

    if ( !wxTheApp )
    {
        OH_LOG_ERROR(LOG_APP, "[I-1.ENTRY] FAIL: wxTheApp is NULL (drop event)");
        return;
    }
    OH_LOG_INFO(LOG_APP, "[I-1.CHECK] wxTheApp OK, ix=%d iy=%d", ix, iy);

    TryBeginAuiDragEarly(action, ix, iy);
    UpdateHostKeyFocusSync(action, ix, iy);

    // Synchronous dispatch only (pid=4399 v4 sweep, 23 clicks → zero I-1.CA
    // CallAfter lambda executed). Main UI thread = ArkTS NAPI callback = wx
    // event loop thread, so sync dispatch on the current thread is correct.
    OH_LOG_INFO(LOG_APP, "[I-1.SYNC] pre-DispatchImpl ix=%d iy=%d", ix, iy);
    DispatchImpl(action, button, x, y);
    OH_LOG_INFO(LOG_APP, "[I-1.DONE] post-DispatchImpl ix=%d iy=%d", ix, iy);

    wxEventLoopBase* loop = wxEventLoopBase::GetActive();
    OH_LOG_INFO(LOG_APP,
                "[I-1.ENTRY] POST: loop=%p",
                static_cast<void*>(loop));
    if ( loop )
        loop->WakeUp();
}

void Fw2_DispatchWheel(int rotation, float x, float y)
{
    if ( !wxTheApp )
        return;

    wxTheApp->CallAfter([rotation, x, y]() {
        DispatchWheelImpl(rotation, x, y);
    });

    if ( wxEventLoopBase* loop = wxEventLoopBase::GetActive() )
        loop->WakeUp();
}

const char* OhosKeyActionName(int action)
{
    switch ( action )
    {
        case 0: return "DOWN";  // OH_NATIVEXCOMPONENT_KEY_ACTION_DOWN
        case 1: return "UP";    // OH_NATIVEXCOMPONENT_KEY_ACTION_UP
        default: return "OTHER";
    }
}

struct WxKeyTranslation
{
    int wxKeyCode = 0;
    wchar_t unicode = 0;
    int modifiers = 0;
    bool mapped = false;
};

int CompactOhosModifiers(uint64_t ohosMods)
{
    int m = 0;
    if ( ohosMods & (1ULL << 0) )
        m |= 2; // ctrl
    if ( ohosMods & (1ULL << 1) )
        m |= 1; // shift
    if ( ohosMods & (1ULL << 2) )
        m |= 4; // alt
    return m;
}

WxKeyTranslation TranslateOhosKey(int ohosCode, uint64_t ohosMods)
{
    WxKeyTranslation t;
    t.modifiers = CompactOhosModifiers(ohosMods);
    const bool shift = (t.modifiers & 1) != 0;

    if ( ohosCode >= KEY_A && ohosCode <= KEY_Z )
    {
        const char upper = static_cast<char>('A' + (ohosCode - KEY_A));
        t.wxKeyCode = upper;
        t.unicode = shift ? upper : static_cast<wchar_t>(upper - 'A' + 'a');
        t.mapped = true;
        return t;
    }

    if ( ohosCode >= KEY_0 && ohosCode <= KEY_9 )
    {
        t.wxKeyCode = static_cast<int>('0' + (ohosCode - KEY_0));
        t.unicode = static_cast<wchar_t>(t.wxKeyCode);
        t.mapped = true;
        return t;
    }

    switch ( ohosCode )
    {
        case KEY_ENTER:
            t.wxKeyCode = WXK_RETURN;
            t.mapped = true;
            break;
        case KEY_TAB:
            t.wxKeyCode = WXK_TAB;
            t.unicode = L'\t';
            t.mapped = true;
            break;
        case KEY_DEL:
            t.wxKeyCode = WXK_BACK;
            t.mapped = true;
            break;
        case KEY_DPAD_LEFT:
            t.wxKeyCode = WXK_LEFT;
            t.mapped = true;
            break;
        case KEY_DPAD_RIGHT:
            t.wxKeyCode = WXK_RIGHT;
            t.mapped = true;
            break;
        case KEY_DPAD_UP:
            t.wxKeyCode = WXK_UP;
            t.mapped = true;
            break;
        case KEY_DPAD_DOWN:
            t.wxKeyCode = WXK_DOWN;
            t.mapped = true;
            break;
        case KEY_SPACE:
            t.wxKeyCode = WXK_SPACE;
            t.unicode = L' ';
            t.mapped = true;
            break;
        case KEY_ESCAPE:
            t.wxKeyCode = WXK_ESCAPE;
            t.mapped = true;
            break;
        case KEY_SHIFT_LEFT:
        case KEY_SHIFT_RIGHT:
            t.wxKeyCode = WXK_SHIFT;
            t.mapped = true;
            break;
        case KEY_CTRL_LEFT:
        case KEY_CTRL_RIGHT:
            t.wxKeyCode = WXK_CONTROL;
            t.mapped = true;
            break;
        case KEY_ALT_LEFT:
        case KEY_ALT_RIGHT:
            t.wxKeyCode = WXK_ALT;
            t.mapped = true;
            break;
        default:
            break;
    }

    return t;
}

char UnicodeProbeChar(wchar_t ch)
{
    if ( ch >= 32 && ch < 127 )
        return static_cast<char>(ch);
    return '.';
}

void ProbeI72Acceptance(int ohosCode, const WxKeyTranslation& t)
{
    static bool probedA = false;
    static bool probedEnter = false;
    static bool probedBack = false;
    static bool probedCtrl = false;

    if ( ohosCode == KEY_A && t.wxKeyCode == 'A' && !probedA )
    {
        probedA = true;
        OH_LOG_INFO(LOG_APP,
                    "[I-7.2] OK A → WXK_A wxCode=%{public}d unicode='%{public}c'",
                    t.wxKeyCode, UnicodeProbeChar(t.unicode));
    }
    else if ( ohosCode == KEY_ENTER && t.wxKeyCode == WXK_RETURN && !probedEnter )
    {
        probedEnter = true;
        OH_LOG_INFO(LOG_APP, "[I-7.2] OK Enter → RETURN wxCode=%{public}d", t.wxKeyCode);
    }
    else if ( ohosCode == KEY_DEL && t.wxKeyCode == WXK_BACK && !probedBack )
    {
        probedBack = true;
        OH_LOG_INFO(LOG_APP, "[I-7.2] OK Backspace → BACK wxCode=%{public}d", t.wxKeyCode);
    }
    else if ( (ohosCode == KEY_CTRL_LEFT || ohosCode == KEY_CTRL_RIGHT)
              && t.wxKeyCode == WXK_CONTROL && !probedCtrl )
    {
        probedCtrl = true;
        OH_LOG_INFO(LOG_APP,
                    "[I-7.2] OK Ctrl → CONTROL wxCode=%{public}d modifiers=%{public}d",
                    t.wxKeyCode, t.modifiers);
    }
}

void ApplyCompactModifiers(wxKeyEvent& event, int mods)
{
    event.m_shiftDown = (mods & 1) != 0;
    event.m_controlDown = (mods & 2) != 0;
    event.m_altDown = (mods & 4) != 0;
    event.m_metaDown = false;
}

static bool g_i74CharSeenThisKey = false;

wxStyledTextCtrl* AsStyledTextCtrl(wxWindow* target)
{
    return wxDynamicCast(target, wxStyledTextCtrl);
}

void LogI741CharEvent(wxWindow* target, const WxKeyTranslation& t)
{
    static bool probed741 = false;
    if ( probed741 )
        return;
    probed741 = true;
    const wxString cls = target && target->GetClassInfo()
        ? wxString(target->GetClassInfo()->GetClassName())
        : wxString(wxT("?"));
    OH_LOG_INFO(LOG_APP,
                "[I-7.4.1] OK wxEVT_CHAR unicode='%{public}c' target=%{public}p "
                "class=%{public}s",
                UnicodeProbeChar(t.unicode),
                static_cast<void*>(target), cls.utf8_str().data());
}

void ProbeI74LengthAndRepaint(wxStyledTextCtrl* stc, int lenBefore)
{
    if ( !stc )
        return;

    static bool probed742 = false;
    static bool probed743 = false;

    const int lenAfter = stc->GetLength();
    if ( !probed742 )
    {
        probed742 = true;
        OH_LOG_INFO(LOG_APP,
                    "[I-7.4.2] OK before length=%{public}d after length=%{public}d",
                    lenBefore, lenAfter);
    }

    if ( lenAfter > lenBefore && !probed743 )
    {
        probed743 = true;
        stc->Refresh(false);
        stc->Update();
        if ( wxWindow* tlw = GetTopWindow() )
            PresentIfNeeded(tlw);
        OH_LOG_INFO(LOG_APP, "[I-7.4.3] OK Editor repaint dirty");
    }
}

void DispatchCharIfNeeded(wxWindow* target, const WxKeyTranslation& t, int lenBefore)
{
    wxStyledTextCtrl* stc = AsStyledTextCtrl(target);
    if ( !stc || !t.unicode || t.unicode < 32 )
        return;
    if ( stc->GetLength() != lenBefore )
    {
        g_i74CharSeenThisKey = true;
        LogI741CharEvent(target, t);
        return;
    }

    OH_LOG_INFO(LOG_APP,
                "[I-7.4.0] no natural wxEVT_CHAR after KEY_DOWN; host wxEVT_CHAR");

    wxKeyEvent charEvent(wxEVT_CHAR);
    charEvent.SetEventObject(target);
    charEvent.SetId(target->GetId());
    charEvent.m_keyCode = static_cast<int>(t.unicode);
    charEvent.m_uniChar = t.unicode;
    ApplyCompactModifiers(charEvent, t.modifiers);
    charEvent.SetTimestamp(static_cast<long>(wxGetLocalTimeMillis().GetValue()));
    LogI741CharEvent(target, t);

    if ( stc->GetReadOnly() )
    {
        static bool probedReadOnly = false;
        stc->SetReadOnly(false);
        if ( !probedReadOnly )
        {
            probedReadOnly = true;
            OH_LOG_INFO(LOG_APP,
                        "[I-7.4.fix] cleared readOnly on target STC (host insert bring-up)");
        }
    }

    if ( !target->HandleWindowEvent(charEvent) )
        (void)target->GetEventHandler()->ProcessEvent(charEvent);

    if ( wxTheApp )
        wxTheApp->ProcessPendingEvents();
}

wxWindow* ResolveKeyDispatchTarget()
{
    wxWindow* candidate = nullptr;
    if ( g_hostKeyFocus )
        candidate = g_hostKeyFocus;
    else if ( wxWindow* focus = wxWindow::FindFocus() )
        candidate = focus;
    else if ( wxWindow* tlw = GetTopWindow() )
        candidate = FindKeyRecipientAtSurfacePoint(tlw, g_lastMouseX, g_lastMouseY);

    if ( !candidate )
        return nullptr;
    return RefineKeyDispatchTarget(candidate, g_lastMouseX, g_lastMouseY);
}

void DispatchKeyImpl(int action, int ohosCode, uint64_t modifierKeys)
{
    static bool probed731 = false;
    static bool probed732 = false;
    static bool probed733 = false;

    if ( action != 0 )
        return;

    const WxKeyTranslation t = TranslateOhosKey(ohosCode, modifierKeys);
    if ( !t.mapped )
        return;

    wxWindow* wxFocus = wxWindow::FindFocus();
    wxWindow* target = ResolveKeyDispatchTarget();
    if ( !target )
    {
        if ( !probed731 )
        {
            probed731 = true;
            OH_LOG_INFO(LOG_APP, "[I-7.3.1] focus=null");
        }
        return;
    }

    if ( !probed731 )
    {
        probed731 = true;
        const wxString focusCls = wxFocus && wxFocus->GetClassInfo()
            ? wxString(wxFocus->GetClassInfo()->GetClassName())
            : wxString(wxT("?"));
        const wxString targetCls = target->GetClassInfo()
            ? wxString(target->GetClassInfo()->GetClassName())
            : wxString(wxT("?"));
        if ( wxFocus )
        {
            OH_LOG_INFO(LOG_APP,
                        "[I-7.3.1] OK focus window=%{public}p class=%{public}s",
                        static_cast<void*>(wxFocus), focusCls.utf8_str().data());
        }
        else if ( g_hostKeyFocus )
        {
            OH_LOG_INFO(LOG_APP,
                        "[I-7.3.1] OK focus window=%{public}p class=%{public}s "
                        "(hostTarget=%{public}p)",
                        static_cast<void*>(target), targetCls.utf8_str().data(),
                        static_cast<void*>(g_hostKeyFocus));
        }
        else
        {
            OH_LOG_INFO(LOG_APP,
                        "[I-7.3.1] focus=null hostTarget=%{public}p class=%{public}s",
                        static_cast<void*>(target), targetCls.utf8_str().data());
        }
    }

    wxKeyEvent event(wxEVT_KEY_DOWN);
    event.SetEventObject(target);
    event.SetId(target->GetId());
    event.m_keyCode = t.wxKeyCode;
    if ( t.unicode )
        event.m_uniChar = t.unicode;
    ApplyCompactModifiers(event, t.modifiers);
    event.SetTimestamp(static_cast<long>(wxGetLocalTimeMillis().GetValue()));

    if ( !probed732 )
    {
        probed732 = true;
        const wxString targetCls = target->GetClassInfo()
            ? wxString(target->GetClassInfo()->GetClassName())
            : wxString(wxT("?"));
        OH_LOG_INFO(LOG_APP,
                    "[I-7.3.2] OK wxEVT_KEY_DOWN code=%{public}d unicode='%{public}c' "
                    "target=%{public}p class=%{public}s",
                    t.wxKeyCode, UnicodeProbeChar(t.unicode),
                    static_cast<void*>(target), targetCls.utf8_str().data());
    }

    wxStyledTextCtrl* stc = AsStyledTextCtrl(target);
    const int lenBefore = stc ? stc->GetLength() : -1;
    g_i74CharSeenThisKey = false;

    const bool handled = target->HandleWindowEvent(event);

    if ( stc && t.unicode >= 32 && t.unicode < 127 )
        DispatchCharIfNeeded(target, t, lenBefore);

    if ( stc && lenBefore >= 0 )
        ProbeI74LengthAndRepaint(stc, lenBefore);

    if ( !probed733 )
    {
        probed733 = true;
        const wxString targetCls = target->GetClassInfo()
            ? wxString(target->GetClassInfo()->GetClassName())
            : wxString(wxT("?"));
        OH_LOG_INFO(LOG_APP,
                    "[I-7.3.3] OK ProcessEvent handled=%{public}d target=%{public}p "
                    "class=%{public}s",
                    handled ? 1 : 0, static_cast<void*>(target),
                    targetCls.utf8_str().data());
    }
}

void Fw2_DispatchKey(int action, int keyCode, uint64_t modifierKeys)
{
    static bool probedI71Down = false;
    static bool probedI71Up = false;

    const char* act = OhosKeyActionName(action);
    OH_LOG_INFO(LOG_APP,
                "[I-7.1] OHOS key received code=%{public}d action=%{public}s",
                keyCode, act);

    if ( action == 0 && !probedI71Down )
    {
        probedI71Down = true;
        OH_LOG_INFO(LOG_APP, "[I-7.1] OK key down code=%{public}d", keyCode);
        std::fprintf(stderr, "[I-7.1] OK key down code=%d\n", keyCode);
        std::fflush(stderr);
    }
    else if ( action == 1 && !probedI71Up )
    {
        probedI71Up = true;
        OH_LOG_INFO(LOG_APP, "[I-7.1] OK key up code=%{public}d", keyCode);
        std::fprintf(stderr, "[I-7.1] OK key up code=%d\n", keyCode);
        std::fflush(stderr);
    }

    if ( action != 0 )
        return;

    const WxKeyTranslation t = TranslateOhosKey(keyCode, modifierKeys);
    if ( !t.mapped )
    {
        OH_LOG_INFO(LOG_APP,
                    "[I-7.2] ohosCode=%{public}d wxCode=0 unicode='.' modifiers=%{public}d (unmapped)",
                    keyCode, t.modifiers);
        return;
    }

    const char u = UnicodeProbeChar(t.unicode);
    OH_LOG_INFO(LOG_APP,
                "[I-7.2] ohosCode=%{public}d wxCode=%{public}d unicode='%{public}c' modifiers=%{public}d",
                keyCode, t.wxKeyCode, u, t.modifiers);
    std::fprintf(stderr,
                 "[I-7.2] ohosCode=%d wxCode=%d unicode='%c' modifiers=%d\n",
                 keyCode, t.wxKeyCode, u, t.modifiers);
    std::fflush(stderr);

    ProbeI72Acceptance(keyCode, t);

    if ( wxTheApp )
    {
        wxTheApp->CallAfter([action, keyCode, modifierKeys]() {
            DispatchKeyImpl(action, keyCode, modifierKeys);
        });
        if ( wxEventLoopBase* loop = wxEventLoopBase::GetActive() )
            loop->WakeUp();
    }
}
