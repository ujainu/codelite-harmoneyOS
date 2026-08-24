#include "fw2_xcomponent.h"
#include "fw2_wx_host.h"
#include "fw2_filepicker.h"
#include "fw2_process_probe.h"
#include "fw2_toolchain_probe.h"
#include "fw2_process_output_probe.h"
#include "fw2_compiler_probe.h"
#include "fw2_runner_probe.h"
#include "fw2_build_probe.h"
#include "fw2_project_bridge.h"
#include "fw2_project_dialog.h"
#include "fw2_input.h"

extern "C" int HarmonyCodeLite_F3_OpenWorkspacePath(const char* path);

#include <ace/xcomponent/native_interface_xcomponent.h>
#include <hilog/log.h>
#include <napi/native_api.h>

#include <cstring>

#include "wx/ohos/nativewindow.h"

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xF002
#define LOG_TAG "FW2Host"

static napi_value Fw2Ping(napi_env env, napi_callback_info /*info*/)
{
    OH_LOG_INFO(LOG_APP, "FW-2 host: native lib loaded (fw2Ping)");
    napi_value result;
    const char* msg = "FW2 host native OK";
    napi_create_string_utf8(env, msg, NAPI_AUTO_LENGTH, &result);
    return result;
}

static napi_value Fw2Lifecycle(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1] = { nullptr };
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    if ( argc < 1 )
        return nullptr;

    char buf[32] = {};
    size_t len = 0;
    napi_get_value_string_utf8(env, args[0], buf, sizeof(buf), &len);
    if ( std::strcmp(buf, "destroy") == 0 )
    {
        wxOhos_DetachFromTopWindow();
        Fw2_ShutdownWx();
    }
    return nullptr;
}

// B4-002: ArkTS Deployment tells native where share/codelite was landed.
static napi_value Fw2SetInstallDir(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1] = { nullptr };
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    if ( argc < 1 )
        return nullptr;

    char buf[1024] = {};
    size_t len = 0;
    napi_get_value_string_utf8(env, args[0], buf, sizeof(buf), &len);
    Fw2_SetInstallDir(buf);
    Fw2_ProbeProcess();
    Fw2_ProbeToolchain();
    Fw2_ProbeProcessOutput();
    Fw2_ProbeCompilerBackend();
    Fw2_ProbeRunnerBackend();
    Fw2_RunBuildProbe();
    return nullptr;
}

static napi_value Fw2OpenWorkspacePath(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1] = { nullptr };
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    if ( argc < 1 )
        return nullptr;

    char buf[4096] = {};
    size_t len = 0;
    napi_get_value_string_utf8(env, args[0], buf, sizeof(buf), &len);
    const int rc = HarmonyCodeLite_F3_OpenWorkspacePath(buf);
    napi_value result;
    napi_create_int32(env, rc, &result);
    return result;
}

static napi_value Fw2NewHarmonyConsoleProject(napi_env env, napi_callback_info /*info*/)
{
    const int rc = Fw2_NewHarmonyConsoleProject();
    napi_value result;
    napi_create_int32(env, rc, &result);
    return result;
}

static napi_value Fw2CreateHarmonyProject(napi_env env, napi_callback_info info)
{
    size_t argc = 3;
    napi_value args[3] = {nullptr, nullptr, nullptr};
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    if ( argc < 2 )
        return nullptr;

    char name[256] = {};
    char location[4096] = {};
    size_t nameLen = 0;
    size_t locLen = 0;
    napi_get_value_string_utf8(env, args[0], name, sizeof(name), &nameLen);
    napi_get_value_string_utf8(env, args[1], location, sizeof(location), &locLen);

    int templateIndex = 0;
    if ( argc >= 3 )
    {
        napi_get_value_int32(env, args[2], &templateIndex);
    }

    HarmonyProjectCreateParams params;
    params.name = name;
    params.location = location;
    params.templ = templateIndex == 1 ? HarmonyProjectTemplate::HarmonyConsole : HarmonyProjectTemplate::CppConsole;
    params.openEditor = true;
    params.buildAfterCreate = true;

    const int rc = Fw2_CreateHarmonyProject(params);
    napi_value result;
    napi_create_int32(env, rc, &result);
    return result;
}

