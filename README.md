# Harmony CodeLite（HarmonyOS PC）

将开源 IDE [CodeLite](https://github.com/eranif/codelite) 移植到 **HarmonyOS PC**。

> **交付物是 Harmony CodeLite**，不是 wx Demo / GUI Spike。

```text
git clone → cmake → ninja → CodeLite
```

## 验收目标

用户功能约 **75%**（见 [`docs/delivery-mvp.md`](docs/delivery-mvp.md) / [`docs/feature-matrix.md`](docs/feature-matrix.md)），可按文档复现构建。

```text
Harmony CodeLite  ← 验收对象
      ↓
codelite/
      ↓
wxWidgets + wxOHOS（手段，按编译缺口补）
      ↓
HarmonyOS PC
```

## 当前主线（V2.8 · Boot）

| Gate | 状态 |
|------|------|
| 5A Configure | ✅ |
| 5B Compile | 🟡 |
| 5C Link | ⬜ |
| 5D Boot | ⬜ |

验证门禁：[`docs/verification.md`](docs/verification.md) · `./scripts/verify-boot.sh`  
计划：[`docs/roadmap.md`](docs/roadmap.md)（V2.8 结构稳定，勿频繁改）

## 仓库布局

| 路径 | 说明 |
|------|------|
| `codelite/` | CodeLite 主体（移植改动在此） |
| `docs/` | 路线、门禁、Error Queue、功能矩阵、ADR |
| `patches/` | 可重放补丁 |
| `scripts/` | 构建 / 验证脚本 |
| `toolchain/` | HarmonyOS PC CMake 工具链 |
| `third_party/` | 本地依赖树（默认 gitignore；见 build-manifest） |
| `archive/` | **平台验证 Spike 归档**（非交付主体，不扩展） |

仓库定位说明：[`docs/repo-audit.md`](docs/repo-audit.md)

## 文档索引

- [Repo Audit](docs/repo-audit.md) · [路线 V2.8](docs/roadmap.md)
- [持续验证](docs/verification.md) · [Compile Dashboard](docs/compile-dashboard.md) · [Build Manifest](docs/build-manifest.md)
- [Error Queue](docs/error-queue.md) · [移植日志](docs/porting-log.md)
- [功能矩阵](docs/feature-matrix.md) · [MVP](docs/delivery-mvp.md)
- [阶段状态板](docs/phase3-wx.md) · [ADR](docs/adr/) · [BUILD](BUILD.md)

## 触发语

`继续 CodeLite。`
