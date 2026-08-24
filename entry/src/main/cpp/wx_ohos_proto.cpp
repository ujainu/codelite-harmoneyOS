#include "wx_ohos_proto.h"

#include <hilog/log.h>

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xB001
#define LOG_TAG "SpikeB4"

WxOhosApp& WxOhosApp::Get()
{
    static WxOhosApp instance;
    return instance;
}

bool WxOhosApp::OnInit()
{
    if (didInit_) {
        return true;
    }
    didInit_ = true;
    exited_ = false;
    OH_LOG_INFO(LOG_APP, "SpikeB4: OnInit OK (wxApp-shaped stub, not real wxWidgets)");
    return true;
}

void WxOhosApp::EnterMainLoop()
{
    if (!didInit_) {
        OH_LOG_WARN(LOG_APP, "SpikeB4: EnterMainLoop before OnInit — calling OnInit");
        OnInit();
    }
    if (inMainLoop_) {
        return;
    }
    inMainLoop_ = true;
    tickLogCounter_ = 0;
    OH_LOG_INFO(LOG_APP, "SpikeB4: MainLoop enter");
}

void WxOhosApp::OnMainLoopTick()
{
    if (!inMainLoop_ || exited_) {
        return;
    }
    // ~1 Hz at 60fps — prove loop is alive without flooding hilog.
    if ((++tickLogCounter_ % 60) == 1) {
        OH_LOG_INFO(LOG_APP, "SpikeB4: MainLoop tick n=%{public}u", tickLogCounter_);
    }
}

void WxOhosApp::OnSuspend()
{
    OH_LOG_INFO(LOG_APP, "SpikeB4: MainLoop suspend (background)");
}

void WxOhosApp::OnResume()
{
    OH_LOG_INFO(LOG_APP, "SpikeB4: MainLoop resume (foreground)");
}

void WxOhosApp::OnExit()
{
    if (exited_) {
        return;
    }
    exited_ = true;
    if (inMainLoop_) {
        OH_LOG_INFO(LOG_APP, "SpikeB4: MainLoop leave");
        inMainLoop_ = false;
    }
    OH_LOG_INFO(LOG_APP, "SpikeB4: OnExit OK");
    didInit_ = false;
}
