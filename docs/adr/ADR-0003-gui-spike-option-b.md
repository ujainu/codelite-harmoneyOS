# ADR-0003：GUI 验证方向选用 Option B（Spike）

## Status

Accepted as **Spike Direction** — 2026-07-21  
**Not** “final architecture selection”.

## Context

GTK 已否决；无官方 wxOHOS。需在投入 CodeLite/wx port 前证明鸿蒙 PC 能否稳定提供 Native 渲染表面与生命周期。

## Decision

采用 **Option B：XComponent / NativeWindow 最小 GUI Spike**（技术验证，非正式架构）。

B1 验收目标（HarmonyOS 原生模型）：

> 成功获得 `OHNativeWindow`，并确认 Native 渲染表面可见。  
> （不是 Win32 式 `CreateWindow()`。）

拆分：B1 表面 → B2 `glClear` 变色 → B3 输入事件 → B4 wx Platform Layer Prototype。

**STOP GATE**：B1 无法建立可行路径 → 暂停 wx GUI 适配与 CodeLite UI 移植，重估方案。

## Alternatives

| 选项 | 结论 |
|------|------|
| A 自研 wxOHOS | 过早；缺「可承载」前提 |
| C ArkUI 重写 IDE | 最后手段；等于换产品 |
| E 长期不做 GUI | 延后必返工 |

## Consequences

- 成功率粗估：B 成功 → ~80%；B 失败 → 重估是否坚持 wx  
- B 通过前禁止 Phase 4 CodeLite GUI 移植  
- A 仅在 B1–B4 证明可承载后评估  
- 工程：`archive/spikes/gui-b/`（已归档，非交付主体）  
