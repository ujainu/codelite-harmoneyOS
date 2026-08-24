#pragma once

#include <ace/xcomponent/native_interface_xcomponent.h>

class Fw2XComponentBridge
{
public:
    static Fw2XComponentBridge* GetInstance();

    void SetNativeXComponent(OH_NativeXComponent* component);

    void OnSurfaceCreated(OH_NativeXComponent* component, void* window);
    void OnSurfaceChanged(OH_NativeXComponent* component, void* window);
    void OnSurfaceDestroyed(OH_NativeXComponent* component, void* window);

private:
    Fw2XComponentBridge() = default;

    OH_NativeXComponent* component_ = nullptr;
    OH_NativeXComponent_Callback callback_ {};
    OH_NativeXComponent_MouseEvent_Callback mouseCallback_ {};
    bool attached_ = false;
};
