#include "spike_render.h"
#include "wx_ohos_proto.h"

#include <hilog/log.h>
#include <native_buffer/native_buffer.h>
#include <native_vsync/native_vsync.h>
#include <native_window/buffer_handle.h>

#include <sys/mman.h>
#include <cerrno>
#include <poll.h>

#include <cstring>
#include <unistd.h>

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xB001
#define LOG_TAG "SpikeB2"

namespace {
constexpr int kFramesPerColor = 90; // ~1.5s @60fps
constexpr uint64_t kStablePresentTarget = 300;

// PC MateBook emulator: DGLES reports g_handle null / EGL_BAD_SURFACE even when
// eglSwapBuffers returns true — pixels never reach the compositor (gray XComponent bg).
// Prefer CPU NativeWindow producer for a visible B2 proof; keep EGL behind this flag.
constexpr bool kPreferCpuPresent = true;

void OnSurfaceCreatedCB(OH_NativeXComponent* component, void* window)
{
    SpikeRender::GetInstance()->OnSurfaceCreated(component, window);
}

void OnSurfaceChangedCB(OH_NativeXComponent* component, void* window)
{
    SpikeRender::GetInstance()->OnSurfaceChanged(component, window);
}

void OnSurfaceDestroyedCB(OH_NativeXComponent* component, void* window)
{
    SpikeRender::GetInstance()->OnSurfaceDestroyed(component, window);
}

void DispatchTouchEventCB(OH_NativeXComponent* component, void* window)
{
    SpikeRender::GetInstance()->DispatchTouchEvent(component, window);
}

void DispatchMouseEventCB(OH_NativeXComponent* component, void* window)
{
    SpikeRender::GetInstance()->DispatchMouseEvent(component, window);
}

void DispatchKeyEventCB(OH_NativeXComponent* component, void* window)
{
    SpikeRender::GetInstance()->DispatchKeyEvent(component, window);
}

const char* TouchTypeName(OH_NativeXComponent_TouchEventType t)
{
    switch (t) {
        case OH_NATIVEXCOMPONENT_DOWN:
            return "DOWN";
        case OH_NATIVEXCOMPONENT_UP:
            return "UP";
        case OH_NATIVEXCOMPONENT_MOVE:
            return "MOVE";
        case OH_NATIVEXCOMPONENT_CANCEL:
            return "CANCEL";
        default:
            return "OTHER";
    }
}

const char* MouseActionName(OH_NativeXComponent_MouseEventAction a)
{
    switch (a) {
        case OH_NATIVEXCOMPONENT_MOUSE_PRESS:
            return "PRESS";
        case OH_NATIVEXCOMPONENT_MOUSE_RELEASE:
            return "RELEASE";
        case OH_NATIVEXCOMPONENT_MOUSE_MOVE:
            return "MOVE";
        case OH_NATIVEXCOMPONENT_MOUSE_CANCEL:
            return "CANCEL";
        default:
            return "NONE";
    }
}

const char* PhaseName(int phase)
{
    if (phase == 0) {
        return "blue";
    }
    if (phase == 1) {
        return "red";
    }
    return "green";
}

uint32_t PhaseRgba8888(int phase)
{
    // Memory little-endian: R, G, B, A
    if (phase == 0) {
        return 0xFFFF401A; // blue-ish
    }
    if (phase == 1) {
        return 0xFF1A1AFF; // red
    }
    return 0xFF40FF1A; // green
}

void ApplyGlPhaseColor(int phase)
{
    if (phase == 0) {
        glClearColor(0.1f, 0.35f, 1.0f, 1.0f);
    } else if (phase == 1) {
        glClearColor(1.0f, 0.15f, 0.1f, 1.0f);
    } else {
        glClearColor(0.1f, 0.85f, 0.25f, 1.0f);
    }
}

void NotePhase(SpikeSurface& surface)
{
    const int phase = static_cast<int>((surface.swapOkCount / kFramesPerColor) % 3);
    if (phase != surface.colorPhase) {
        surface.colorPhase = phase;
        OH_LOG_INFO(LOG_APP, "SpikeB2: color → %{public}s (present %{public}llu)", PhaseName(phase),
            static_cast<unsigned long long>(surface.swapOkCount));
    }
}

void NotePresentOk(SpikeSurface& surface, const char* path)
{
    surface.swapOkCount++;
    if (surface.swapOkCount == 1) {
        OH_LOG_INFO(LOG_APP, "SpikeB2: first present OK via %{public}s — expect BLUE→RED→GREEN on screen", path);
    }
    if (surface.swapOkCount == kStablePresentTarget && !surface.logged300) {
        surface.logged300 = true;
        OH_LOG_INFO(LOG_APP, "SpikeB2: PASS continuous present >= %{public}llu (%{public}s)",
            static_cast<unsigned long long>(kStablePresentTarget), path);
    }
    if (surface.swapOkCount % 90 == 0) {
        OH_LOG_INFO(LOG_APP, "SpikeB2: presentOk=%{public}llu path=%{public}s",
            static_cast<unsigned long long>(surface.swapOkCount), path);
    }
}
} // namespace

