/////////////////////////////////////////////////////////////////////////////
// Minimal wxGraphicsRenderer for OHOS (no Cairo).
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#if wxUSE_GRAPHICS_CONTEXT

#ifndef WX_PRECOMP
    #include "wx/bitmap.h"
    #include "wx/brush.h"
    #include "wx/colour.h"
    #include "wx/dcclient.h"
    #include "wx/dcmemory.h"
    #include "wx/font.h"
    #include "wx/icon.h"
    #include "wx/image.h"
    #include "wx/pen.h"
    #include "wx/window.h"
#endif

#include "wx/graphics.h"
#include "wx/private/graphics.h"
#include "wx/ohos/private/graphics.h"
#include "wx/ohos/private/textextent.h"

#include <hilog/log.h>
#include <link.h>
#include <unistd.h>

#define OHOS_GC_LOG(msg) \
    OH_LOG_Print(LOG_APP, LOG_INFO, 0xA0000, "wxOHOS", "[FUI_TRACE] " msg " tid=%{public}d", (int)gettid())

constexpr uintptr_t kWxOhosRendererSlotRva = 0x794548;

namespace
{

class wxOhosGraphicsRenderer;

class wxOhosGraphicsMatrixData : public wxGraphicsMatrixData
{
public:
    explicit wxOhosGraphicsMatrixData(wxGraphicsRenderer* renderer)
        : wxGraphicsMatrixData(renderer),
          m_a(1.0), m_b(0.0), m_c(0.0), m_d(1.0), m_tx(0.0), m_ty(0.0)
    {
    }

    void Concat(const wxGraphicsMatrixData* t) override
    {
        if ( !t )
            return;

        wxDouble a, b, c, d, tx, ty;
        t->Get(&a, &b, &c, &d, &tx, &ty);

        const wxDouble na = m_a * a + m_c * b;
        const wxDouble nb = m_b * a + m_d * b;
        const wxDouble nc = m_a * c + m_c * d;
        const wxDouble nd = m_b * c + m_d * d;
        const wxDouble ntx = m_a * tx + m_c * ty + m_tx;
        const wxDouble nty = m_b * tx + m_d * ty + m_ty;

        m_a = na; m_b = nb; m_c = nc; m_d = nd; m_tx = ntx; m_ty = nty;
    }

    void Set(wxDouble a, wxDouble b, wxDouble c, wxDouble d,
             wxDouble tx, wxDouble ty) override
    {
        m_a = a; m_b = b; m_c = c; m_d = d; m_tx = tx; m_ty = ty;
    }

    void Get(wxDouble* a, wxDouble* b, wxDouble* c, wxDouble* d,
             wxDouble* tx, wxDouble* ty) const override
    {
        if ( a ) *a = m_a;
        if ( b ) *b = m_b;
        if ( c ) *c = m_c;
        if ( d ) *d = m_d;
        if ( tx ) *tx = m_tx;
        if ( ty ) *ty = m_ty;
    }

    void Invert() override
    {
        const wxDouble det = m_a * m_d - m_b * m_c;
        if ( det == 0.0 )
            return;

        const wxDouble ia = m_d / det;
        const wxDouble ib = -m_b / det;
        const wxDouble ic = -m_c / det;
        const wxDouble id = m_a / det;
        const wxDouble itx = -(ia * m_tx + ic * m_ty);
        const wxDouble ity = -(ib * m_tx + id * m_ty);

        m_a = ia; m_b = ib; m_c = ic; m_d = id; m_tx = itx; m_ty = ity;
    }

    bool IsEqual(const wxGraphicsMatrixData* t) const override
    {
        if ( !t )
            return false;

        wxDouble a, b, c, d, tx, ty;
        t->Get(&a, &b, &c, &d, &tx, &ty);
        return m_a == a && m_b == b && m_c == c && m_d == d && m_tx == tx && m_ty == ty;
    }

    bool IsIdentity() const override
    {
        return m_a == 1.0 && m_b == 0.0 && m_c == 0.0 && m_d == 1.0 && m_tx == 0.0 && m_ty == 0.0;
    }

