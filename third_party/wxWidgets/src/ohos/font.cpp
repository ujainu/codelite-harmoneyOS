/////////////////////////////////////////////////////////////////////////////
// Minimal wxFont for OHOS
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/font.h"
#endif

#include "wx/fontutil.h"

class wxFontRefData : public wxGDIRefData
{
public:
    wxFontRefData() { m_info.Init(); }
    wxFontRefData(const wxFontRefData& data) : wxGDIRefData(), m_info(data.m_info) {}

    wxNativeFontInfo m_info;
};

#define M_FONTDATA ((wxFontRefData *)m_refData)

wxIMPLEMENT_DYNAMIC_CLASS(wxFont, wxGDIObject);

wxFont::wxFont(const wxString& nativeFontInfoString)
{
    m_refData = new wxFontRefData;
    M_FONTDATA->m_info.FromString(nativeFontInfoString);
}

bool wxFont::Create(int size, wxFontFamily family, wxFontStyle style,
                    wxFontWeight weight, bool underlined,
                    const wxString& face, wxFontEncoding encoding)
{
    m_refData = new wxFontRefData;
    M_FONTDATA->m_info.Init();
    M_FONTDATA->m_info.SetPointSize(size);
    M_FONTDATA->m_info.SetFamily(family);
    M_FONTDATA->m_info.SetStyle(style);
    M_FONTDATA->m_info.SetWeight(weight);
    M_FONTDATA->m_info.SetUnderlined(underlined);
    M_FONTDATA->m_info.SetFaceName(face);
    M_FONTDATA->m_info.SetEncoding(encoding);
    return true;
}

bool wxFont::Create(const wxNativeFontInfo& fontinfo)
{
    m_refData = new wxFontRefData;
    M_FONTDATA->m_info = fontinfo;
    return true;
}

double wxFont::GetFractionalPointSize() const
{
    wxCHECK_MSG(IsOk(), 12.0, "invalid font");
    return M_FONTDATA->m_info.GetFractionalPointSize();
}
wxFontStyle wxFont::GetStyle() const
{
    wxCHECK_MSG(IsOk(), wxFONTSTYLE_NORMAL, "invalid font");
    return M_FONTDATA->m_info.GetStyle();
}
int wxFont::GetNumericWeight() const
{
    wxCHECK_MSG(IsOk(), wxFONTWEIGHT_NORMAL, "invalid font");
    return M_FONTDATA->m_info.GetNumericWeight();
}
wxString wxFont::GetFaceName() const
{
    wxCHECK_MSG(IsOk(), wxString{}, "invalid font");
    return M_FONTDATA->m_info.GetFaceName();
}
bool wxFont::GetUnderlined() const
{
    wxCHECK_MSG(IsOk(), false, "invalid font");
    return M_FONTDATA->m_info.GetUnderlined();
}
wxFontEncoding wxFont::GetEncoding() const
{
    wxCHECK_MSG(IsOk(), wxFONTENCODING_DEFAULT, "invalid font");
    return M_FONTDATA->m_info.GetEncoding();
}
bool wxFont::IsFixedWidth() const { return false; }
const wxNativeFontInfo *wxFont::GetNativeFontInfo() const
{
    return IsOk() ? &M_FONTDATA->m_info : nullptr;
}

void wxFont::SetFractionalPointSize(double pointSize)
{
    AllocExclusive();
    M_FONTDATA->m_info.SetFractionalPointSize(pointSize);
}
void wxFont::SetFamily(wxFontFamily family)
{
    AllocExclusive();
    M_FONTDATA->m_info.SetFamily(family);
}
void wxFont::SetStyle(wxFontStyle style)
{
    AllocExclusive();
    M_FONTDATA->m_info.SetStyle(style);
}
void wxFont::SetNumericWeight(int weight)
{
    AllocExclusive();
    M_FONTDATA->m_info.SetNumericWeight(weight);
}
bool wxFont::SetFaceName(const wxString& faceName)
{
    AllocExclusive();
    return M_FONTDATA->m_info.SetFaceName(faceName);
}
void wxFont::SetUnderlined(bool underlined)
{
    AllocExclusive();
    M_FONTDATA->m_info.SetUnderlined(underlined);
}
void wxFont::SetEncoding(wxFontEncoding encoding)
{
    AllocExclusive();
    M_FONTDATA->m_info.SetEncoding(encoding);
}

wxGDIRefData *wxFont::CreateGDIRefData() const { return new wxFontRefData; }
wxGDIRefData *wxFont::CloneGDIRefData(const wxGDIRefData *data) const
{
    return new wxFontRefData(*static_cast<const wxFontRefData*>(data));
}
wxFontFamily wxFont::DoGetFamily() const
{
    return IsOk() ? M_FONTDATA->m_info.GetFamily() : wxFONTFAMILY_DEFAULT;
}