SpikeRender* SpikeRender::GetInstance()
{
    static SpikeRender instance;
    return &instance;
}

void SpikeRender::VSyncCallback(long long timestamp, void* data)
{
    auto* self = reinterpret_cast<SpikeRender*>(data);
    if (self != nullptr) {
        self->OnVSync(timestamp);
    }
}

void SpikeRender::SetNativeXComponent(OH_NativeXComponent* component)
{
    callback_.OnSurfaceCreated = OnSurfaceCreatedCB;
    callback_.OnSurfaceChanged = OnSurfaceChangedCB;
    callback_.OnSurfaceDestroyed = OnSurfaceDestroyedCB;
    callback_.DispatchTouchEvent = DispatchTouchEventCB;
    OH_NativeXComponent_RegisterCallback(component, &callback_);

    mouseCallback_.DispatchMouseEvent = DispatchMouseEventCB;
    mouseCallback_.DispatchHoverEvent = nullptr;
    int32_t mouseRet = OH_NativeXComponent_RegisterMouseEventCallback(component, &mouseCallback_);
    int32_t keyRet = OH_NativeXComponent_RegisterKeyEventCallback(component, DispatchKeyEventCB);

    OH_LOG_INFO(LOG_APP, "SpikeB1: XComponent callbacks registered (mouseRet=%{public}d keyRet=%{public}d)", mouseRet,
        keyRet);
    OH_LOG_INFO(LOG_APP, "SpikeB3: touch/mouse/key → native log enabled");
}

bool SpikeRender::PrepareNativeWindow(SpikeSurface& surface, bool forCpu)
{
    if (surface.window == nullptr) {
        OH_LOG_ERROR(LOG_APP, "SpikeB2: PrepareNativeWindow null");
        return false;
    }
    if (!surface.windowReferenced) {
        if (OH_NativeWindow_NativeObjectReference(surface.window) == 0) {
            surface.windowReferenced = true;
        }
    }

    int32_t w = static_cast<int32_t>(surface.width);
    int32_t h = static_cast<int32_t>(surface.height);
    if (w <= 0 || h <= 0) {
        w = 800;
        h = 600;
    }

    int32_t geoRet = OH_NativeWindow_NativeWindowHandleOpt(surface.window, SET_BUFFER_GEOMETRY, w, h);
    int32_t fmtRet = OH_NativeWindow_NativeWindowHandleOpt(
        surface.window, SET_FORMAT, static_cast<int32_t>(NATIVEBUFFER_PIXEL_FMT_RGBA_8888));

    uint64_t usage = 0;
    if (forCpu) {
        usage = NATIVEBUFFER_USAGE_CPU_READ | NATIVEBUFFER_USAGE_CPU_WRITE | NATIVEBUFFER_USAGE_MEM_DMA;
    } else {
        usage = NATIVEBUFFER_USAGE_CPU_READ | NATIVEBUFFER_USAGE_CPU_WRITE | NATIVEBUFFER_USAGE_MEM_DMA |
            NATIVEBUFFER_USAGE_HW_RENDER | NATIVEBUFFER_USAGE_HW_TEXTURE;
    }
    int32_t usageRet = OH_NativeWindow_NativeWindowHandleOpt(surface.window, SET_USAGE, usage);

    OH_LOG_INFO(LOG_APP,
        "SpikeB2: PrepareNativeWindow %{public}s geo=%{public}d fmt=%{public}d usage=%{public}d size=%{public}dx%{public}d",
        forCpu ? "CPU" : "EGL", geoRet, fmtRet, usageRet, w, h);
    surface.windowPrepared = true;
    return true;
}