    void Translate(wxDouble dx, wxDouble dy) override { m_tx += dx; m_ty += dy; }
    void Scale(wxDouble xScale, wxDouble yScale) override
    {
        m_a *= xScale; m_b *= xScale; m_c *= yScale; m_d *= yScale;
    }
    void Rotate(wxDouble angle) override
    {
        const wxDouble s = sin(angle);
        const wxDouble c = cos(angle);
        const wxDouble na = m_a * c + m_c * s;
        const wxDouble nb = m_b * c + m_d * s;
        const wxDouble nc = m_a * -s + m_c * c;
        const wxDouble nd = m_b * -s + m_d * c;
        m_a = na; m_b = nb; m_c = nc; m_d = nd;
    }

    void TransformPoint(wxDouble* x, wxDouble* y) const override
    {
        if ( !x || !y )
            return;
        const wxDouble nx = m_a * *x + m_c * *y + m_tx;
        const wxDouble ny = m_b * *x + m_d * *y + m_ty;
        *x = nx;
        *y = ny;
    }

    void TransformDistance(wxDouble* dx, wxDouble* dy) const override
    {
        if ( !dx || !dy )
            return;
        const wxDouble nx = m_a * *dx + m_c * *dy;
        const wxDouble ny = m_b * *dx + m_d * *dy;
        *dx = nx;
        *dy = ny;
    }

    void* GetNativeMatrix() const override { return nullptr; }

    wxGraphicsObjectRefData* Clone() const override
    {
        auto* copy = new wxOhosGraphicsMatrixData(m_renderer);
        copy->m_a = m_a;
        copy->m_b = m_b;
        copy->m_c = m_c;
        copy->m_d = m_d;
        copy->m_tx = m_tx;
        copy->m_ty = m_ty;
        return copy;
    }

private:
    wxDouble m_a, m_b, m_c, m_d, m_tx, m_ty;
};

class wxOhosGraphicsPathData : public wxGraphicsPathData
{
public:
    explicit wxOhosGraphicsPathData(wxGraphicsRenderer* renderer)
        : wxGraphicsPathData(renderer), m_x(0.0), m_y(0.0), m_hasPoint(false)
    {
    }

    void MoveToPoint(wxDouble x, wxDouble y) override { m_x = x; m_y = y; m_hasPoint = true; }
    void AddLineToPoint(wxDouble x, wxDouble y) override { m_x = x; m_y = y; m_hasPoint = true; }
    void AddCurveToPoint(wxDouble, wxDouble, wxDouble, wxDouble, wxDouble x, wxDouble y) override
    {
        m_x = x; m_y = y; m_hasPoint = true;
    }
    void AddPath(const wxGraphicsPathData*) override {}
    void CloseSubpath() override {}
    void GetCurrentPoint(wxDouble* x, wxDouble* y) const override
    {
        if ( x ) *x = m_hasPoint ? m_x : 0.0;
        if ( y ) *y = m_hasPoint ? m_y : 0.0;
    }
    void AddArc(wxDouble x, wxDouble y, wxDouble, wxDouble, wxDouble, bool) override
    {
        m_x = x; m_y = y; m_hasPoint = true;
    }
    void* GetNativePath() const override { return nullptr; }
    void UnGetNativePath(void*) const override {}
    void Transform(const wxGraphicsMatrixData*) override {}
    void GetBox(wxDouble* x, wxDouble* y, wxDouble* w, wxDouble* h) const override
    {
        if ( x ) *x = 0.0;
        if ( y ) *y = 0.0;
        if ( w ) *w = 0.0;
        if ( h ) *h = 0.0;
    }
    bool Contains(wxDouble, wxDouble, wxPolygonFillMode) const override { return false; }

private:
    wxDouble m_x, m_y;
    bool m_hasPoint;
};

class wxOhosPenData : public wxGraphicsObjectRefData
{
public:
    wxOhosPenData(wxGraphicsRenderer* renderer, const wxGraphicsPenInfo& info)
        : wxGraphicsObjectRefData(renderer), m_info(info)
    {
    }

    const wxGraphicsPenInfo& GetInfo() const { return m_info; }

private:
    wxGraphicsPenInfo m_info;
};

class wxOhosBrushData : public wxGraphicsObjectRefData
{
public:
    wxOhosBrushData(wxGraphicsRenderer* renderer, const wxBrush& brush)
        : wxGraphicsObjectRefData(renderer), m_brush(brush)
    {
    }

