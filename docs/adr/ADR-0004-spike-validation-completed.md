# ADR-0004：Spike 验证结束，停止继续 Spike

## Status

Accepted — 2026-07-21

## Context

B1–B4 已证明：Toolchain、NativeWindow、Ability/Native 生命周期、Input、最小 `wxApp` 形生命周期（OnInit → MainLoop → OnExit）。  
最大技术不确定性（「鸿蒙 PC 能否承载 Native GUI 路线」）已基本消除。

继续 `Spike5/6/7` 边际收益下降，且会把正式平台实现困在 `spikes/` 目录。

## Decision

**停止新增 GUI Spike。**  
`archive/spikes/gui-b` / `archive/spikes/b4-wx-proto` 保留为证据与映射参考，不再作为主线开发载体。

Phase 3（Platform Feasibility）收尾；其后主线经 ADR-0006 纠正为 **Boot CodeLite / Harmony CodeLite 交付**（wxOHOS 为支撑，非终点）。


## Consequences

- 新代码落在 `third_party/wxWidgets/{src,include/wx}/ohos/`  
- Spike 仅作回归对照，不扩展功能面  
- B2 EGL 真机验证保留为 **并行 Known Limitation**，不阻塞 Phase 4
