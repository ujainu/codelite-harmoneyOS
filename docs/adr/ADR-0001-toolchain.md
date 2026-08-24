# ADR-0001：采用 DevEco Native Toolchain 作为构建基线

## Status

Accepted — 2026-07-17

## Context

CodeLite 适配需要可复现的 C/C++ 交叉编译与设备运行验证。本机 `$HOME/Library/Huawei/Sdk` 仅有系统镜像，编译器在 DevEco 自带 `openharmony/native`。

## Decision

以 DevEco 内置 OHOS Native SDK（Clang 15.0.4、`ohos.toolchain.cmake`）为准，封装为 `toolchain/harmonyos-pc.cmake` + `env.sh`。Phase 2 Exit Criteria：Clang、hello 编译、MateBook Pro 运行、CMake 最小工程、toolchain 文件。

## Alternatives

- 仅使用镜像目录（无编译器）→ 不可行  
- 自建无关工具链 → 与设备 ABI/sysroot 不一致风险高  

## Consequences

- Toolchain / CMake / Native Risk → 🟢  
- 后续所有 native/Spike/wx_base 构建统一走该 toolchain  