    const wxBrush& GetBrush() const { return m_brush; }

private:
    wxBrush m_brush;
};

class wxOhosFontData : public wxGraphicsObjectRefData
{
public:
    wxOhosFontData(wxGraphicsRenderer* renderer, const wxFont& font, const wxColour& col)
        : wxGraphicsObjectRefData(renderer), m_font(font), m_colour(col)
    {
    }

    const wxFont& GetFont() const { return m_font; }
    const wxColour& GetColour() const { return m_colour; }

private:
    wxFont m_font;
    wxColour m_colour;
};

class wxOhosBitmapData : public wxGraphicsBitmapData
{
public:
    wxOhosBitmapData(wxGraphicsRenderer* renderer, const wxBitmap& bmp)
        : wxGraphicsBitmapData(renderer), m_bitmap(bmp)
    {
    }

    void* GetNativeBitmap() const override { return nullptr; }

private:
    wxBitmap m_bitmap;
};

class wxOhosGraphicsContext : public wxGraphicsContext
{
public:
    wxOhosGraphicsContext(wxGraphicsRenderer* renderer,
                          wxWindow* window = nullptr,
                          wxDouble width = 1024.0,
                          wxDouble height = 768.0)
        : wxGraphicsContext(renderer, window)
    {
        m_width = width;
        m_height = height;
    }

    void PushState() override {}
    void PopState() override {}
    void Clip(const wxRegion&) override {}
    void Clip(wxDouble, wxDouble, wxDouble, wxDouble) override {}
    void ResetClip() override {}
    void GetClipBox(wxDouble* x, wxDouble* y, wxDouble* w, wxDouble* h) override
    {
        if ( x ) *x = 0.0;
        if ( y ) *y = 0.0;
        if ( w ) *w = m_width;
        if ( h ) *h = m_height;
    }
    void* GetNativeContext() override { return nullptr; }
    bool SetAntialiasMode(wxAntialiasMode antialias) override
    {
        m_antialias = antialias;
        return true;
    }
    bool SetInterpolationQuality(wxInterpolationQuality interpolation) override
    {
        m_interpolation = interpolation;
        return true;
    }
    bool SetCompositionMode(wxCompositionMode op) override
    {
        m_composition = op;
        return true;
    }
    void BeginLayer(wxDouble) override {}
    void EndLayer() override {}
    void Translate(wxDouble, wxDouble) override {}
    void Scale(wxDouble, wxDouble) override {}
    void Rotate(wxDouble) override {}
    void ConcatTransform(const wxGraphicsMatrix&) override {}
    void SetTransform(const wxGraphicsMatrix&) override {}
    wxGraphicsMatrix GetTransform() const override
    {
        wxGraphicsMatrix matrix;
        auto* data = new wxOhosGraphicsMatrixData(const_cast<wxGraphicsRenderer*>(GetRenderer()));
        matrix.SetRefData(data);
        return matrix;
    }
    void StrokePath(const wxGraphicsPath&) override {}
    void FillPath(const wxGraphicsPath&, wxPolygonFillMode) override {}
    void DoDrawText(const wxString&, wxDouble, wxDouble) override {}

    void GetTextExtent(const wxString& text, wxDouble* width, wxDouble* height,
                       wxDouble* descent, wxDouble* externalLeading) const override
    {
        int w = 0, h = 0, d = 0, e = 0;
        wxOhosTextMeasure::FallbackGetTextExtent(text, &w, &h, &d, &e);
        if ( width ) *width = w;
        if ( height ) *height = h;
        if ( descent ) *descent = d;
        if ( externalLeading ) *externalLeading = e;
    }

    void GetPartialTextExtents(const wxString& text, wxArrayDouble& widths) const override
    {
        widths.Clear();
        widths.Add(0, text.length());
        if ( text.empty() )
            return;

        int w = 0, h = 0;
        for ( size_t i = 0; i < text.length(); ++i )
        {
            wxOhosTextMeasure::FallbackGetTextExtent(text.substr(0, i + 1), &w, &h);
            widths[i] = w;
        }
    }

    void DrawBitmap(const wxGraphicsBitmap&, wxDouble, wxDouble, wxDouble, wxDouble) override {}
    void DrawBitmap(const wxBitmap&, wxDouble, wxDouble, wxDouble, wxDouble) override {}
    void DrawIcon(const wxIcon&, wxDouble, wxDouble, wxDouble, wxDouble) override {}
    void DrawRectangle(wxDouble, wxDouble, wxDouble, wxDouble) override {}
    void DrawEllipse(wxDouble, wxDouble, wxDouble, wxDouble) override {}
    void DrawRoundedRectangle(wxDouble, wxDouble, wxDouble, wxDouble, wxDouble) override {}
    void StrokeLines(size_t, const wxPoint2DDouble*) override {}
    void DrawLines(size_t, const wxPoint2DDouble*, wxPolygonFillMode) override {}

