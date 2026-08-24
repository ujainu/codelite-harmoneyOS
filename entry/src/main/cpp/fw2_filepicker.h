#pragma once

struct napi_env__; using napi_env = napi_env__*;
struct napi_value__; using napi_value = napi_value__*;

// F-1.4: Register OHOS DocumentViewPicker bridge (ArkTS → native → CodeLite).
void Fw2_FilePicker_Init(napi_env env, napi_value exports);
void Fw2_FilePicker_RegisterWxBridge();
