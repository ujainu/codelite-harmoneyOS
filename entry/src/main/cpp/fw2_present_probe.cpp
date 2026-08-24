/////////////////////////////////////////////////////////////////////////////
// MV-4: NativeWindow → EGL → first pixel (Spike gui-b proven paths).
// Not EVT_PAINT / not wxGraphicsContext / not Feature Recovery.
//
// MateBook emulator: eglSwapBuffers may return true without compositor pixels
// (Spike Known Limitation). After EGL API probe, present via CPU NativeWindow.
/////////////////////////////////////////////////////////////////////////////

#include "fw2_present_probe.h"

#include <hilog/log.h>

#include <native_buffer/native_buffer.h>
#include <native_window/external_window.h>

#include <EGL/egl.h>
#include <GLES2/gl2.h>

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <poll.h>
#include <sys/mman.h>
#include <unistd.h>

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xF002
#define LOG_TAG "FW2Host"

namespace {

void Log(const char* msg)
{
    OH_LOG_INFO(LOG_APP, "%{public}s", msg);
    std::fprintf(stderr, "%s\n", msg);
    std::fflush(stderr);
}

void LogF(const char* fmt, ...)
{
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    OH_LOG_INFO(LOG_APP, "%{public}s", buf);
    std::fprintf(stderr, "%s\n", buf);
    std::fflush(stderr);
}

void Fail(const char* msg)
{
    OH_LOG_ERROR(LOG_APP, "%{public}s", msg);
    std::fprintf(stderr, "%s\n", msg);
    std::fflush(stderr);
}

bool PrepareWindow(OHNativeWindow* nw, int32_t w, int32_t h, bool forCpu)
{
    if ( w <= 0 || h <= 0 ) {
        w = 800;
        h = 600;
    }
    const int32_t geoRet = OH_NativeWindow_NativeWindowHandleOpt(nw, SET_BUFFER_GEOMETRY, w, h);
    const int32_t fmtRet = OH_NativeWindow_NativeWindowHandleOpt(
        nw, SET_FORMAT, static_cast<int32_t>(NATIVEBUFFER_PIXEL_FMT_RGBA_8888));
    uint64_t usage = NATIVEBUFFER_USAGE_CPU_READ | NATIVEBUFFER_USAGE_CPU_WRITE |
                     NATIVEBUFFER_USAGE_MEM_DMA;
    if ( !forCpu ) {
        usage |= NATIVEBUFFER_USAGE_HW_RENDER | NATIVEBUFFER_USAGE_HW_TEXTURE;
    }
    const int32_t usageRet = OH_NativeWindow_NativeWindowHandleOpt(nw, SET_USAGE, usage);
    LogF("[MV-4.1] Prepare %s geo=%d fmt=%d usage=%d set=%dx%d",
         forCpu ? "CPU" : "EGL", (int)geoRet, (int)fmtRet, (int)usageRet, (int)w, (int)h);
    return geoRet == 0 && fmtRet == 0;
}

void DestroyEgl(EGLDisplay display, EGLSurface surface, EGLContext context)
{
    if ( display == EGL_NO_DISPLAY )
        return;
    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if ( context != EGL_NO_CONTEXT )
        eglDestroyContext(display, context);
    if ( surface != EGL_NO_SURFACE )
        eglDestroySurface(display, surface);
    eglTerminate(display);
}

// Spike-proven CPU present: RequestBuffer → mmap → fill → FlushBuffer.
bool PresentCpuSolid(OHNativeWindow* nw, uint32_t rgba, int32_t fallbackW, int32_t fallbackH)
{
    OHNativeWindowBuffer* buffer = nullptr;
    int fenceFd = -1;
    int32_t ret = OH_NativeWindow_NativeWindowRequestBuffer(nw, &buffer, &fenceFd);
    if ( ret != 0 || !buffer ) {
        LogF("[MV-4.4] FAIL RequestBuffer ret=%d", (int)ret);
        return false;
    }
    if ( fenceFd >= 0 ) {
        pollfd pfd{ fenceFd, POLLIN, 0 };
        (void)poll(&pfd, 1, 3000);
        close(fenceFd);
    }

    BufferHandle* handle = OH_NativeWindow_GetBufferHandleFromNative(buffer);
    if ( !handle || handle->fd < 0 || handle->size <= 0 ) {
        Region empty{ nullptr, 0 };
        OH_NativeWindow_NativeWindowFlushBuffer(nw, buffer, -1, empty);
        Fail("[MV-4.4] FAIL GetBufferHandleFromNative / bad handle");
        return false;
    }

    void* mapped = mmap(handle->virAddr, static_cast<size_t>(handle->size),
                        PROT_READ | PROT_WRITE, MAP_SHARED, handle->fd, 0);
    if ( mapped == MAP_FAILED ) {
        mapped = mmap(nullptr, static_cast<size_t>(handle->size), PROT_READ | PROT_WRITE,
                      MAP_SHARED, handle->fd, 0);
    }
    if ( mapped == MAP_FAILED ) {
        Region empty{ nullptr, 0 };
        OH_NativeWindow_NativeWindowFlushBuffer(nw, buffer, -1, empty);
        LogF("[MV-4.4] FAIL mmap errno=%d", errno);
        return false;
    }

    const int32_t w = handle->width > 0 ? handle->width : fallbackW;
    const int32_t h = handle->height > 0 ? handle->height : fallbackH;
    auto* base = reinterpret_cast<uint8_t*>(mapped);
    if ( handle->stride >= w * 4 ) {
        const int32_t strideBytes = handle->stride;
        for ( int32_t y = 0; y < h; ++y ) {
            auto* row = reinterpret_cast<uint32_t*>(base + static_cast<size_t>(y) * strideBytes);
            for ( int32_t x = 0; x < w; ++x )
                row[x] = rgba;
        }
    } else {
        const int32_t stridePixels = handle->stride > 0 ? handle->stride : w;
        auto* addr = reinterpret_cast<uint32_t*>(base);
        for ( int32_t y = 0; y < h; ++y )
            for ( int32_t x = 0; x < w; ++x )
                addr[static_cast<size_t>(y) * stridePixels + x] = rgba;
    }

    LogF("[MV-4.4] CPU mmap OK w=%d h=%d stride=%d fd=%d size=%d",
         (int)handle->width, (int)handle->height, (int)handle->stride,
         (int)handle->fd, (int)handle->size);
    munmap(mapped, static_cast<size_t>(handle->size));

    Region region{ nullptr, 0 };
    ret = OH_NativeWindow_NativeWindowFlushBuffer(nw, buffer, -1, region);
    if ( ret != 0 ) {
        LogF("[MV-4.4] FAIL FlushBuffer ret=%d", (int)ret);
        return false;
    }
    return true;
}

} // namespace

