/////////////////////////////////////////////////////////////////////////////
// Minimal wxMenu / wxMenuBar for OHOS — R-4 painted MenuBar
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/menu.h"
    #include "wx/menuitem.h"
    #include "wx/frame.h"
    #include "wx/dcclient.h"
    #include "wx/settings.h"
    #include "wx/utils.h"
    #include "wx/toplevel.h"
#endif

#include "wx/ohos/dc.h"
#include "wx/ohos/toplevel.h"

#include <algorithm>
#include <hilog/log.h>

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xF004
#define LOG_TAG "wxOHOS"

// RTTI for wxMenu/wxMenuBar lives in menucmn.cpp (XTI).

wxBEGIN_EVENT_TABLE(wxMenuBar, wxMenuBarBase)
    EVT_PAINT(wxMenuBar::OnPaint)
    EVT_LEFT_DOWN(wxMenuBar::OnLeftDown)
    EVT_LEFT_UP(wxMenuBar::OnLeftUp)
    EVT_MOTION(wxMenuBar::OnMouseMove)
wxEND_EVENT_TABLE()

#if wxUSE_MENUS
namespace {

class wxOhosMenuPopup;
wxOhosMenuPopup* g_activeMenuPopup = nullptr;

class wxOhosMenuPopup : public wxWindow
{
public:
    wxOhosMenuPopup(wxMenuBar* bar, wxMenu* menu, const wxPoint& posScreen)
        : m_bar(bar), m_menu(menu)
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

        const wxSize popupSize(maxW, rows * rowH + 8);
        Create(bar->GetFrame(), wxID_ANY, posScreen, popupSize, wxBORDER_SIMPLE);
        SetBackgroundColour(*wxWHITE);
        Bind(wxEVT_PAINT, &wxOhosMenuPopup::OnPaint, this);
        Bind(wxEVT_LEFT_UP, &wxOhosMenuPopup::OnLeftUp, this);
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
            const wxString label = wxStripMenuCodes(item->GetItemLabel());
            dc.DrawText(label, 8, y);
            y += rowH;
        }
    }

    void OnLeftUp(wxMouseEvent& event)
    {
        const int rowH = 22;
        const int row = (event.GetY() - 4) / rowH;
        int idx = 0;
        for ( wxMenuItemList::compatibility_iterator node = m_menu->GetMenuItems().GetFirst();
              node;
              node = node->GetNext() )
        {
            wxMenuItem* item = node->GetData();
            if ( !item || item->IsSeparator() )
                continue;
            if ( idx == row )
            {
                OH_LOG_INFO(LOG_APP, "[I-5] menu item pick id=%{public}d label=%{public}s",
                            item->GetId(),
                            item->GetItemLabel().utf8_str().data());
                break;
            }
            ++idx;
        }
        Dismiss();
        g_activeMenuPopup = nullptr;
    }

    wxMenuBar* m_bar;
    wxMenu* m_menu;
};

} // namespace
#endif // wxUSE_MENUS

wxMenu::wxMenu(long style) : wxMenuBase(style) {}
wxMenu::wxMenu(const wxString& title, long style) : wxMenuBase(title, style) {}

wxMenuItem *wxMenu::DoAppend(wxMenuItem *item)
{
    return wxMenuBase::DoAppend(item);
}

wxMenuItem *wxMenu::DoInsert(size_t pos, wxMenuItem *item)
{
    return wxMenuBase::DoInsert(pos, item);
}

wxMenuItem *wxMenu::DoRemove(wxMenuItem *item)
{
    return wxMenuBase::DoRemove(item);
}

wxMenuBar::wxMenuBar() = default;
wxMenuBar::wxMenuBar(long style) : wxMenuBarBase()
{
    SetWindowStyle(style);
}

wxMenuBar::wxMenuBar(size_t n, wxMenu *menus[], const wxString titles[], long style)
{
    SetWindowStyle(style);
    for ( size_t i = 0; i < n; ++i )
        Append(menus[i], titles[i]);
}

bool wxMenuBar::Append(wxMenu *menu, const wxString& title)
{
#ifdef __WXOHOS__
    OH_LOG_INFO(LOG_APP,
                "[FUI_MENU] MenuBar append menu title=%{public}s menu=%{public}p",
                title.utf8_str().data(),
                menu);
#endif
    if ( !wxMenuBarBase::Append(menu, title) )
        return false;

    m_titles.push_back(title);
    if ( GetParent() )
        Refresh();
    return true;
}

