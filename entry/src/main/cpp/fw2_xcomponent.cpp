/////////////////////////////////////////////////////////////////////////////
// FW-2 Host Integration: XComponent → wxOhos_AttachToTopWindow
// I-1: pointer events → wxOhos_DispatchMouseEvent
/////////////////////////////////////////////////////////////////////////////

#include "fw2_xcomponent.h"
#include "fw2_wx_host.h"
#include "fw2_present_probe.h"
#include "fw2_input.h"
#include "fw2_build_bridge.h"
#include "fw2_project_bridge.h"

#include <ace/xcomponent/native_interface_xcomponent.h>
#include <hilog/log.h>
#include <cstdio>

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
    #include "wx/window.h"
#endif

#include "wx/ohos/nativewindow.h"

extern "C" int HarmonyCodeLite_F1_GetOpenFileXrcId();
extern "C" int HarmonyCodeLite_F1_MenuBarOpenFileItemId();
extern "C" int HarmonyCodeLite_F1_BypassProcessCommandOpenFile();

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xF002
#define LOG_TAG "FW2Host"

namespace {

enum OhosPointerAction
{
    kMove = 0,
    kDown = 1,
    kUp = 2,
    kCancel = 3
};

void ForwardMouse(int action, int button, float x, float y)
{
    Fw2_DispatchPointer(action, button, x, y);
}

void OnDispatchKeyEventCB(OH_NativeXComponent* component, void* /*window*/)
{
    if ( !component )
        return;

    OH_NativeXComponent_KeyEvent* keyEvent = nullptr;
    if ( OH_NativeXComponent_GetKeyEvent(component, &keyEvent)
         != OH_NATIVEXCOMPONENT_RESULT_SUCCESS || !keyEvent )
    {
        OH_LOG_WARN(LOG_APP, "[I-7.1] FAIL GetKeyEvent");
        return;
    }

    OH_NativeXComponent_KeyAction action = OH_NATIVEXCOMPONENT_KEY_ACTION_UNKNOWN;
    OH_NativeXComponent_KeyCode code = KEY_UNKNOWN;
    OH_NativeXComponent_GetKeyEventAction(keyEvent, &action);
    OH_NativeXComponent_GetKeyEventCode(keyEvent, &code);

    uint64_t modifierKeys = 0;
    (void)OH_NativeXComponent_GetKeyEventModifierKeyStates(keyEvent, &modifierKeys);

    Fw2_DispatchKey(static_cast<int>(action), static_cast<int>(code), modifierKeys);
}

void OnDispatchTouchEventCB(OH_NativeXComponent* component, void* window)
{
    if ( !component || !window )
        return;

    OH_NativeXComponent_TouchEvent touch{};
    if ( OH_NativeXComponent_GetTouchEvent(component, window, &touch)
         != OH_NATIVEXCOMPONENT_RESULT_SUCCESS )
        return;

    int action = kMove;
    switch ( touch.type )
    {
        case OH_NATIVEXCOMPONENT_DOWN:
            action = kDown;
            break;
        case OH_NATIVEXCOMPONENT_UP:
            action = kUp;
            break;
        case OH_NATIVEXCOMPONENT_MOVE:
            action = kMove;
            break;
        case OH_NATIVEXCOMPONENT_CANCEL:
            action = kCancel;
            break;
        default:
            return;
    }

    if ( action == kMove && !Fw2_ShouldForwardAllMoves() && !Fw2_IsAuiDragActive() )
    {
        static uint64_t moveCount = 0;
        if ( (++moveCount % 4) != 0 )
            return;
    }

    ForwardMouse(action, 0, touch.x, touch.y);
}

void OnDispatchMouseEventCB(OH_NativeXComponent* component, void* window)
{
    if ( !component || !window )
        return;

    OH_NativeXComponent_MouseEvent mouse{};
    if ( OH_NativeXComponent_GetMouseEvent(component, window, &mouse)
         != OH_NATIVEXCOMPONENT_RESULT_SUCCESS )
        return;

    int action = kMove;
    switch ( mouse.action )
    {
        case OH_NATIVEXCOMPONENT_MOUSE_PRESS:
            action = kDown;
            break;
        case OH_NATIVEXCOMPONENT_MOUSE_RELEASE:
            action = kUp;
            break;
        case OH_NATIVEXCOMPONENT_MOUSE_MOVE:
            action = kMove;
            break;
        case OH_NATIVEXCOMPONENT_MOUSE_CANCEL:
            action = kCancel;
            break;
        default:
            return;
    }

    if ( action == kMove && !Fw2_ShouldForwardAllMoves() && !Fw2_IsAuiDragActive() )
    {
        static uint64_t moveCount = 0;
        if ( (++moveCount % 4) != 0 )
            return;
    }

    const int button = static_cast<int>(mouse.button);
    ForwardMouse(action, button, mouse.x, mouse.y);
}

void OnSurfaceCreatedCB(OH_NativeXComponent* component, void* window)
{
    Fw2XComponentBridge::GetInstance()->OnSurfaceCreated(component, window);
}

void OnSurfaceChangedCB(OH_NativeXComponent* component, void* window)
{
    Fw2XComponentBridge::GetInstance()->OnSurfaceChanged(component, window);
}

void OnSurfaceDestroyedCB(OH_NativeXComponent* component, void* window)
{
    Fw2XComponentBridge::GetInstance()->OnSurfaceDestroyed(component, window);
}

} // namespace

