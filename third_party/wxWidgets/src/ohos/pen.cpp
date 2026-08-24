/////////////////////////////////////////////////////////////////////////////
// Name:        src/ohos/pen.cpp
// Purpose:     wxPen class implementation
// Author:      Vaclav Slavik
// Created:     2006-08-04
// Copyright:   (c) 2006 REA Elektronik GmbH
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

// For compilers that support precompilation, includes "wx.h".
#include "wx/wxprec.h"


#include "wx/pen.h"

#ifndef WX_PRECOMP
    #include "wx/bitmap.h"
    #include "wx/colour.h"
#endif

//-----------------------------------------------------------------------------
// wxPen
//-----------------------------------------------------------------------------

class wxPenRefData : public wxGDIRefData
{
public:
    wxPenRefData(const wxColour& clr = wxNullColour, wxPenStyle style = wxPENSTYLE_SOLID)
        : m_style(wxPENSTYLE_SOLID),
          m_width(1),
          m_joinStyle(wxJOIN_ROUND),
          m_capStyle(wxCAP_ROUND),
          m_colour(clr)
    {
        SetStyle(style);
    }

    wxPenRefData(const wxPenInfo& info)
        : m_style(info.GetStyle()),
          m_width(info.GetWidth() > 0 ? info.GetWidth() : 1),
          m_joinStyle(info.GetJoin() != wxJOIN_INVALID ? info.GetJoin() : wxJOIN_ROUND),
          m_capStyle(info.GetCap() != wxCAP_INVALID ? info.GetCap() : wxCAP_ROUND),
          m_colour(info.GetColour())
    {
        SetStyle(m_style);
    }

    wxPenRefData(const wxPenRefData& data)
        : m_style(data.m_style),
          m_width(data.m_width),
          m_joinStyle(data.m_joinStyle),
          m_capStyle(data.m_capStyle),
          m_colour(data.m_colour)
    {
    }

    virtual bool IsOk() const { return m_colour.IsOk(); }

    void SetStyle(wxPenStyle style)
    {
        if ( style != wxPENSTYLE_SOLID && style != wxPENSTYLE_TRANSPARENT )
        {
            wxFAIL_MSG( "only wxPENSTYLE_SOLID and wxPENSTYLE_TRANSPARENT styles are supported" );
            style = wxPENSTYLE_SOLID;
        }

        m_style = style;
    }

    bool operator==(const wxPenRefData& other) const
    {
        return m_style == other.m_style &&
               m_width == other.m_width &&
               m_joinStyle == other.m_joinStyle &&
               m_capStyle == other.m_capStyle &&
               m_colour == other.m_colour;
    }

    wxPenStyle     m_style;
    int            m_width;
    wxPenJoin      m_joinStyle;
    wxPenCap       m_capStyle;
    wxColour       m_colour;
};

//-----------------------------------------------------------------------------

#define M_PENDATA ((wxPenRefData *)m_refData)

wxIMPLEMENT_DYNAMIC_CLASS(wxPen, wxGDIObject);

wxPen::wxPen(const wxColour &colour, int width, wxPenStyle style)
{
    wxASSERT_MSG( width <= 1, "only width=0,1 are supported" );

    m_refData = new wxPenRefData(colour, style);
    if ( width >= 0 )
        M_PENDATA->m_width = width > 0 ? width : 1;
}

wxPen::wxPen(const wxColour& col, int width, int style)
{
    m_refData = new wxPenRefData(col, (wxPenStyle)style);
    if ( width >= 0 )
        M_PENDATA->m_width = width > 0 ? width : 1;
}

wxPen::wxPen(const wxBitmap& WXUNUSED(stipple), int WXUNUSED(width))
{
    wxFAIL_MSG( "stipple pens not supported" );

    m_refData = new wxPenRefData();
}

wxPen::wxPen(const wxPenInfo& info)
{
    m_refData = new wxPenRefData(info);
}

bool wxPen::operator==(const wxPen& pen) const
{
    if ( m_refData == pen.m_refData )
        return true;
    if ( !m_refData || !pen.m_refData )
        return false;

    return *(wxPenRefData*)m_refData == *(wxPenRefData*)pen.m_refData;
}

void wxPen::SetColour(const wxColour &colour)
{
    AllocExclusive();
    M_PENDATA->m_colour = colour;
}

void wxPen::SetDashes(int WXUNUSED(number_of_dashes), const wxDash *WXUNUSED(dash))
{
    wxFAIL_MSG( "SetDashes not implemented" );
}

void wxPen::SetColour(unsigned char red, unsigned char green, unsigned char blue)
{
    AllocExclusive();
    M_PENDATA->m_colour.Set(red, green, blue);
}

void wxPen::SetCap(wxPenCap capStyle)
{
    AllocExclusive();
    M_PENDATA->m_capStyle = capStyle != wxCAP_INVALID ? capStyle : wxCAP_ROUND;
}

void wxPen::SetJoin(wxPenJoin joinStyle)
{
    AllocExclusive();
    M_PENDATA->m_joinStyle = joinStyle != wxJOIN_INVALID ? joinStyle : wxJOIN_ROUND;
}

void wxPen::SetStyle(wxPenStyle style)
{
    AllocExclusive();
    M_PENDATA->SetStyle(style);
}

void wxPen::SetStipple(const wxBitmap& WXUNUSED(stipple))
{
    wxFAIL_MSG( "SetStipple not implemented" );
}

void wxPen::SetWidth(int width)
{
    AllocExclusive();
    M_PENDATA->m_width = width > 0 ? width : 1;
}

int wxPen::GetDashes(wxDash **ptr) const
{
    wxFAIL_MSG( "GetDashes not implemented" );

    *ptr = nullptr;
    return 0;
}

int wxPen::GetDashCount() const
{
    wxFAIL_MSG( "GetDashCount not implemented" );

    return 0;
}

wxDash* wxPen::GetDash() const
{
    wxFAIL_MSG( "GetDash not implemented" );

    return nullptr;
}

wxPenCap wxPen::GetCap() const
{
    wxCHECK_MSG( IsOk(), wxCAP_ROUND, wxT("invalid pen") );

    return M_PENDATA->m_capStyle;
}

wxPenJoin wxPen::GetJoin() const
{
    wxCHECK_MSG( IsOk(), wxJOIN_ROUND, wxT("invalid pen") );

    return M_PENDATA->m_joinStyle;
}

wxPenStyle wxPen::GetStyle() const
{
    wxCHECK_MSG( IsOk(), wxPENSTYLE_INVALID, wxT("invalid pen") );

    return M_PENDATA->m_style;
}

int wxPen::GetWidth() const
{
    wxCHECK_MSG( IsOk(), 1, wxT("invalid pen") );

    return M_PENDATA->m_width;
}

wxColour wxPen::GetColour() const
{
    wxCHECK_MSG( IsOk(), wxNullColour, wxT("invalid pen") );

    return M_PENDATA->m_colour;
}

wxBitmap *wxPen::GetStipple() const
{
    wxCHECK_MSG( IsOk(), nullptr, wxT("invalid pen") );

    wxFAIL_MSG( "GetStipple not implemented" );
    return nullptr;
}

wxGDIRefData *wxPen::CreateGDIRefData() const
{
    return new wxPenRefData;
}

wxGDIRefData *wxPen::CloneGDIRefData(const wxGDIRefData *data) const
{
    return new wxPenRefData(*(wxPenRefData *)data);
}