int Fw2_MV4_ProbePresent(void* ohNativeWindow, int32_t width, int32_t height)
{
    auto* nw = reinterpret_cast<OHNativeWindow*>(ohNativeWindow);
    if ( !nw ) {
        Fail("[MV-4.1] FAIL null OHNativeWindow");
        return 0;
    }

    // ── MV-4.1: NativeWindow surface geometry/format ────────────────────
    if ( !PrepareWindow(nw, width, height, /*forCpu=*/false) ) {
        Fail("[MV-4.1] FAIL SET_BUFFER_GEOMETRY/FORMAT");
        return 0;
    }

    int32_t getH = 0;
    int32_t getW = 0;
    const int32_t getGeoRet =
        OH_NativeWindow_NativeWindowHandleOpt(nw, GET_BUFFER_GEOMETRY, &getH, &getW);
    int32_t format = -1;
    const int32_t getFmtRet = OH_NativeWindow_NativeWindowHandleOpt(nw, GET_FORMAT, &format);
    uint64_t usage = 0;
    (void)OH_NativeWindow_NativeWindowHandleOpt(nw, GET_USAGE, &usage);

    LogF("[MV-4.1] GET geoRet=%d size=%dx%d fmtRet=%d format=%d usage=0x%llx",
         (int)getGeoRet, (int)getW, (int)getH, (int)getFmtRet, (int)format,
         (unsigned long long)usage);

    if ( getGeoRet != 0 || getW <= 0 || getH <= 0 ) {
        Fail("[MV-4.1] FAIL NativeWindow surface geometry invalid");
        return 0;
    }
    if ( getFmtRet != 0 || format < 0 ) {
        Fail("[MV-4.1] FAIL NativeWindow format invalid");
        return 0;
    }
    Log("[MV-4.1] OK NativeWindow surface (geometry/format)");

    // ── MV-4.2 / MV-4.3: EGL WindowSurface + MakeCurrent (API probe) ────
    eglBindAPI(EGL_OPENGL_ES_API);
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    EGLSurface eglSurface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    bool eglApiOk = false;

    if ( display == EGL_NO_DISPLAY ) {
        Fail("[MV-4.2] FAIL eglGetDisplay");
        // Still try CPU present for compositor proof.
    } else if ( !eglInitialize(display, nullptr, nullptr) ) {
        LogF("[MV-4.2] FAIL eglInitialize err=0x%x", (unsigned)eglGetError());
        display = EGL_NO_DISPLAY;
    } else {
        const EGLint attribs[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_NONE
        };
        EGLConfig config = nullptr;
        EGLint numConfigs = 0;
        if ( !eglChooseConfig(display, attribs, &config, 1, &numConfigs) || numConfigs == 0 ) {
            LogF("[MV-4.2] FAIL eglChooseConfig err=0x%x", (unsigned)eglGetError());
            eglTerminate(display);
            display = EGL_NO_DISPLAY;
        } else {
            auto nativeWin = reinterpret_cast<EGLNativeWindowType>(nw);
            eglSurface = eglCreateWindowSurface(display, config, nativeWin, nullptr);
            if ( eglSurface == EGL_NO_SURFACE ) {
                LogF("[MV-4.2] FAIL eglCreateWindowSurface err=0x%x", (unsigned)eglGetError());
                eglTerminate(display);
                display = EGL_NO_DISPLAY;
            } else {
                Log("[MV-4.2] OK eglCreateWindowSurface");
                const EGLint ctxAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
                context = eglCreateContext(display, config, EGL_NO_CONTEXT, ctxAttribs);
                if ( context == EGL_NO_CONTEXT ) {
                    LogF("[MV-4.3] FAIL eglCreateContext err=0x%x", (unsigned)eglGetError());
                } else if ( !eglMakeCurrent(display, eglSurface, eglSurface, context) ) {
                    LogF("[MV-4.3] FAIL eglMakeCurrent err=0x%x", (unsigned)eglGetError());
                } else {
                    Log("[MV-4.3] OK eglMakeCurrent");
                    glViewport(0, 0, getW, getH);
                    glClearColor(0.10f, 0.35f, 0.85f, 1.0f);
                    glClear(GL_COLOR_BUFFER_BIT);
                    glFinish();
                    if ( !eglSwapBuffers(display, eglSurface) ) {
                        LogF("[MV-4.4] eglSwapBuffers API FAIL err=0x%x", (unsigned)eglGetError());
                    } else {
                        Log("[MV-4.4] eglSwapBuffers API returned true (may be silent on DGLES)");
                        eglApiOk = true;
                    }
                }
            }
        }
    }

    // Release EGL so CPU path can own the NativeWindow (Spike: DGLES often blank).
    DestroyEgl(display, eglSurface, context);
    display = EGL_NO_DISPLAY;

    // ── MV-4.4: first compositor pixel via Spike CPU present ────────────
    if ( !PrepareWindow(nw, getW, getH, /*forCpu=*/true) ) {
        Fail("[MV-4.4] FAIL PrepareWindow CPU");
        return 0;
    }

    // RGBA8888 solid blue (ABGR word on little-endian = 0xFF853C1A? Use Spike style).
    // Spike PhaseRgba8888 — use 0xFFFF661A-ish; simple: R=0x1A G=0x3C B=0x85 A=0xFF
    constexpr uint32_t kBlue = 0xFF853C1Au; // A R G B depending on format — Spike used phase colors
    // Match Spike: typically 0xAARRGGBB in memory for RGBA_8888 little-endian as bytes R,G,B,A
    // Spike PhaseRgba8888 — read it
    const uint32_t pixel = 0xFF1A3C85u; // try AABBGGRR / common OHOS: write as R G B A bytes
    // Explicit byte layout RGBA:
    const uint32_t rgba = (uint32_t)0x1A | ((uint32_t)0x3C << 8) | ((uint32_t)0x85 << 16) |
                          ((uint32_t)0xFF << 24);

    bool cpuOk = false;
    for ( int i = 0; i < 3; ++i ) {
        if ( PresentCpuSolid(nw, rgba, getW, getH) ) {
            cpuOk = true;
        } else {
            cpuOk = false;
            break;
        }
    }

    if ( !cpuOk ) {
        Fail("[MV-4.4] FAIL first pixel (CPU NativeWindow present)");
        return 0;
    }

    LogF("[MV-4.4] OK first pixel presented (CPU NativeWindow; eglApiOk=%d)", eglApiOk ? 1 : 0);
    Log("[MV-4.4] OK expect solid blue on XComponent (eye)");
    (void)kBlue;
    (void)pixel;
    return 1;
}
