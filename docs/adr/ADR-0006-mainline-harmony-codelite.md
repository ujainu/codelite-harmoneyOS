# ADR-0006：主线纠正为 Harmony CodeLite 交付

## Status

Accepted — 2026-07-22

## Context

Platform Bring-up（Toolchain → NativeWindow → Loop → Input → wxOHOS Skeleton）已证明链路可行。  
若继续以「完善 wxOHOS / Gate 演示」为中心，会偏离客户验收对象。

客户要求摘要：

- 基于 CodeLite 源码做 HarmonyOS PC 适配  
- 功能与 UI 尽量与原版一致，用户功能约 **75%**  
- 同步 Gitee/GitHub，他人可下载构建运行  

## Decision

1. **最终交付物 = Harmony CodeLite**，不是 wxWidgets Demo / Spike。  
2. **当前主线 = Boot CodeLite**：`codelite/` 真实接入编译 → 错误清单 → 按模块消除 → 先求主窗口。  
3. wxOHOS Gate 4.2–4.4 **降为支撑**：仅在编译/链接/运行缺口需要时推进。  
4. **停止**新增 Spike；已迁至 `archive/spikes/`（见 `docs/repo-audit.md`）。  
5. 「75%」按 **用户功能表**（`docs/delivery-mvp.md`）计量。  
6. 发版最低文档：README / BUILD / INSTALL / CHANGELOG / LICENSE；可选 Release zip。  
7. 计划变更必须更新 `docs/roadmap.md` 与 `docs/phase3-wx.md`。

## Consequences

- 工程叙述从「平台可行性」转为「产品级 IDE 移植」  
- 允许早期大量编译错误；以清单驱动，不要求一次全绿  
- Platform 工作未浪费：没有它 CodeLite 无法启动  
