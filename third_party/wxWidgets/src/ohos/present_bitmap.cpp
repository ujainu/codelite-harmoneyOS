/////////////////////////////////////////////////////////////////////////////
// P-4 transparent Present: Bitmap RGBA → NativeWindow (no UI knowledge)
// Also: gated P-3 checker probe + R-1 RendererNative probe.
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/bitmap.h"
    #include "wx/dcmemory.h"
    #include "wx/toplevel.h"
    #include "wx/window.h"
#endif

#include "wx/renderer.h"
#include "wx/ohos/nativewindow.h"
#include "wx/ohos/toplevel.h"

#include <native_buffer/native_buffer.h>
#include <native_window/external_window.h>

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <hilog/log.h>
#include <poll.h>
#include <sys/mman.h>
#include <unistd.h>

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xF004
#define LOG_TAG "wxOHOS"

namespace {

uint32_t Crc32Buf(const unsigned char* data, size_t n)
{
    uint32_t crc = 0xFFFFFFFFu;
    for ( size_t i = 0; i < n; ++i )
    {
        crc ^= data[i];
        for ( int k = 0; k < 8; ++k )
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

} // namespace

bool wxOhos_PresentBitmap(void* ohNativeWindow, const wxBitmap& bmp)
{
    auto* nw = reinterpret_cast<OHNativeWindow*>(ohNativeWindow);
    if ( !nw || !bmp.IsOk() || !bmp.GetOhosPixels() )
        return false;

    OHNativeWindowBuffer* buffer = nullptr;
    int fenceFd = -1;
    int32_t ret = OH_NativeWindow_NativeWindowRequestBuffer(nw, &buffer, &fenceFd);
    if ( ret != 0 || !buffer )
    {
        OH_LOG_ERROR(LOG_APP, "[Present] FAIL RequestBuffer ret=%{public}d", (int)ret);
        return false;
    }
    if ( fenceFd >= 0 )
    {
        pollfd pfd{ fenceFd, POLLIN, 0 };
        (void)poll(&pfd, 1, 3000);
        close(fenceFd);
    }

    BufferHandle* handle = OH_NativeWindow_GetBufferHandleFromNative(buffer);
    if ( !handle || handle->fd < 0 || handle->size <= 0 )
    {
        Region empty{ nullptr, 0 };
        OH_NativeWindow_NativeWindowFlushBuffer(nw, buffer, -1, empty);
        OH_LOG_ERROR(LOG_APP, "[Present] FAIL bad BufferHandle");
        return false;
    }

    void* mapped = mmap(handle->virAddr, static_cast<size_t>(handle->size),
                        PROT_READ | PROT_WRITE, MAP_SHARED, handle->fd, 0);
    if ( mapped == MAP_FAILED )
        mapped = mmap(nullptr, static_cast<size_t>(handle->size), PROT_READ | PROT_WRITE,
                      MAP_SHARED, handle->fd, 0);
    if ( mapped == MAP_FAILED )
    {
        Region empty{ nullptr, 0 };
        OH_NativeWindow_NativeWindowFlushBuffer(nw, buffer, -1, empty);
        OH_LOG_ERROR(LOG_APP, "[Present] FAIL mmap errno=%{public}d", errno);
        return false;
    }

    const int srcStride = bmp.GetOhosStride();
    const int dstStride = handle->stride > 0 ? handle->stride : srcStride;
    const int copyW = std::min(bmp.GetWidth(),
                               dstStride > 0 ? dstStride / 4 : bmp.GetWidth());
    const int copyH = std::min(bmp.GetHeight(),
                               handle->height > 0 ? handle->height : bmp.GetHeight());
    const unsigned char* src = bmp.GetOhosPixels();
    auto* dstBase = static_cast<unsigned char*>(mapped);

    size_t copied = 0;
    for ( int y = 0; y < copyH; ++y )
    {
        std::memcpy(dstBase + static_cast<size_t>(y) * dstStride,
                    src + static_cast<size_t>(y) * srcStride,
                    static_cast<size_t>(copyW) * 4);
        copied += static_cast<size_t>(copyW) * 4;
    }

    munmap(mapped, static_cast<size_t>(handle->size));

    Region region{ nullptr, 0 };
    ret = OH_NativeWindow_NativeWindowFlushBuffer(nw, buffer, -1, region);
    if ( ret != 0 )
    {
        OH_LOG_ERROR(LOG_APP, "[Present] FAIL FlushBuffer ret=%{public}d", (int)ret);
        return false;
    }

    static int s_presentLog = 0;
    if ( s_presentLog < 6 )
    {
        ++s_presentLog;
        OH_LOG_INFO(LOG_APP,
                    "[Present] OK copied=%{public}zu strideSrc=%{public}d "
                    "strideDst=%{public}d size=%{public}dx%{public}d",
                    copied, srcStride, dstStride, copyW, copyH);
    }
    return true;
}

extern "C" int wxOhos_PresentTopWindow(void)
{
    if ( !wxTheApp )
        return 0;
    wxWindow* top = wxTheApp->GetTopWindow();
    if ( !top || !top->IsTopLevel() )
        return 0;
    return static_cast<wxTopLevelWindowOHOS*>(top)->PresentBackingStore() ? 1 : 0;
}

extern "C" int wxOhos_R1_ProbeRendererNative(void)
{
    if ( !wxTheApp )
    {
        OH_LOG_ERROR(LOG_APP, "[R-1] FAIL wxTheApp null");
        return 0;
    }

    wxWindow* top = wxTheApp->GetTopWindow();
    if ( !top )
    {
        OH_LOG_ERROR(LOG_APP, "[R-1] FAIL no TopWindow");
        return 0;
    }

    const int w = 240;
    const int h = 120;
    wxBitmap bmp(w, h, 32);
    unsigned char* pixels = bmp.GetOhosPixels();
    if ( !bmp.IsOk() || !pixels )
    {
        OH_LOG_ERROR(LOG_APP, "[R-1] FAIL bitmap buffer null");
        return 0;
    }

    // Neutral fill so CRC before ≠ after when renderer draws.
    std::memset(pixels, 0x77, bmp.GetOhosByteCount());
    const uint32_t crcBefore = Crc32Buf(pixels, bmp.GetOhosByteCount());

    wxMemoryDC dc;
    dc.SelectObject(bmp);
    if ( !dc.IsOk() )
    {
        OH_LOG_ERROR(LOG_APP, "[R-1] FAIL MemoryDC");
        return 0;
    }

    wxRendererNative& rnd = wxRendererNative::Get();
    rnd.DrawPushButton(top, dc, wxRect(8, 8, 100, 32), 0);
    rnd.DrawCheckBox(top, dc, wxRect(120, 12, 20, 20), 0);
    rnd.DrawItemSelectionRect(top, dc, wxRect(8, 52, 220, 28), wxCONTROL_SELECTED);
    rnd.DrawFocusRect(top, dc, wxRect(8, 88, 220, 24), 0);

    dc.SelectObject(wxNullBitmap);

    const uint32_t crcAfter = Crc32Buf(bmp.GetOhosPixels(), bmp.GetOhosByteCount());
    OH_LOG_INFO(LOG_APP,
                "[R-1] RendererNative DrawPushButton/CheckBox/Selection/Focus "
                "CRC before=%{public}08x after=%{public}08x",
                (unsigned)crcBefore, (unsigned)crcAfter);

    if ( crcBefore == crcAfter )
    {
        OH_LOG_ERROR(LOG_APP, "[R-1] FAIL RendererNative did not modify Bitmap");
        return 0;
    }

    OH_LOG_INFO(LOG_APP, "[R-1] OK wxRendererNative → Bitmap");
    return 1;
}

extern "C" int wxOhos_P3_ProbeBackingStorePresent(void)
{
    // Gated: official path is paint → TLW backing → Present. Checker is evidence-only.
    const char* en = std::getenv("WXOHOS_P3_PROBE");
    if ( !en || en[0] != '1' )
    {
        OH_LOG_INFO(LOG_APP, "[P-3] skipped (set WXOHOS_P3_PROBE=1 to force checker)");
        return 0;
    }

    if ( !wxTheApp )
        return 0;

    wxWindow* top = wxTheApp->GetTopWindow();
    if ( !top || !top->IsTopLevel() )
        return 0;

    auto* tlw = static_cast<wxTopLevelWindowOHOS*>(top);
    auto* nw = tlw->GetNativeWindow();
    if ( !nw )
        return 0;

    int w = 0, h = 0;
    top->GetClientSize(&w, &h);
    if ( w <= 0 || h <= 0 )
        top->GetSize(&w, &h);
    if ( w <= 0 || h <= 0 )
        return 0;

    wxBitmap bmp(w, h, 32);
    unsigned char* pixels = bmp.GetOhosPixels();
    if ( !bmp.IsOk() || !pixels )
        return 0;

    OH_LOG_INFO(LOG_APP,
                "[P-3.1] OK Bitmap %{public}dx%{public}d depth=%{public}d stride=%{public}d "
                "bytes=%{public}zu buffer=%{public}p",
                bmp.GetWidth(), bmp.GetHeight(), bmp.GetDepth(), bmp.GetOhosStride(),
                bmp.GetOhosByteCount(), static_cast<void*>(pixels));

    const uint32_t crcBefore = Crc32Buf(pixels, bmp.GetOhosByteCount());

    wxMemoryDC dc;
    dc.SelectObject(bmp);
    dc.SetBackground(wxBrush(wxColour(32, 32, 40)));
    dc.Clear();
    dc.SetBrush(*wxRED_BRUSH);
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.DrawRectangle(0, 0, std::min(200, w), std::min(200, h));
    dc.SetBrush(wxBrush(wxColour(0, 180, 80)));
    dc.DrawRectangle(std::min(220, w / 2), 0, std::min(200, w), std::min(200, h));
    for ( int y = 0; y < std::min(64, h); ++y )
    {
        for ( int x = 0; x < w; ++x )
        {
            const bool on = ((x / 16) ^ (y / 16)) & 1;
            unsigned char* p = pixels + static_cast<size_t>(y) * bmp.GetOhosStride() +
                               static_cast<size_t>(x) * 4;
            if ( on )
            {
                p[0] = 240;
                p[1] = 240;
                p[2] = 40;
                p[3] = 255;
            }
        }
    }
    dc.SelectObject(wxNullBitmap);

    const uint32_t crcAfter = Crc32Buf(bmp.GetOhosPixels(), bmp.GetOhosByteCount());
    OH_LOG_INFO(LOG_APP, "[P-3.3] CRC before=%{public}08x after=%{public}08x",
                (unsigned)crcBefore, (unsigned)crcAfter);
    if ( crcBefore == crcAfter )
        return 0;
    OH_LOG_INFO(LOG_APP, "[P-3.2] OK MemoryDC SelectObject Selected=1");
    OH_LOG_INFO(LOG_APP, "[P-3.3] OK Bitmap pixels changed (draw wrote RGBA)");

    if ( !wxOhos_PresentBitmap(nw, bmp) )
        return 0;
    OH_LOG_INFO(LOG_APP, "[P-3.4] OK CPU Present from Bitmap");
    OH_LOG_INFO(LOG_APP, "[P-3] OK BackingStore → CPU Present chain");
    return 1;
}
