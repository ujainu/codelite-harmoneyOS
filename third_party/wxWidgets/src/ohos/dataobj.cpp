/////////////////////////////////////////////////////////////////////////////
// Minimal wxDataObject for OHOS
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_DATAOBJ

#include "wx/dataobj.h"

wxDataObject::wxDataObject() = default;

bool wxDataObject::IsSupportedFormat(const wxDataFormat& format, Direction dir) const
{
    size_t nFormatCount = GetFormatCount(dir);
    if ( nFormatCount == 1 )
        return format == GetPreferredFormat();

    wxDataFormat *formats = new wxDataFormat[nFormatCount];
    GetAllFormats(formats, dir);

    size_t n;
    for ( n = 0; n < nFormatCount; n++ )
    {
        if ( formats[n] == format )
            break;
    }

    delete [] formats;
    return n < nFormatCount;
}

wxFileDataObject::wxFileDataObject() = default;

void wxFileDataObject::AddFile(const wxString& filename)
{
    m_filenames.Add(filename);
}

#endif // wxUSE_DATAOBJ
