#ifndef WX_OHOS_PROTO_H
#define WX_OHOS_PROTO_H

/**
 * B4: minimal wxApp-shaped platform stub — NOT real wxWidgets.
 * Proves: OnInit → MainLoop → OnExit mapping onto Spike lifecycle.
 */
class WxOhosApp {
public:
    static WxOhosApp& Get();

    /** Call once NativeWindow is acquired (≈ wxApp::OnInit). */
    bool OnInit();

    /** Enter event/present loop (≈ wxApp::MainLoop). Idempotent. */
    void EnterMainLoop();

    /** One loop iteration from vsync (throttled hilog). */
    void OnMainLoopTick();

    /** Ability background / foreground (optional pause semantics). */
    void OnSuspend();
    void OnResume();

    /** Surface/Ability teardown (≈ wxApp::OnExit). */
    void OnExit();

    bool IsInMainLoop() const { return inMainLoop_; }
    bool DidInit() const { return didInit_; }

private:
    WxOhosApp() = default;

    bool didInit_ = false;
    bool inMainLoop_ = false;
    bool exited_ = false;
    unsigned tickLogCounter_ = 0;
};

#endif