Fw2XComponentBridge* Fw2XComponentBridge::GetInstance()
{
    static Fw2XComponentBridge inst;
    return &inst;
}

void Fw2XComponentBridge::SetNativeXComponent(OH_NativeXComponent* component)
{
    component_ = component;
    if ( !component_ )
        return;

    callback_.OnSurfaceCreated = OnSurfaceCreatedCB;
    callback_.OnSurfaceChanged = OnSurfaceChangedCB;
    callback_.OnSurfaceDestroyed = OnSurfaceDestroyedCB;
    callback_.DispatchTouchEvent = OnDispatchTouchEventCB;
    OH_NativeXComponent_RegisterCallback(component_, &callback_);

    mouseCallback_.DispatchMouseEvent = OnDispatchMouseEventCB;
    mouseCallback_.DispatchHoverEvent = nullptr;
    const int32_t mouseRet =
        OH_NativeXComponent_RegisterMouseEventCallback(component_, &mouseCallback_);

    const int32_t keyRet =
        OH_NativeXComponent_RegisterKeyEventCallback(component_, OnDispatchKeyEventCB);

    OH_LOG_INFO(LOG_APP,
                "FW-2 host: XComponent callbacks registered (mouseRet=%{public}d keyRet=%{public}d)",
                mouseRet, keyRet);
    OH_LOG_INFO(LOG_APP, "[I-1] pointer callbacks registered (touch+mouse → wx)");
    OH_LOG_INFO(LOG_APP, "[I-7.1] key callback registered (OHOS key → host log)");
}

