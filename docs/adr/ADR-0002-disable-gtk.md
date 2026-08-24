# ADR-0002：关闭 GTK3（及 X11/Wayland/SDL）GUI 路径

## Status

Accepted — 2026-07-17（证据），文档固化 2026-07-21

## Context

wxWidgets 在 UNIX 默认 `gtk3`。对 OHOS 交叉 Configure 时：`Could NOT find GTK3`。sysroot 扫描确认无 GTK/X11/Wayland/SDL。

## Decision

**Do NOT continue GTK path.** Linux 默认 GUI 路线在 HarmonyOS PC 上不存在，禁止继续投入 GTK 移植。

## Alternatives

- 自带/编译 GTK 到 OHOS → 工作量巨大且非官方桌面栈，否决  
- 继续重试 pkg-config → 无意义  

## Consequences

- 最大风险从「编译器」转移到 **GUI Backend**（🔴）  
- 必须另寻 ArkUI / XComponent / NativeWindow / EGL 路径  
- 详见 `archive/docs/gui-backend-investigation.md`  
