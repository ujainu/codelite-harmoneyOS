# ADR-0005：引入 wxOHOS 正式平台层

## Status

Accepted — 2026-07-21

## Context

Spike 已证明 NativeWindow / 输入 / 生命周期可映射到最小平台抽象。  
工程需要让 **wxWidgets 将 HarmonyOS 视为正式 toolkit**，而不是长期挂在 `spikes/`。

本树现有端口均为 `src/<toolkit>/` + `include/wx/<toolkit>/`（msw/gtk/dfb/…），无 `src/os/` / `src/port/` 惯例。

## Decision

1. **HarmonyOS 作为 wxWidgets 正式平台实现**，目录：
   - `third_party/wxWidgets/src/ohos/`
   - `third_party/wxWidgets/include/wx/ohos/`
2. CMake：`wxBUILD_TOOLKIT=ohos` → `WXOHOS` / `__WXOHOS__`（见 `build/cmake/toolkit.cmake`、`files.cmake`、`lib/core/CMakeLists.txt`）。
3. Phase 4 第一刀骨架类（**真实** Compile/Link/Run，非空目录）：
   - `wxOHOSApp` / `wxOHOSEventLoop` / `wxOHOSWindow` / `wxOHOSDisplay` / `wxOHOSTimer`
4. **继承与改动边界（评审强制）**：
   ```text
   wxAppBase          ← 尽量不改 Core；不要过早深入修改
       │
       ▼
   wxOHOSApp          ← 新增平台实现
       │
       ▼
   wxOHOSEventLoop → Harmony Native
   ```
5. 与 Spike 对应：`archive/spikes/b4-wx-proto/MAPPING.md`；实现以 **ohos/** 为准。  
6. 必须改上游时 → `patches/wxwidgets/*.patch`（可 `git apply` 升级）。  
7. 公共头 `wx/app.h` 的 `#elif __WXOHOS__`：**待** 平台 `wxApp` 就绪再挂，Gate 4.1 不改 Core 派发。

## Phase 4 Gates

| Gate | 内容 | 状态 |
|------|------|------|
| 4.1 Skeleton | Compile → Link → Run（真实验证） | ✅ |
| 4.2 Window | Create + OHNativeWindow + Show/Hide/Destroy + **100× 压力** | ▶ |
| 4.3A Surface | Present / Swap / Resize | 未开始 |
| 4.3B DC | wxPaintDC → wxOHOSDC（勿跳过 4.3A） | 未开始 |
| 4.4 Event | 见 `docs/event-mapping.md` | 规格初稿 |

## Consequences

- 目标从「能不能跑」转为「wxOHOS 长期可维护」  
- 成功率粗估 ~85%  
- Patch：一问题一文件、可独立 apply（`patches/wxwidgets/`）  
- Smoke：`wxohos_skel_smoke` 已验证；完整 `wxcore` 随 Gate 推进
