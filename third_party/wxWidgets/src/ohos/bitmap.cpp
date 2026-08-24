/////////////////////////////////////////////////////////////////////////////
// wxBitmap for OHOS — real RGBA8888 buffer (P-3.1; was metadata-only stub)
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/bitmap.h"
    #include "wx/dc.h"
#endif

#if wxUSE_IMAGE
    #include "wx/image.h"
#endif

#include "wx/rawbmp.h"

#include <cstring>
#include <vector>

class wxBitmapRefData : public wxGDIRefData
{
public:
    wxBitmapRefData()
        : m_width(0), m_height(0), m_depth(0), m_stride(0) {}

    wxBitmapRefData(const wxBitmapRefData& d)
        : wxGDIRefData(),
          m_width(d.m_width),
          m_height(d.m_height),
          m_depth(d.m_depth),
          m_stride(d.m_stride),
          m_pixels(d.m_pixels)
    {
    }

    virtual bool IsOk() const override
    {
        return m_width > 0 && m_height > 0 && !m_pixels.empty();
    }

    bool Alloc(int width, int height, int depth)
    {
        if ( width <= 0 || height <= 0 )
            return false;
        m_width = width;
        m_height = height;
        m_depth = depth > 0 ? depth : 32;
        // Always store RGBA8888 for Present; depth metadata may say 24/32.
        m_stride = width * 4;
        m_pixels.assign(static_cast<size_t>(m_stride) * static_cast<size_t>(height), 0);
        return !m_pixels.empty();
    }

    int m_width;
    int m_height;
    int m_depth;
    int m_stride;
    std::vector<unsigned char> m_pixels;
};

#define M_BMPDATA ((wxBitmapRefData *)m_refData)

wxIMPLEMENT_DYNAMIC_CLASS(wxBitmap, wxGDIObject);

wxBitmap::wxBitmap(const char WXUNUSED(bits)[], int width, int height, int depth)
{
    Create(width, height, depth);
}

wxBitmap::wxBitmap(const wxString& WXUNUSED(filename), wxBitmapType WXUNUSED(type)) {}

bool wxBitmap::Create(int width, int height, int depth)
{
    UnRef();
    auto* data = new wxBitmapRefData;
    if ( !data->Alloc(width, height, depth) )
    {
        delete data;
        return false;
    }
    m_refData = data;
    return true;
}

int wxBitmap::GetHeight() const { return IsOk() ? M_BMPDATA->m_height : 0; }
int wxBitmap::GetWidth() const { return IsOk() ? M_BMPDATA->m_width : 0; }
int wxBitmap::GetDepth() const { return IsOk() ? M_BMPDATA->m_depth : 0; }

#if wxUSE_IMAGE
wxImage wxBitmap::ConvertToImage() const
{
    if ( !IsOk() )
        return wxImage();
    wxImage img(GetWidth(), GetHeight(), false);
    unsigned char* dst = img.GetData();
    const unsigned char* src = M_BMPDATA->m_pixels.data();
    const int w = GetWidth();
    const int h = GetHeight();
    const int stride = M_BMPDATA->m_stride;
    for ( int y = 0; y < h; ++y )
    {
        const unsigned char* row = src + static_cast<size_t>(y) * stride;
        for ( int x = 0; x < w; ++x )
        {
            dst[0] = row[x * 4 + 0];
            dst[1] = row[x * 4 + 1];
            dst[2] = row[x * 4 + 2];
            dst += 3;
        }
    }
    return img;
}

void wxBitmap::InitFromImage(const wxImage& image, int depth, double WXUNUSED(scale))
{
    if ( !Create(image.GetWidth(), image.GetHeight(), depth) )
        return;
    if ( !image.IsOk() )
        return;
    const unsigned char* src = image.GetData();
    unsigned char* dst = M_BMPDATA->m_pixels.data();
    const int w = GetWidth();
    const int h = GetHeight();
    const int stride = M_BMPDATA->m_stride;
    for ( int y = 0; y < h; ++y )
    {
        unsigned char* row = dst + static_cast<size_t>(y) * stride;
        for ( int x = 0; x < w; ++x )
        {
            row[x * 4 + 0] = src[0];
            row[x * 4 + 1] = src[1];
            row[x * 4 + 2] = src[2];
            row[x * 4 + 3] = 0xFF;
            src += 3;
        }
    }
}
#endif

wxMask *wxBitmap::GetMask() const { return nullptr; }
void wxBitmap::SetMask(wxMask *mask) { delete mask; }

wxBitmap wxBitmap::GetSubBitmap(const wxRect& rect) const
{
    wxBitmap bmp;
    if ( !IsOk() || !bmp.Create(rect.width, rect.height, GetDepth()) )
        return bmp;
    const unsigned char* srcBase = GetOhosPixels();
    unsigned char* dstBase = bmp.GetOhosPixels();
    const int srcStride = GetOhosStride();
    const int dstStride = bmp.GetOhosStride();
    if ( !srcBase || !dstBase )
        return bmp;
    for ( int y = 0; y < rect.height; ++y )
    {
        const unsigned char* src = srcBase + static_cast<size_t>(rect.y + y) * srcStride +
                                   static_cast<size_t>(rect.x) * 4;
        unsigned char* dst = dstBase + static_cast<size_t>(y) * dstStride;
        std::memcpy(dst, src, static_cast<size_t>(rect.width) * 4);
    }
    return bmp;
}

bool wxBitmap::SaveFile(const wxString&, wxBitmapType, const wxPalette*) const { return false; }
bool wxBitmap::LoadFile(const wxString&, wxBitmapType) { return false; }

#if wxUSE_PALETTE
wxPalette *wxBitmap::GetPalette() const { return nullptr; }
void wxBitmap::SetPalette(const wxPalette&) {}
#endif

void wxBitmap::InitStandardHandlers() {}

void *wxBitmap::GetRawData(wxPixelDataBase& WXUNUSED(data), int bpp)
{
    // Prefer GetOhosPixels() for OHOS Present path; rawbmp accessors optional later.
    if ( !IsOk() || bpp != 32 )
        return nullptr;
    return M_BMPDATA->m_pixels.data();
}

void wxBitmap::UngetRawData(wxPixelDataBase&) {}

bool wxBitmap::HasAlpha() const { return IsOk(); }

wxGDIRefData *wxBitmap::CreateGDIRefData() const { return new wxBitmapRefData; }
wxGDIRefData *wxBitmap::CloneGDIRefData(const wxGDIRefData *data) const
{
    return new wxBitmapRefData(*static_cast<const wxBitmapRefData*>(data));
}

unsigned char* wxBitmap::GetOhosPixels()
{
    return IsOk() ? M_BMPDATA->m_pixels.data() : nullptr;
}

const unsigned char* wxBitmap::GetOhosPixels() const
{
    return IsOk() ? M_BMPDATA->m_pixels.data() : nullptr;
}

int wxBitmap::GetOhosStride() const
{
    return IsOk() ? M_BMPDATA->m_stride : 0;
}

size_t wxBitmap::GetOhosByteCount() const
{
    return IsOk() ? M_BMPDATA->m_pixels.size() : 0;
}