/**
 * ArkTS → C++ pointer dispatch fallback.
 * The OHOS native XComponent mouse/touch callbacks are not always reliably
 * fired in 6.1.1 TEXTURE mode (ArkUI hit test may swallow them). This NAPI
 * bridge gives Index.ets an explicit way to forward every
 * Touch/Mouse event it sees — bypassing the native callback layer entirely.
 *
 * ArkTS maps:
 *   TouchType.Down   → action=1  (kDown)
 *   TouchType.Up     → action=2  (kUp)
 *   TouchType.Move   → action=0  (kMove)
 *   TouchType.Cancel → action=3  (kCancel)
 *   MouseButton.Left/Right/Middle → button=0/1/2 (same as wxNormalizeButton)
 */
static napi_value Fw2DispatchPointer(napi_env env, napi_callback_info info)
{
    size_t argc = 4;
    napi_value args[4] = { nullptr, nullptr, nullptr, nullptr };
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    if ( argc < 3 )
        return nullptr;

    int32_t action = 0;
    int32_t button = 0;
    double x = 0.0;
    double y = 0.0;
    napi_get_value_int32(env, args[0], &action);
    napi_get_value_int32(env, args[1], &button);
    napi_get_value_double(env, args[2], &x);
    if ( argc >= 4 )
        napi_get_value_double(env, args[3], &y);

    OH_LOG_INFO(LOG_APP,
                "[I-1.NAPI] Fw2DispatchPointer action=%{public}d button=%{public}d x=%{public}f y=%{public}f",
                action, button, x, y);

    Fw2_DispatchPointer(action, button, static_cast<float>(x), static_cast<float>(y));
    return nullptr;
}

/** ArkTS wheel dispatch fallback (same reasoning as Fw2DispatchPointer). */
static napi_value Fw2DispatchWheel(napi_env env, napi_callback_info info)
{
    size_t argc = 3;
    napi_value args[3] = { nullptr, nullptr, nullptr };
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    if ( argc < 1 )
        return nullptr;

    int32_t rotation = 0;
    double x = 0.0;
    double y = 0.0;
    napi_get_value_int32(env, args[0], &rotation);
    if ( argc >= 2 )
        napi_get_value_double(env, args[1], &x);
    if ( argc >= 3 )
        napi_get_value_double(env, args[2], &y);

    Fw2_DispatchWheel(rotation, static_cast<float>(x), static_cast<float>(y));
    return nullptr;
}

static napi_value Init(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        { "fw2Ping", nullptr, Fw2Ping, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "fw2Lifecycle", nullptr, Fw2Lifecycle, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "fw2SetInstallDir", nullptr, Fw2SetInstallDir, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "fw2OpenWorkspacePath", nullptr, Fw2OpenWorkspacePath, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "fw2NewHarmonyConsoleProject", nullptr, Fw2NewHarmonyConsoleProject, nullptr, nullptr, nullptr,
            napi_default, nullptr },
        { "fw2CreateHarmonyProject", nullptr, Fw2CreateHarmonyProject, nullptr, nullptr, nullptr, napi_default,
            nullptr },
        { "fw2DispatchPointer", nullptr, Fw2DispatchPointer, nullptr, nullptr, nullptr, napi_default, nullptr },
        { "fw2DispatchWheel", nullptr, Fw2DispatchWheel, nullptr, nullptr, nullptr, napi_default, nullptr },
    };
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);

    Fw2_FilePicker_Init(env, exports);

    napi_value exportInstance = nullptr;
    if ( napi_get_named_property(env, exports, OH_NATIVE_XCOMPONENT_OBJ, &exportInstance) != napi_ok )
    {
        OH_LOG_WARN(LOG_APP, "FW-2 host: OH_NATIVE_XCOMPONENT_OBJ not present yet");
        return exports;
    }

    OH_NativeXComponent* nativeXComponent = nullptr;
    if ( napi_unwrap(env, exportInstance, reinterpret_cast<void**>(&nativeXComponent)) != napi_ok )
    {
        OH_LOG_ERROR(LOG_APP, "FW-2 host: napi_unwrap XComponent failed");
        return exports;
    }

    Fw2XComponentBridge::GetInstance()->SetNativeXComponent(nativeXComponent);
    return exports;
}

static napi_module demoModule = {
    .nm_version = 1,
    .nm_flags = 0,
    .nm_filename = nullptr,
    .nm_register_func = Init,
    .nm_modname = "entry",
    .nm_priv = nullptr,
    .reserved = { 0 },
};

extern "C" __attribute__((constructor)) void RegisterEntryModule(void)
{
    napi_module_register(&demoModule);
    OH_LOG_INFO(LOG_APP, "FW-2 host: napi module registered");
}