    wxDECLARE_NO_COPY_CLASS(wxOhosGraphicsContext);
};

class wxOhosGraphicsRenderer : public wxGraphicsRenderer
{
public:
    wxOhosGraphicsRenderer()
    {
        static bool logged = false;
        if ( !logged )
        {
            OHOS_GC_LOG("wxOHOS GraphicsRenderer created");
            logged = true;
        }
    }

    wxGraphicsContext* CreateContext(const wxWindowDC&) override
    {
        return new wxOhosGraphicsContext(this);
    }

    wxGraphicsContext* CreateContext(const wxMemoryDC&) override
    {
        return new wxOhosGraphicsContext(this);
    }

#if wxUSE_PRINTING_ARCHITECTURE
    wxGraphicsContext* CreateContext(const wxPrinterDC&) override
    {
        return new wxOhosGraphicsContext(this);
    }
#endif

    wxGraphicsContext* CreateContextFromNativeContext(void*) override
    {
        return new wxOhosGraphicsContext(this);
    }

    wxGraphicsContext* CreateContextFromNativeWindow(void*) override
    {
        return new wxOhosGraphicsContext(this);
    }

    wxGraphicsContext* CreateContext(wxWindow* window) override
    {
        return new wxOhosGraphicsContext(this, window);
    }

#if wxUSE_IMAGE
    wxGraphicsContext* CreateContextFromImage(wxImage&) override
    {
        return new wxOhosGraphicsContext(this, nullptr, 1.0, 1.0);
    }
#endif

    wxGraphicsContext* CreateMeasuringContext() override
    {
        return new wxOhosGraphicsContext(this, nullptr, 1.0, 1.0);
    }

    wxGraphicsPath CreatePath() override
    {
        wxGraphicsPath path;
        path.SetRefData(new wxOhosGraphicsPathData(this));
        return path;
    }

    wxGraphicsMatrix CreateMatrix(wxDouble a, wxDouble b, wxDouble c, wxDouble d,
                                  wxDouble tx, wxDouble ty) override
    {
        wxGraphicsMatrix matrix;
        auto* data = new wxOhosGraphicsMatrixData(this);
        data->Set(a, b, c, d, tx, ty);
        matrix.SetRefData(data);
        return matrix;
    }

    wxGraphicsPen CreatePen(const wxGraphicsPenInfo& info) override
    {
        if ( info.GetStyle() == wxPENSTYLE_TRANSPARENT )
            return wxNullGraphicsPen;

        wxGraphicsPenInfo safe = info;
        bool usedFallback = false;

        if ( safe.GetJoin() == wxJOIN_INVALID )
        {
            safe.Join(wxJOIN_ROUND);
            usedFallback = true;
        }
        if ( safe.GetCap() == wxCAP_INVALID )
        {
            safe.Cap(wxCAP_ROUND);
            usedFallback = true;
        }
        if ( safe.GetWidth() <= 0 )
        {
            safe.Width(1);
            usedFallback = true;
        }

        static bool logged = false;
        if ( usedFallback && !logged )
        {
            OHOS_GC_LOG("OHOS CreatePen fallback");
            logged = true;
        }

        wxGraphicsPen pen;
        pen.SetRefData(new wxOhosPenData(this, safe));
        return pen;
    }

    wxGraphicsBrush CreateBrush(const wxBrush& brush) override
    {
        if ( !brush.IsOk() || brush.GetStyle() == wxBRUSHSTYLE_TRANSPARENT )
            return wxNullGraphicsBrush;

        wxGraphicsBrush gfxBrush;
        gfxBrush.SetRefData(new wxOhosBrushData(this, brush));
        return gfxBrush;
    }

    wxGraphicsBrush CreateLinearGradientBrush(wxDouble, wxDouble, wxDouble, wxDouble,
                                                const wxGraphicsGradientStops&,
                                                const wxGraphicsMatrix&) override
    {
        return CreateBrush(*wxBLACK_BRUSH);
    }

