/////////////////////////////////////////////////////////////////////////////
// F-1.4 OHOS DocumentPicker bridge (ArkTS UI thread ↔ wx worker thread)
/////////////////////////////////////////////////////////////////////////////

#include "fw2_filepicker.h"

#include <hilog/log.h>
#include <napi/native_api.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <string>

extern "C" void HarmonyCodeLite_F1_RegisterPickOpenFileFn(
    int (*fn)(const char*, int, char*, int, char*, int));

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xF002
#define LOG_TAG "FW2Host"

namespace {

struct PickerSyncState
{
    std::mutex mtx;
    std::condition_variable cv;
    bool done = false;
    int result = 0; // 1 ok, 0 cancel, -1 error
    std::string uri;
    std::string path;
};

PickerSyncState g_pickerState;
napi_env g_pickerEnv = nullptr;
napi_ref g_pickerRunnerRef = nullptr;
napi_threadsafe_function g_pickerTsfn = nullptr;
std::atomic<bool> g_pickerBridgeReady{false};

void ResetPickerStateLocked(PickerSyncState& st)
{
    st.done = false;
    st.result = 0;
    st.uri.clear();
    st.path.clear();
}

void CallJsPicker(napi_env env, napi_value /*js_cb*/, void* /*context*/, void* /*data*/)
{
    if ( !g_pickerRunnerRef )
    {
        OH_LOG_ERROR(LOG_APP, "[F-1.4] picker runner not installed");
        std::lock_guard<std::mutex> lock(g_pickerState.mtx);
        g_pickerState.result = -1;
        g_pickerState.done = true;
        g_pickerState.cv.notify_one();
        return;
    }

    napi_value runner = nullptr;
    if ( napi_get_reference_value(env, g_pickerRunnerRef, &runner) != napi_ok || !runner )
    {
        OH_LOG_ERROR(LOG_APP, "[F-1.4] picker runner ref invalid");
        std::lock_guard<std::mutex> lock(g_pickerState.mtx);
        g_pickerState.result = -1;
        g_pickerState.done = true;
        g_pickerState.cv.notify_one();
        return;
    }

    OH_LOG_INFO(LOG_APP, "[F-1.4] picker opened");

    napi_call_function(env, nullptr, runner, 0, nullptr, nullptr);
    // Async runner completes via fw2PickerDeliver().
}

static napi_value Fw2InstallFilePicker(napi_env env, napi_callback_info info)
{
    size_t argc = 1;
    napi_value args[1] = { nullptr };
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);
    if ( argc < 1 )
        return nullptr;

    napi_valuetype ty = napi_undefined;
    napi_typeof(env, args[0], &ty);
    if ( ty != napi_function )
    {
        OH_LOG_ERROR(LOG_APP, "[F-1.4] fw2InstallFilePicker expects function");
        return nullptr;
    }

    g_pickerEnv = env;
    if ( g_pickerRunnerRef )
    {
        napi_delete_reference(env, g_pickerRunnerRef);
        g_pickerRunnerRef = nullptr;
    }
    napi_create_reference(env, args[0], 1, &g_pickerRunnerRef);

    if ( !g_pickerTsfn )
    {
        napi_value resourceName = nullptr;
        napi_create_string_utf8(env, "Fw2FilePicker", NAPI_AUTO_LENGTH, &resourceName);
        napi_create_threadsafe_function(env,
                                        args[0],
                                        nullptr,
                                        resourceName,
                                        0,
                                        1,
                                        nullptr,
                                        nullptr,
                                        nullptr,
                                        CallJsPicker,
                                        &g_pickerTsfn);
    }

    g_pickerBridgeReady = true;
    OH_LOG_INFO(LOG_APP, "[F-1.4] picker bridge installed");
    return nullptr;
}

