# Harmony CodeLite — 交付与「75%」定义（V2.7）

> **75% = 用户功能**，不是代码行数、不是模块文件数。  
> 客户验收对象：**可下载、构建、运行的 Harmony CodeLite**，UI/主流程尽量接近原版。

## 非交付物

- wxOHOS / Spike Demo（仅基础设施与证据）
- 「只有平台层、没有 IDE」的中间态作为最终交付

## MVP 功能表（目标约 75%）

| 模块 | 目标 | 状态 |
|------|------|------|
| 打开工程 / Workspace | ✅ 要有 | ⬜ |
| 编辑代码 | ✅ | ⬜ |
| 保存文件 | ✅ | ⬜ |
| 文件树 | ✅ | ⬜ |
| 搜索 | ✅ | ⬜ |
| 编译 / Build | ✅ | ⬜ |
| 主窗口布局（Menu / Toolbar / Tree / Editor / Bottom）≈ 原版 | ✅ | ⬜ |
| Debug | ❌ 可暂缓（计入未做的 25%） | ⬜ 默认 OFF |
| Git 插件 | ❌ 可暂缓 | ⬜ 默认 OFF |
| 插件市场 | ❌ 可暂缓 | ⬜ 默认 OFF |
| Terminal / SFTP / SVN 等 | ❌ 默认可 OFF | ⬜ |

勾选随 Boot / Module / UI 阶段更新；计划变更时同步改本表与 `roadmap.md`。

## 发布验收（阶段五）

| 项 | 要求 |
|----|------|
| 远程仓 | Gitee 和/或 GitHub 同步 |
| 文档 | README · BUILD · INSTALL · CHANGELOG · LICENSE |
| 构建 | clone → cmake → build → run（按文档可复现） |
| 可选 | `HarmonyCodeLite-v0.x.x` Release 包 |

## 与阶段的关系

```text
Platform Bring-up ✅
        → Boot CodeLite（现主线）
        → Module Bring-up
        → UI & 本表勾选
        → Publish
```