bool SpikeRender::InitEgl(SpikeSurface& surface)
{
    if (surface.window == nullptr) {
        return false;
    }
    PrepareNativeWindow(surface, false);
    eglBindAPI(EGL_OPENGL_ES_API);

    surface.display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (surface.display == EGL_NO_DISPLAY) {
        OH_LOG_ERROR(LOG_APP, "SpikeB2: eglGetDisplay failed");
        return false;
    }

    EGLint major = 0;
    EGLint minor = 0;
    if (!eglInitialize(surface.display, &major, &minor)) {
        OH_LOG_ERROR(LOG_APP, "SpikeB2: eglInitialize failed");
        return false;
    }

    const EGLint attribs[] = { EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8,
        EGL_ALPHA_SIZE, 8, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_NONE };
    EGLint numConfigs = 0;
    if (!eglChooseConfig(surface.display, attribs, &surface.config, 1, &numConfigs) || numConfigs == 0) {
        OH_LOG_ERROR(LOG_APP, "SpikeB2: eglChooseConfig failed");
        return false;
    }

    auto nativeWin = reinterpret_cast<EGLNativeWindowType>(surface.window);
    surface.eglSurface = eglCreateWindowSurface(surface.display, surface.config, nativeWin, nullptr);
    if (surface.eglSurface == EGL_NO_SURFACE) {
        OH_LOG_ERROR(LOG_APP, "SpikeB2: eglCreateWindowSurface failed err=%{public}d", eglGetError());
        return false;
    }

    const EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
    surface.context = eglCreateContext(surface.display, surface.config, EGL_NO_CONTEXT, contextAttribs);
    if (surface.context == EGL_NO_CONTEXT) {
        OH_LOG_ERROR(LOG_APP, "SpikeB2: eglCreateContext failed");
        return false;
    }

    if (!eglMakeCurrent(surface.display, surface.eglSurface, surface.eglSurface, surface.context)) {
        OH_LOG_ERROR(LOG_APP, "SpikeB2: eglMakeCurrent failed");
        return false;
    }

    glViewport(0, 0, static_cast<GLsizei>(surface.width), static_cast<GLsizei>(surface.height));
    surface.eglReady = true;
    surface.useCpuPath = false;
    surface.swapOkCount = 0;
    surface.swapFailCount = 0;
    surface.colorPhase = -1;
    surface.logged300 = false;
    surface.loggedSwapFail = false;
    OH_LOG_INFO(LOG_APP, "SpikeB2: EGL ready GLES2 %{public}llu x %{public}llu",
        static_cast<unsigned long long>(surface.width), static_cast<unsigned long long>(surface.height));
    return true;
}