bool wxMenuBar::Insert(size_t pos, wxMenu *menu, const wxString& title)
{
    if ( !wxMenuBarBase::Insert(pos, menu, title) )
        return false;

    m_titles.Insert(title, pos);
    if ( GetParent() )
        Refresh();
    return true;
}

wxMenu *wxMenuBar::Remove(size_t pos)
{
    wxMenu *menu = wxMenuBarBase::Remove(pos);
    if ( menu )
        m_titles.RemoveAt(pos);
    if ( GetParent() )
        Refresh();
    return menu;
}

void wxMenuBar::EnableTop(size_t WXUNUSED(pos), bool WXUNUSED(enable)) {}
bool wxMenuBar::IsEnabledTop(size_t WXUNUSED(pos)) const { return true; }

void wxMenuBar::SetMenuLabel(size_t pos, const wxString& label)
{
    wxCHECK_RET( pos < m_titles.size(), wxT("invalid index in SetMenuLabel") );
    m_titles[pos] = label;
    if ( GetParent() )
        Refresh();
}

wxString wxMenuBar::GetMenuLabel(size_t pos) const
{
    wxCHECK_MSG( pos < m_titles.size(), wxEmptyString, wxT("invalid index in GetMenuLabel") );
    return m_titles[pos];
}

void wxMenuBar::Attach(wxFrame *frame)
{
    wxCHECK_RET( frame, wxT("wxMenuBar::Attach(nullptr)") );

    wxMenuBarBase::Attach(frame);

    // First attach: create real child window (was data-only stub → invisible).
    if ( GetSize().y <= 0 )
    {
        int fw = 0, fh = 0;
        frame->GetSize(&fw, &fh);
        if ( fw <= 0 )
            fw = 800;
        if ( !Create(frame, wxID_ANY, wxDefaultPosition,
                     wxSize(fw, kMenuBarHeight),
                     wxBORDER_NONE, wxS("menuBar")) )
        {
            OH_LOG_ERROR(LOG_APP, "[R-4] FAIL MenuBar Create");
            return;
        }
        SetBackgroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_MENUBAR));
        SetForegroundColour(wxSystemSettings::GetColour(wxSYS_COLOUR_MENUTEXT));
        OH_LOG_INFO(LOG_APP,
                    "[R-4] MenuBar Create ok titles=%{public}zu h=%{public}d",
                    m_titles.size(), kMenuBarHeight);
    }
    else
    {
        Reparent(frame);
        Show(true);
    }

    // Univ-style: sit above client origin (negative y vs client).
    // wxSIZE_NO_ADJUSTMENTS: clMainFrame vtable GetClientAreaOrigin dispatch is
    // broken — parent origin adjustment SIGSEGV in GetToolBar/GetStatusBar (F-UI-3.4).
    int fw = frame->GetSize().x;
    if ( fw <= 0 )
        fw = 800;
    SetSize(0, -kMenuBarHeight, fw, kMenuBarHeight,
            wxSIZE_ALLOW_MINUS_ONE | wxSIZE_NO_ADJUSTMENTS);
    Show(true);
    // Queue paint only — Update before paintDelivery clears/skips and used
    // to drop the dirty region. TLW Show flushes shown children.
    Refresh(true);

    OH_LOG_INFO(LOG_APP,
                "[R-4] MenuBar Attach frame=%{public}p size=%{public}dx%{public}d "
                "titles=%{public}zu",
                static_cast<void*>(frame), GetSize().x, GetSize().y,
                m_titles.size());
}

void wxMenuBar::Detach()
{
    Hide();
    wxMenuBarBase::Detach();
    OH_LOG_INFO(LOG_APP, "[R-4] MenuBar Detach");
}

