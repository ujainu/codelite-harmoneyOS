# 持续验证（5B+ 强制）

> V2.8 不变。从 **5B** 起：消耗真实编译错误 + 持续可验证。

## 原则

1. 每完成一个 Patch → **完整**跑 `./scripts/verify-boot.sh`（不是只确认当前报错消失）。
2. Patch 必须让 **`.o` / TU 成功数前进**，且不回归 Configure。
3. **L0 永远不要伪造 OHOS Build。**

## Patch Acceptance（合并前必勾）

```text
□ verify-boot.sh 已跑（默认全量；调试可用 --keep-build，验收仍要全量）
□ Configure / Generate 不回归
□ objects_generated >= 上一次（见 compile-progress.json）
□ Error Queue 未新增未记录的 Critical（新 Fatal 必须入队）
□ Patch 只解决一个 Root Cause
```

## Compile Baseline（比 Error Count 更重要）

`docs/logs/compile-progress.json` 字段（由 verify-boot 写入）：

| 字段 | 含义 |
|------|------|
| `translation_units_total` | `build.ninja` 中 `.o` 边数量 |
| `translation_units_success` / `objects_generated` | 已生成 `.o` |
| `objects_codelite` | CodeLite 自身模块 `.o`（非仅子模块） |
| `first_failed_target` / `first_failed_file` | 首个失败点 |
| `error_queue` | 当前开放的 E0xx |

看板：`docs/compile-dashboard.md`

## 本地命令

```bash
./scripts/verify-boot.sh
./scripts/verify-boot.sh --configure-only
./scripts/verify-boot.sh --keep-build
```

## CI 分层（诚实标注）

| 层 | 环境 | 覆盖 | 不覆盖 |
|----|------|------|--------|
| **L0** | GitHub Actions 云 | Patch check · Scripts · Docs · CMake/脚本语法 | **OHOS Build = N/A** |
| **L1** | Self-hosted + DevEco | `verify-boot.sh` 真交叉编译 | — |
| **L2** | 模拟器 | 5D Boot | 以后 |

L0 成功 **不得** 写成 `Build PASS`。正确写法：

```text
L0 ✓ Patch ✓ Scripts ✓ Documentation
OHOS Build  N/A
```

## 相关文档

- `docs/compile-dashboard.md`
- `docs/build-manifest.md`
- `docs/error-queue.md`