void SpikeRender::DestroyEgl(SpikeSurface& surface)
{
    if (surface.display == EGL_NO_DISPLAY) {
        surface.eglReady = false;
        return;
    }
    eglMakeCurrent(surface.display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    if (surface.eglSurface != EGL_NO_SURFACE) {
        eglDestroySurface(surface.display, surface.eglSurface);
        surface.eglSurface = EGL_NO_SURFACE;
    }
    if (surface.context != EGL_NO_CONTEXT) {
        eglDestroyContext(surface.display, surface.context);
        surface.context = EGL_NO_CONTEXT;
    }
    eglTerminate(surface.display);
    surface.display = EGL_NO_DISPLAY;
    surface.eglReady = false;
}

void SpikeRender::ReleaseNativeWindow(SpikeSurface& surface)
{
    if (surface.window != nullptr && surface.windowReferenced) {
        OH_NativeWindow_NativeObjectUnreference(surface.window);
        surface.windowReferenced = false;
    }
    surface.windowPrepared = false;
    surface.window = nullptr;
}

void SpikeRender::SwitchToCpuPath(SpikeSurface& surface)
{
    OH_LOG_WARN(LOG_APP, "SpikeB2: enable CPU NativeWindow present (bypass broken DGLES EGL)");
    DestroyEgl(surface);
    PrepareNativeWindow(surface, true);
    surface.useCpuPath = true;
    surface.swapOkCount = 0;
    surface.swapFailCount = 0;
    surface.colorPhase = -1;
    surface.logged300 = false;
}

bool SpikeRender::DrawCpuFrame(SpikeSurface& surface)
{
    if (surface.window == nullptr) {
        return false;
    }
    NotePhase(surface);
    const uint32_t pixel = PhaseRgba8888(surface.colorPhase < 0 ? 0 : surface.colorPhase);

    OHNativeWindowBuffer* buffer = nullptr;
    int fenceFd = -1;
    int32_t ret = OH_NativeWindow_NativeWindowRequestBuffer(surface.window, &buffer, &fenceFd);
    if (ret != 0 || buffer == nullptr) {
        if (surface.swapFailCount % 60 == 0) {
            OH_LOG_ERROR(LOG_APP, "SpikeB2: RequestBuffer failed ret=%{public}d", ret);
        }
        surface.swapFailCount++;
        return false;
    }

    // Official rule: fenceFd>=0 means GPU still owns buffer — wait, then close.
    if (fenceFd >= 0) {
        pollfd pfd{ fenceFd, POLLIN, 0 };
        (void)poll(&pfd, 1, 3000);
        close(fenceFd);
        fenceFd = -1;
    }

    BufferHandle* handle = OH_NativeWindow_GetBufferHandleFromNative(buffer);
    if (handle == nullptr) {
        if (surface.swapFailCount % 60 == 0) {
            OH_LOG_ERROR(LOG_APP, "SpikeB2: GetBufferHandleFromNative returned null");
        }
        Region empty{ nullptr, 0 };
        OH_NativeWindow_NativeWindowFlushBuffer(surface.window, buffer, -1, empty);
        surface.swapFailCount++;
        return false;
    }

    // Official NativeWindow guide: map via mmap(fd), do NOT assume virAddr is writable.
    if (handle->fd < 0 || handle->size <= 0) {
        if (surface.swapFailCount % 60 == 0) {
            OH_LOG_ERROR(LOG_APP, "SpikeB2: bad BufferHandle fd=%{public}d size=%{public}d vir=%{public}d",
                handle->fd, handle->size, handle->virAddr != nullptr ? 1 : 0);
        }
        Region empty{ nullptr, 0 };
        OH_NativeWindow_NativeWindowFlushBuffer(surface.window, buffer, -1, empty);
        surface.swapFailCount++;
        return false;
    }

    void* mapped = mmap(handle->virAddr, static_cast<size_t>(handle->size), PROT_READ | PROT_WRITE, MAP_SHARED,
        handle->fd, 0);
    if (mapped == MAP_FAILED) {
        // Fallback: map at nullptr if hint address rejected.
        mapped = mmap(nullptr, static_cast<size_t>(handle->size), PROT_READ | PROT_WRITE, MAP_SHARED, handle->fd, 0);
    }
    if (mapped == MAP_FAILED) {
        OH_LOG_ERROR(LOG_APP, "SpikeB2: mmap failed errno=%{public}d fd=%{public}d size=%{public}d", errno, handle->fd,
            handle->size);
        Region empty{ nullptr, 0 };
        OH_NativeWindow_NativeWindowFlushBuffer(surface.window, buffer, -1, empty);
        surface.swapFailCount++;
        return false;
    }

    const int32_t w = handle->width > 0 ? handle->width : static_cast<int32_t>(surface.width);
    const int32_t h = handle->height > 0 ? handle->height : static_cast<int32_t>(surface.height);
    auto* base = reinterpret_cast<uint8_t*>(mapped);

    if (handle->stride >= w * 4) {
        const int32_t strideBytes = handle->stride;
        for (int32_t y = 0; y < h; ++y) {
            auto* row = reinterpret_cast<uint32_t*>(base + static_cast<size_t>(y) * static_cast<size_t>(strideBytes));
            for (int32_t x = 0; x < w; ++x) {
                row[x] = pixel;
            }
        }
    } else {
        const int32_t stridePixels = handle->stride > 0 ? handle->stride : w;
        auto* addr = reinterpret_cast<uint32_t*>(base);
        for (int32_t y = 0; y < h; ++y) {
            for (int32_t x = 0; x < w; ++x) {
                addr[static_cast<size_t>(y) * static_cast<size_t>(stridePixels) + static_cast<size_t>(x)] = pixel;
            }
        }
    }

    if (surface.swapOkCount == 0) {
        OH_LOG_INFO(LOG_APP,
            "SpikeB2: CPU mmap OK w=%{public}d h=%{public}d stride=%{public}d fmt=%{public}d fd=%{public}d size=%{public}d",
            handle->width, handle->height, handle->stride, handle->format, handle->fd, handle->size);
    }

    munmap(mapped, static_cast<size_t>(handle->size));

    Region region{ nullptr, 0 };
    ret = OH_NativeWindow_NativeWindowFlushBuffer(surface.window, buffer, -1, region);
    if (ret != 0) {
        OH_LOG_ERROR(LOG_APP, "SpikeB2: FlushBuffer failed ret=%{public}d", ret);
        surface.swapFailCount++;
        return false;
    }
    NotePresentOk(surface, "CPU");
    return true;
}

void SpikeRender::DrawFrame(SpikeSurface& surface)
{
    if (surface.useCpuPath) {
        DrawCpuFrame(surface);
        return;
    }
    if (!surface.eglReady) {
        return;
    }
    if (!eglMakeCurrent(surface.display, surface.eglSurface, surface.eglSurface, surface.context)) {
        OH_LOG_ERROR(LOG_APP, "SpikeB2: eglMakeCurrent DrawFrame failed");
        return;
    }

    NotePhase(surface);
    ApplyGlPhaseColor(surface.colorPhase < 0 ? 0 : surface.colorPhase);
    glClear(GL_COLOR_BUFFER_BIT);
    glFlush();

    // Emulator may return TRUE while DGLES still logs BAD_SURFACE — treat as soft success only.
    if (!eglSwapBuffers(surface.display, surface.eglSurface)) {
        EGLint err = eglGetError();
        surface.swapFailCount++;
        if (surface.swapFailCount % 60 == 1) {
            OH_LOG_ERROR(LOG_APP, "SpikeB2: eglSwapBuffers FAILED err=0x%{public}x", err);
        }
        if (surface.swapFailCount >= 5) {
            SwitchToCpuPath(surface);
        }
        return;
    }
    NotePresentOk(surface, "EGL");
}

void SpikeRender::StartVSync()
{
    if (vsync_ != nullptr) {
        return;
    }
    const char* name = "SpikeB2VSync";
    vsync_ = OH_NativeVSync_Create(name, static_cast<unsigned int>(strlen(name)));
    if (vsync_ == nullptr) {
        OH_LOG_ERROR(LOG_APP, "SpikeB2: OH_NativeVSync_Create failed");
        return;
    }
    running_.store(true);
    RequestNextVSync();
    OH_LOG_INFO(LOG_APP, "SpikeB2: vsync started");
}

void SpikeRender::RequestNextVSync()
{
    if (running_.load() && appForeground_.load() && vsync_ != nullptr) {
        OH_NativeVSync_RequestFrame(reinterpret_cast<OH_NativeVSync*>(vsync_), VSyncCallback, this);
    }
}

void SpikeRender::StopVSync()
{
    running_.store(false);
    if (vsync_ != nullptr) {
        OH_NativeVSync_Destroy(reinterpret_cast<OH_NativeVSync*>(vsync_));
        vsync_ = nullptr;
        OH_LOG_INFO(LOG_APP, "SpikeB2: vsync stopped");
    }
}

void SpikeRender::OnAppForeground()
{
    appForeground_.store(true);
    OH_LOG_INFO(LOG_APP, "SpikeB1: lifecycle native FOREGROUND — resume present");
    WxOhosApp::Get().OnResume();
    RequestNextVSync();
}

void SpikeRender::OnAppBackground()
{
    appForeground_.store(false);
    OH_LOG_INFO(LOG_APP, "SpikeB1: lifecycle native BACKGROUND — pause present");
    WxOhosApp::Get().OnSuspend();
}

void SpikeRender::OnVSync(long long /*timestamp*/)
{
    if (!running_.load() || !appForeground_.load()) {
        return;
    }
    WxOhosApp::Get().OnMainLoopTick();
    {
        std::lock_guard<std::mutex> lock(mu_);
        auto it = surfaces_.find(activeId_);
        if (it != surfaces_.end()) {
            if (!it->second.useCpuPath && !it->second.eglReady) {
                if (kPreferCpuPresent) {
                    SwitchToCpuPath(it->second);
                } else if (!InitEgl(it->second)) {
                    SwitchToCpuPath(it->second);
                }
            }
            DrawFrame(it->second);
        }
    }
    RequestNextVSync();
}

void SpikeRender::OnSurfaceCreated(OH_NativeXComponent* component, void* window)
{
    if (component == nullptr || window == nullptr) {
        OH_LOG_ERROR(LOG_APP, "SpikeB1: OnSurfaceCreated null args");
        return;
    }

    char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = {};
    uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;
    if (OH_NativeXComponent_GetXComponentId(component, idStr, &idSize) != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        OH_LOG_ERROR(LOG_APP, "SpikeB1: GetXComponentId failed");
        return;
    }

    auto* nativeWindow = reinterpret_cast<OHNativeWindow*>(window);
    uint64_t width = 0;
    uint64_t height = 0;
    OH_NativeXComponent_GetXComponentSize(component, window, &width, &height);

    {
        std::lock_guard<std::mutex> lock(mu_);
        SpikeSurface surface;
        surface.window = nativeWindow;
        surface.width = width;
        surface.height = height;
        if (OH_NativeWindow_NativeObjectReference(surface.window) == 0) {
            surface.windowReferenced = true;
        }
        surfaces_[idStr] = surface;
        activeId_ = idStr;
    }

    OH_LOG_INFO(LOG_APP, "SpikeB1: OHNativeWindow acquired id=%{public}s size=%{public}llu x %{public}llu", idStr,
        static_cast<unsigned long long>(width), static_cast<unsigned long long>(height));
    OH_LOG_INFO(LOG_APP, "SpikeB2: present mode=%{public}s (PC emulator DGLES workaround)",
        kPreferCpuPresent ? "CPU-first" : "EGL-first");

    // B4: wxApp-shaped lifecycle on top of NativeWindow (stub, not real wx).
    WxOhosApp::Get().OnInit();
    WxOhosApp::Get().EnterMainLoop();
    StartVSync();
}

void SpikeRender::OnSurfaceChanged(OH_NativeXComponent* component, void* window)
{
    if (component == nullptr || window == nullptr) {
        return;
    }
    char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = {};
    uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;
    OH_NativeXComponent_GetXComponentId(component, idStr, &idSize);
    uint64_t width = 0;
    uint64_t height = 0;
    OH_NativeXComponent_GetXComponentSize(component, window, &width, &height);

    std::lock_guard<std::mutex> lock(mu_);
    auto it = surfaces_.find(idStr);
    if (it == surfaces_.end()) {
        return;
    }
    it->second.width = width;
    it->second.height = height;
    it->second.window = reinterpret_cast<OHNativeWindow*>(window);
    if (it->second.useCpuPath || it->second.windowPrepared) {
        OH_NativeWindow_NativeWindowHandleOpt(it->second.window, SET_BUFFER_GEOMETRY, static_cast<int>(width),
            static_cast<int>(height));
    }
    if (it->second.eglReady) {
        eglMakeCurrent(it->second.display, it->second.eglSurface, it->second.eglSurface, it->second.context);
        glViewport(0, 0, static_cast<GLsizei>(width), static_cast<GLsizei>(height));
    }
    OH_LOG_INFO(LOG_APP, "SpikeB1: surface changed %{public}s %{public}llu x %{public}llu", idStr,
        static_cast<unsigned long long>(width), static_cast<unsigned long long>(height));
}

void SpikeRender::OnSurfaceDestroyed(OH_NativeXComponent* component, void* /*window*/)
{
    if (component == nullptr) {
        return;
    }
    char idStr[OH_XCOMPONENT_ID_LEN_MAX + 1] = {};
    uint64_t idSize = OH_XCOMPONENT_ID_LEN_MAX + 1;
    OH_NativeXComponent_GetXComponentId(component, idStr, &idSize);

    std::lock_guard<std::mutex> lock(mu_);
    auto it = surfaces_.find(idStr);
    if (it != surfaces_.end()) {
        DestroyEgl(it->second);
        ReleaseNativeWindow(it->second);
        surfaces_.erase(it);
    }
    if (surfaces_.empty()) {
        WxOhosApp::Get().OnExit();
        StopVSync();
        activeId_.clear();
    }
    OH_LOG_INFO(LOG_APP, "SpikeB1: surface destroyed id=%{public}s", idStr);
}

void SpikeRender::DispatchTouchEvent(OH_NativeXComponent* component, void* window)
{
    if (component == nullptr || window == nullptr) {
        return;
    }
    OH_NativeXComponent_TouchEvent touch{};
    if (OH_NativeXComponent_GetTouchEvent(component, window, &touch) != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        OH_LOG_WARN(LOG_APP, "SpikeB3: GetTouchEvent failed");
        return;
    }
    // Throttle MOVE spam; always log DOWN/UP.
    if (touch.type == OH_NATIVEXCOMPONENT_MOVE) {
        static uint64_t moveCount = 0;
        if ((++moveCount % 15) != 0) {
            return;
        }
    }
    OH_LOG_INFO(LOG_APP, "SpikeB3: TOUCH %{public}s id=%{public}d x=%{public}.1f y=%{public}.1f points=%{public}u",
        TouchTypeName(touch.type), touch.id, touch.x, touch.y, touch.numPoints);
}

void SpikeRender::DispatchMouseEvent(OH_NativeXComponent* component, void* window)
{
    if (component == nullptr || window == nullptr) {
        return;
    }
    OH_NativeXComponent_MouseEvent mouse{};
    if (OH_NativeXComponent_GetMouseEvent(component, window, &mouse) != OH_NATIVEXCOMPONENT_RESULT_SUCCESS) {
        OH_LOG_WARN(LOG_APP, "SpikeB3: GetMouseEvent failed");
        return;
    }
    if (mouse.action == OH_NATIVEXCOMPONENT_MOUSE_MOVE) {
        static uint64_t moveCount = 0;
        if ((++moveCount % 20) != 0) {
            return;
        }
    }
    OH_LOG_INFO(LOG_APP, "SpikeB3: MOUSE %{public}s btn=%{public}d x=%{public}.1f y=%{public}.1f",
        MouseActionName(mouse.action), static_cast<int>(mouse.button), mouse.x, mouse.y);
}

void SpikeRender::DispatchKeyEvent(OH_NativeXComponent* component, void* /*window*/)
{
    if (component == nullptr) {
        return;
    }
    OH_NativeXComponent_KeyEvent* keyEvent = nullptr;
    if (OH_NativeXComponent_GetKeyEvent(component, &keyEvent) != OH_NATIVEXCOMPONENT_RESULT_SUCCESS ||
        keyEvent == nullptr) {
        OH_LOG_WARN(LOG_APP, "SpikeB3: GetKeyEvent failed");
        return;
    }
    OH_NativeXComponent_KeyAction action = OH_NATIVEXCOMPONENT_KEY_ACTION_UNKNOWN;
    OH_NativeXComponent_KeyCode code = KEY_UNKNOWN;
    OH_NativeXComponent_GetKeyEventAction(keyEvent, &action);
    OH_NativeXComponent_GetKeyEventCode(keyEvent, &code);
    const char* act = (action == OH_NATIVEXCOMPONENT_KEY_ACTION_DOWN) ? "DOWN"
        : (action == OH_NATIVEXCOMPONENT_KEY_ACTION_UP)                 ? "UP"
                                                                       : "OTHER";
    OH_LOG_INFO(LOG_APP, "SpikeB3: KEY %{public}s code=%{public}d", act, static_cast<int>(code));
}

bool SpikeRender::HasNativeWindow() const
{
    return !surfaces_.empty();
}