    wxGraphicsBrush CreateRadialGradientBrush(wxDouble, wxDouble, wxDouble, wxDouble, wxDouble,
                                              const wxGraphicsGradientStops&,
                                              const wxGraphicsMatrix&) override
    {
        return CreateBrush(*wxBLACK_BRUSH);
    }

    wxGraphicsFont CreateFont(const wxFont& font, const wxColour& col) override
    {
        return CreateFontAtDPI(font, wxRealPoint(), col);
    }

    wxGraphicsFont CreateFont(double, const wxString&, int, const wxColour& col) override
    {
        wxGraphicsFont font;
        font.SetRefData(new wxOhosFontData(this, *wxNORMAL_FONT, col));
        return font;
    }

    wxGraphicsFont CreateFontAtDPI(const wxFont& font, const wxRealPoint&, const wxColour& col) override
    {
        wxGraphicsFont gfxFont;
        gfxFont.SetRefData(new wxOhosFontData(this, font, col));
        return gfxFont;
    }

    wxGraphicsBitmap CreateBitmap(const wxBitmap& bitmap) override
    {
        wxGraphicsBitmap gfxBmp;
        gfxBmp.SetRefData(new wxOhosBitmapData(this, bitmap));
        return gfxBmp;
    }

#if wxUSE_IMAGE
    wxGraphicsBitmap CreateBitmapFromImage(const wxImage& image) override
    {
        return CreateBitmap(wxBitmap(image));
    }

    wxImage CreateImageFromBitmap(const wxGraphicsBitmap& bmp) override
    {
        wxImage image(1, 1);
        wxUnusedVar(bmp);
        return image;
    }
#endif

    wxGraphicsBitmap CreateBitmapFromNativeBitmap(void*) override
    {
        return CreateBitmap(wxBitmap(1, 1));
    }

    wxGraphicsBitmap CreateSubBitmap(const wxGraphicsBitmap&, wxDouble, wxDouble,
                                     wxDouble, wxDouble) override
    {
        return CreateBitmap(wxBitmap(1, 1));
    }

    wxString GetName() const override { return "OHOS"; }

    void GetVersion(int* major, int* minor, int* micro) const override
    {
        if ( major ) *major = 1;
        if ( minor ) *minor = 0;
        if ( micro ) *micro = 0;
    }

private:
    wxDECLARE_DYNAMIC_CLASS_NO_COPY(wxOhosGraphicsRenderer);
};

wxOhosGraphicsRenderer gs_ohosGraphicsRenderer;

} // namespace

wxIMPLEMENT_DYNAMIC_CLASS(wxOhosGraphicsRenderer, wxGraphicsRenderer);

extern "C" __attribute__((visibility("default")))
wxGraphicsRenderer* wxOhosGetGraphicsRenderer()
{
    return &gs_ohosGraphicsRenderer;
}

wxGraphicsRenderer* wxGraphicsRenderer::GetDefaultRenderer()
{
    return wxOhosGetGraphicsRenderer();
}

wxGraphicsRenderer* wxGraphicsRenderer::GetCairoRenderer()
{
    return wxOhosGetGraphicsRenderer();
}

namespace
{

struct WxCoreBaseQuery
{
    uintptr_t base = 0;
};

int WxCorePhdrCallback(struct dl_phdr_info* info, size_t, void* data)
{
    auto* query = static_cast<WxCoreBaseQuery*>(data);
    if ( !info || !info->dlpi_name || !info->dlpi_name[0] )
        return 0;
    if ( std::strstr(info->dlpi_name, "libwx_ohosu_core") == nullptr )
        return 0;
    query->base = static_cast<uintptr_t>(info->dlpi_addr);
    return 1;
}

__attribute__((constructor(101)))
void wxOhosGraphicsAutoInit()
{
    wxGraphicsRenderer* renderer = wxOhosGetGraphicsRenderer();
    if ( !renderer )
        return;

    WxCoreBaseQuery query;
    dl_iterate_phdr(WxCorePhdrCallback, &query);
    if ( !query.base )
        return;

    auto** slot = reinterpret_cast<wxGraphicsRenderer**>(query.base + kWxOhosRendererSlotRva);
    *slot = renderer;
    OHOS_GC_LOG("wxOHOS GraphicsRenderer slot auto-init");
}

} // namespace

#endif // wxUSE_GRAPHICS_CONTEXT