void Fw2XComponentBridge::OnSurfaceCreated(OH_NativeXComponent* component, void* window)
{
    static bool surface_boot_in_progress = false;
    if ( surface_boot_in_progress )
    {
        OH_LOG_WARN(LOG_APP, "[FW-2.1] OnSurfaceCreated reentry ignored");
        return;
    }
    surface_boot_in_progress = true;

    if ( !component || !window )
    {
        surface_boot_in_progress = false;
        OH_LOG_ERROR(LOG_APP, "FW-2.1 FAIL OnSurfaceCreated null args");
        return;
    }

    // Tightened evidence FW-2.1
    OH_LOG_INFO(LOG_APP, "[FW-2.1] OnSurfaceCreated surface=%{public}p", window);
    std::fprintf(stderr, "[FW-2.1] OnSurfaceCreated surface=%p\n", window);
    std::fflush(stderr);

    uint64_t width = 0;
    uint64_t height = 0;
    OH_NativeXComponent_GetXComponentSize(component, window, &width, &height);

    if ( !Fw2_EnsureWxReady() )
    {
        OH_LOG_ERROR(LOG_APP, "FW-2.2 FAIL: wx not ready before Attach");
        surface_boot_in_progress = false;
        return;
    }

    // Host owns surface only — never creates wxFrame / Demo UI.
    // Attach always targets wxTheApp->GetTopWindow() (must be clMainFrame; see MV-1).
    wxWindow* top = wxTheApp ? wxTheApp->GetTopWindow() : nullptr;
    OH_LOG_INFO(LOG_APP,
                "[MV-2] Attach begin native=%{public}p top=%{public}p size=%{public}ux%{public}u",
                window, static_cast<void*>(top),
                static_cast<unsigned>(width), static_cast<unsigned>(height));
    std::fprintf(stderr, "[MV-2] Attach begin native=%p top=%p\n",
                 window, static_cast<void*>(top));
    std::fflush(stderr);

    if ( !top || !top->IsTopLevel() )
    {
        OH_LOG_ERROR(LOG_APP, "[MV-2] FAIL no TopLevelWindow for OHNativeWindow Attach");
        surface_boot_in_progress = false;
        return;
    }

    const int ok = wxOhos_AttachToTopWindow(window, static_cast<int>(width),
                                            static_cast<int>(height));
    if ( !ok || !wxOhos_TopWindowHasNativeWindow() )
    {
        OH_LOG_ERROR(LOG_APP, "[MV-2] FAIL AttachToTopWindow/bind (ok=%{public}d hasNative=%{public}d)",
                     ok, wxOhos_TopWindowHasNativeWindow());
        surface_boot_in_progress = false;
        return;
    }
    attached_ = true;

    // 1:1: C bridge attaches ONLY GetTopWindow(); Host never invents a second frame.
    OH_LOG_INFO(LOG_APP,
                "[MV-2] Attach done native=%{public}p top=%{public}p (GetTopWindow only)",
                window, static_cast<void*>(top));
    OH_LOG_INFO(LOG_APP, "[MV-2] OK OHNativeWindow ↔ TopWindow 1:1");

    // R-4: EmbeddedStart may return before clMainFrame SetMenuBar; attach on main thread
    // before first TLW Show so wxOHOS MenuBar Create/Attach logs stay on init thread.
    Fw2_EnsureMainMenuBarAttached();

    // MV-4.1…4.4: compositor chain (NativeWindow → EGL → first pixel).
    // First FAIL only — not EVT_PAINT / not CodeLite draw.
    (void)Fw2_MV4_ProbePresent(window, static_cast<int32_t>(width),
                               static_cast<int32_t>(height));

    // MV-3: Show the official TopWindow only (Host must not know layout).
    top->Show(true);
    // Ensure paint path even if TLW Show short-circuited (P-1 evidence).
    top->Refresh(true);
    top->Update();
    const int shown = top->IsShown() ? 1 : 0;
    int cw = 0, ch = 0;
    top->GetClientSize(&cw, &ch);
    OH_LOG_INFO(LOG_APP, "[MV-3] Show on top=%{public}p IsShown=%{public}d client=%{public}dx%{public}d",
                static_cast<void*>(top), shown, cw, ch);
    std::fprintf(stderr, "[MV-3] Show on top=%p IsShown=%d\n",
                 static_cast<void*>(top), shown);
    std::fflush(stderr);
    if ( !shown )
    {
        OH_LOG_ERROR(LOG_APP, "[MV-3] FAIL Show did not stick");
        surface_boot_in_progress = false;
        return;
    }
    OH_LOG_INFO(LOG_APP, "[MV-3] OK TopWindow Show() (official clMainFrame per MV-1)");
    OH_LOG_INFO(LOG_APP, "[B-7] clMainFrame Shown");

    if ( wxTheApp )
    {
        wxTheApp->CallAfter([]() {
            Fw2_ReattachMainMenuBarOnMainLoop();
            Fw2_RegisterBuildBridge();
            Fw2_FlushMenuBarChrome();
            const int xrcId = HarmonyCodeLite_F1_GetOpenFileXrcId();
            const int menuItemId = HarmonyCodeLite_F1_MenuBarOpenFileItemId();
            OH_LOG_INFO(LOG_APP, "[F-1.EVENT] OK OnFileOpen event table registered");
            OH_LOG_INFO(LOG_APP,
                        "[F-1.ID] XRCID(open_file)=%{public}d menuItemId=%{public}d match=%{public}d",
                        xrcId, menuItemId, xrcId == menuItemId ? 1 : 0);
            // F-8.x / F-5.6.5 menu+project probes defer until MainLoop idle (BOOT-2.2).
            // Second LoadMenuBar from CallAfter overflowed worker stack pre-idle.
        });
    }

    // P-3 probe runs inside wx TLW Show (not Host): HAP binxo strips unused
    // exports from libwx; a Host U-ref to wxOhos_P3_* caused dlopen failure.

    // BOOT-001: OnRun/MainLoop on dedicated thread (Host lifecycle only).
    (void)Fw2_EmbeddedRun();
    surface_boot_in_progress = false;
}

void Fw2XComponentBridge::OnSurfaceChanged(OH_NativeXComponent* /*component*/, void* /*window*/)
{
    // No paint / no resize present in FW-2 scope.
}

void Fw2XComponentBridge::OnSurfaceDestroyed(OH_NativeXComponent* /*component*/, void* /*window*/)
{
    if ( attached_ )
    {
        wxOhos_DetachFromTopWindow();
        attached_ = false;
        OH_LOG_INFO(LOG_APP, "FW-2: DetachFromTopWindow on surface destroy");
    }
}
