/////////////////////////////////////////////////////////////////////////////
// Minimal wxCursor for OHOS (no-op)
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/cursor.h"

#ifndef WX_PRECOMP
    #include "wx/bitmap.h"
#endif

#if wxUSE_IMAGE
    #include "wx/image.h"
#endif

class wxCursorRefData : public wxGDIRefData
{
public:
    wxCursorRefData() = default;
    wxCursorRefData(const wxCursorRefData& data)
        : wxGDIRefData(), m_stock(data.m_stock) {}

    wxStockCursor m_stock = wxCURSOR_NONE;
};

wxIMPLEMENT_DYNAMIC_CLASS(wxCursor, wxGDIObject);

void wxCursor::InitFromStock(wxStockCursor id)
{
    m_refData = new wxCursorRefData;
    static_cast<wxCursorRefData*>(m_refData)->m_stock = id;
}

wxCursor::wxCursor(const wxBitmap& WXUNUSED(bitmap), int WXUNUSED(hotSpotX), int WXUNUSED(hotSpotY))
{
    m_refData = new wxCursorRefData;
}
#if wxUSE_IMAGE
wxCursor::wxCursor(const wxImage& WXUNUSED(image))
{
    m_refData = new wxCursorRefData;
}
wxCursor::wxCursor(const char* const* WXUNUSED(xpmData))
{
    m_refData = new wxCursorRefData;
}
#endif
wxCursor::wxCursor(const wxString& WXUNUSED(name), wxBitmapType WXUNUSED(type),
                   int WXUNUSED(hotSpotX), int WXUNUSED(hotSpotY))
{
    m_refData = new wxCursorRefData;
}

wxBitmap wxCursor::GetBitmap() const { return wxNullBitmap; }

wxGDIRefData *wxCursor::CreateGDIRefData() const { return new wxCursorRefData; }
wxGDIRefData *wxCursor::CloneGDIRefData(const wxGDIRefData *data) const
{
    return new wxCursorRefData(*static_cast<const wxCursorRefData*>(data));
}