static napi_value Fw2PickerDeliver(napi_env env, napi_callback_info info)
{
    size_t argc = 2;
    napi_value args[2] = { nullptr, nullptr };
    napi_get_cb_info(env, info, &argc, args, nullptr, nullptr);

    std::string uri;
    std::string path;
    bool cancelled = true;

    if ( argc >= 1 )
    {
        napi_valuetype ty = napi_undefined;
        napi_typeof(env, args[0], &ty);
        if ( ty == napi_string )
        {
            char buf[2048] = {};
            size_t len = 0;
            napi_get_value_string_utf8(env, args[0], buf, sizeof(buf), &len);
            uri.assign(buf, len);
            cancelled = uri.empty();
        }
    }

    if ( argc >= 2 )
    {
        napi_valuetype ty = napi_undefined;
        napi_typeof(env, args[1], &ty);
        if ( ty == napi_string )
        {
            char buf[4096] = {};
            size_t len = 0;
            napi_get_value_string_utf8(env, args[1], buf, sizeof(buf), &len);
            path.assign(buf, len);
            if ( !path.empty() )
                cancelled = false;
        }
    }

    OH_LOG_INFO(LOG_APP, "[F-1.4] uri=%{public}s", uri.empty() ? "(none)" : uri.c_str());
    OH_LOG_INFO(LOG_APP, "[F-1.4] path=%{public}s", path.empty() ? "(none)" : path.c_str());

    {
        std::lock_guard<std::mutex> lock(g_pickerState.mtx);
        g_pickerState.uri = uri;
        g_pickerState.path = path;
        g_pickerState.result = cancelled ? 0 : 1;
        g_pickerState.done = true;
        if ( g_pickerState.result == 1 )
            OH_LOG_INFO(LOG_APP, "[F-1.4] return wxID_OK");
    }
    g_pickerState.cv.notify_one();
    return nullptr;
}

int Fw2_PickOpenFileSync(const char* /*defaultDir*/,
                         int /*allowMultiple*/,
                         char* outPath,
                         int outPathSize,
                         char* outUri,
                         int outUriSize)
{
    if ( !g_pickerBridgeReady || !g_pickerTsfn )
    {
        OH_LOG_ERROR(LOG_APP, "[F-1.4] picker bridge not ready");
        return -1;
    }

    {
        std::lock_guard<std::mutex> lock(g_pickerState.mtx);
        ResetPickerStateLocked(g_pickerState);
    }

    const napi_status st = napi_call_threadsafe_function(g_pickerTsfn, nullptr, napi_tsfn_nonblocking);
    if ( st != napi_ok )
    {
        OH_LOG_ERROR(LOG_APP, "[F-1.4] tsfn dispatch failed status=%{public}d", static_cast<int>(st));
        return -1;
    }

    std::unique_lock<std::mutex> lock(g_pickerState.mtx);
    const bool ok = g_pickerState.cv.wait_for(
        lock, std::chrono::seconds(120), [] { return g_pickerState.done; });
    if ( !ok )
    {
        OH_LOG_ERROR(LOG_APP, "[F-1.4] picker timed out");
        return -1;
    }

    if ( g_pickerState.result != 1 )
        return g_pickerState.result;

    if ( outUri && outUriSize > 0 )
    {
        std::strncpy(outUri, g_pickerState.uri.c_str(), static_cast<size_t>(outUriSize - 1));
        outUri[outUriSize - 1] = '\0';
    }
    if ( outPath && outPathSize > 0 )
    {
        std::strncpy(outPath, g_pickerState.path.c_str(), static_cast<size_t>(outPathSize - 1));
        outPath[outPathSize - 1] = '\0';
    }
    return 1;
}

} // namespace

void Fw2_FilePicker_RegisterWxBridge()
{
    HarmonyCodeLite_F1_RegisterPickOpenFileFn(Fw2_PickOpenFileSync);
    OH_LOG_INFO(LOG_APP, "[F-1.4] HarmonyCodeLite_F1_RegisterPickOpenFileFn OK");
}

void Fw2_FilePicker_Init(napi_env env, napi_value exports)
{
    napi_property_descriptor desc[] = {
        { "fw2InstallFilePicker", nullptr, Fw2InstallFilePicker, nullptr, nullptr, nullptr,
          napi_default, nullptr },
        { "fw2PickerDeliver", nullptr, Fw2PickerDeliver, nullptr, nullptr, nullptr, napi_default,
          nullptr },
    };
    napi_define_properties(env, exports, sizeof(desc) / sizeof(desc[0]), desc);
    Fw2_FilePicker_RegisterWxBridge();
}