void wxMenuBar::OnPaint(wxPaintEvent& WXUNUSED(event))
{
    wxPaintDC dc(this);
    auto* impl = dynamic_cast<wxOhosDCImpl*>(dc.GetImpl());
    const bool hasPixels = impl && impl->GetPixelBuffer();
    const wxSize sz = GetClientSize();
    const wxColour bg = GetBackgroundColour().IsOk()
                            ? GetBackgroundColour()
                            : wxSystemSettings::GetColour(wxSYS_COLOUR_MENUBAR);

    dc.SetBackground(wxBrush(bg));
    dc.Clear();
    dc.SetPen(wxPen(wxSystemSettings::GetColour(wxSYS_COLOUR_3DSHADOW)));
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    if ( sz.x > 0 && sz.y > 0 )
        dc.DrawLine(0, sz.y - 1, sz.x, sz.y - 1);

    dc.SetTextForeground(*wxBLACK);
    int x = 10;
    const int y = 8;
    size_t painted = 0;
    for ( size_t i = 0; i < m_titles.size(); ++i )
    {
        wxString label = wxStripMenuCodes(m_titles[i]);
        if ( label.empty() )
            continue;
        dc.DrawText(label, x, y);
        int tw = 0, th = 0;
        dc.GetTextExtent(label, &tw, &th);
        if ( tw <= 0 )
            tw = int(label.length()) * 6;
        x += tw + 18;
        ++painted;
    }

    // Dark ink in top strip ⇒ glyphs reached TLW backing (not just log OK).
    size_t ink = 0;
    wxWindow* tlwWin = this;
    while ( tlwWin && !tlwWin->IsTopLevel() )
        tlwWin = tlwWin->GetParent();
    if ( tlwWin && tlwWin->IsTopLevel() )
    {
        auto* tlw = static_cast<wxTopLevelWindowOHOS*>(tlwWin);
        wxBitmap* bmp = tlw->GetBackingBitmap();
        if ( bmp && bmp->GetOhosPixels() )
        {
            const unsigned char* p = bmp->GetOhosPixels();
            const int stride = bmp->GetOhosStride();
            const int bw = bmp->GetWidth();
            const int rows = std::min(28, bmp->GetHeight());
            for ( int row = 0; row < rows; ++row )
            {
                const unsigned char* line = p + static_cast<size_t>(row) * stride;
                for ( int col = 0; col < std::min(bw, 600); col += 2 )
                {
                    const unsigned char r = line[col * 4 + 0];
                    const unsigned char g = line[col * 4 + 1];
                    const unsigned char b = line[col * 4 + 2];
                    if ( int(r) + int(g) + int(b) < 400 )
                        ++ink;
                }
            }
        }
    }

    OH_LOG_INFO(LOG_APP,
                "[R-4] MenuBar Paint titles=%{public}zu painted=%{public}zu "
                "size=%{public}dx%{public}d hasPixels=%{public}d ink=%{public}zu",
                m_titles.size(), painted, sz.x, sz.y, hasPixels ? 1 : 0, ink);
    if ( painted > 0 && hasPixels && ink > 16 )
        OH_LOG_INFO(LOG_APP, "[R-4] OK MenuBar visible path (Paint+DrawText)");
    else if ( !hasPixels )
        OH_LOG_ERROR(LOG_APP, "[R-4] FAIL MenuBar PaintDC not bound to backing");
    else
        OH_LOG_ERROR(LOG_APP, "[R-4] FAIL MenuBar ink not in TLW backing");
}

int wxMenuBar::GetMenuFromPoint(const wxPoint& pt) const
{
    int x = 10;
    for ( size_t i = 0; i < m_titles.size(); ++i )
    {
        wxString label = wxStripMenuCodes(m_titles[i]);
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

void wxMenuBar::OnLeftDown(wxMouseEvent& event)
{
    const int item = GetMenuFromPoint(event.GetPosition());
    OH_LOG_INFO(LOG_APP, "[I-5] MenuBar LeftDown pos=(%{public}d,%{public}d) menuIndex=%{public}d",
                event.GetX(), event.GetY(), item);
    if ( item < 0 )
        return;

    wxMenu* menu = GetMenu(static_cast<size_t>(item));
    if ( !menu )
        return;

    if ( g_activeMenuPopup )
    {
        g_activeMenuPopup->Dismiss();
        g_activeMenuPopup = nullptr;
    }

    const wxPoint posScreen = ClientToScreen(wxPoint(event.GetX(), GetSize().y));
    g_activeMenuPopup = new wxOhosMenuPopup(this, menu, posScreen);
    OH_LOG_INFO(LOG_APP, "[I-5] OK MenuBar popup menu=%{public}s items=%{public}zu",
                GetMenuLabel(static_cast<size_t>(item)).utf8_str().data(),
                menu->GetMenuItems().GetCount());
    if ( wxWindow* tlw = GetFrame() )
    {
        tlw->Update();
        if ( tlw->IsTopLevel() )
            static_cast<wxTopLevelWindowOHOS*>(tlw)->PresentBackingStore();
    }
}

void wxMenuBar::OnLeftUp(wxMouseEvent& event)
{
    event.Skip();
}

void wxMenuBar::OnMouseMove(wxMouseEvent& event)
{
    event.Skip();
}
