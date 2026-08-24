#ifndef SPIKE_RENDER_H
#define SPIKE_RENDER_H

#include <ace/xcomponent/native_interface_xcomponent.h>
#include <native_window/external_window.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <unordered_map>

struct SpikeSurface {
    OHNativeWindow* window = nullptr;
    uint64_t width = 0;
    uint64_t height = 0;
    bool windowPrepared = false;
    bool windowReferenced = false;

    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface eglSurface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    EGLConfig config = nullptr;

    bool eglReady = false;
    bool useCpuPath = false;
    uint64_t swapOkCount = 0;
    uint64_t swapFailCount = 0;
    int colorPhase = 0;
    bool logged300 = false;
    bool loggedSwapFail = false;
};

class SpikeRender {
public:
    static SpikeRender* GetInstance();

    void SetNativeXComponent(OH_NativeXComponent* component);
    void OnSurfaceCreated(OH_NativeXComponent* component, void* window);
    void OnSurfaceChanged(OH_NativeXComponent* component, void* window);
    void OnSurfaceDestroyed(OH_NativeXComponent* component, void* window);
    void DispatchTouchEvent(OH_NativeXComponent* component, void* window);
    void DispatchMouseEvent(OH_NativeXComponent* component, void* window);
    void DispatchKeyEvent(OH_NativeXComponent* component, void* window);

    // Ability lifecycle → pause/resume present loop (B1 Lifecycle Gate).
    void OnAppForeground();
    void OnAppBackground();

    bool HasNativeWindow() const;
    void OnVSync(long long timestamp);

private:
    SpikeRender() = default;

    bool PrepareNativeWindow(SpikeSurface& surface, bool forCpu);
    bool InitEgl(SpikeSurface& surface);
    void DestroyEgl(SpikeSurface& surface);
    void ReleaseNativeWindow(SpikeSurface& surface);
    void DrawFrame(SpikeSurface& surface);
    bool DrawCpuFrame(SpikeSurface& surface);
    void SwitchToCpuPath(SpikeSurface& surface);
    void StartVSync();
    void StopVSync();
    void RequestNextVSync();
    static void VSyncCallback(long long timestamp, void* data);

    std::mutex mu_;
    std::unordered_map<std::string, SpikeSurface> surfaces_;
    std::string activeId_;
    OH_NativeXComponent_Callback callback_{};
    OH_NativeXComponent_MouseEvent_Callback mouseCallback_{};
    void* vsync_ = nullptr;
    std::atomic<bool> running_{false};
    std::atomic<bool> appForeground_{true};
};

#endif
