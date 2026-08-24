/////////////////////////////////////////////////////////////////////////////
// Minimal wxDataFormat for OHOS
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/dataobj.h"
#endif

wxDataFormat::wxDataFormat(wxDataFormatId formatId)
    : m_formatId(formatId)
{
}

wxDataFormat::wxDataFormat(const wxString& id)
    : m_mimeType(id), m_formatId(wxDF_PRIVATE)
{
}

const wxString& wxDataFormat::GetId() const { return m_mimeType; }
void wxDataFormat::SetId(const wxString& id)
{
    m_mimeType = id;
    m_formatId = wxDF_PRIVATE;
}

wxDataFormatId wxDataFormat::GetType() const { return m_formatId; }
void wxDataFormat::SetType(wxDataFormatId type) { m_formatId = type; }

bool wxDataFormat::operator==(wxDataFormatId format) const
{
    return m_formatId == format;
}
bool wxDataFormat::operator!=(wxDataFormatId format) const
{
    return !(*this == format);
}
bool wxDataFormat::operator==(const wxDataFormat& format) const
{
    return m_formatId == format.m_formatId && m_mimeType == format.m_mimeType;
}
bool wxDataFormat::operator!=(const wxDataFormat& format) const
{
    return !(*this == format);
}
