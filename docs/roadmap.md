# CodeLite 适配 HarmonyOS PC — 总体路线（V2.8）

> **交付**：Harmony CodeLite（≈75% 用户功能，可 clone/build/run）。  
> **主线**：Boot CodeLite；错误驱动；不提前堆 wxOHOS / 不移植非必要依赖。

## 进度条

```text
━━━━━━━━━━━━━━━━━━━━━━━━━━
Harmony CodeLite

45%

交付物 ▶ Harmony CodeLite（非 wx Demo）

✅ Platform Bring-up（Phase1–3 + Gate4.1）
▶ Boot CodeLite
    ✅ 5A Configure   ← PASS（day4d）
    ▶ 5B Compile（首个 .o 已产出 · 47/1650）  ← 当前
    ⬜ 5C Link
    ⬜ 5D Boot 主窗口

支撑 wxOHOS：仅编译缺口驱动（**勿优先 Gate 4.2**）
并行 EGL：Known Limitation
━━━━━━━━━━━━━━━━━━━━━━━━━━
```

触发语：`继续 CodeLite。` · 计划变更必改本文件 + `phase3-wx.md` + `porting-log.md` + `error-queue.md`。  
仓库定位：[`repo-audit.md`](repo-audit.md)（Spike 已进 `archive/`，交付主体是 CodeLite）。

---

## Boot 子阶段（V2.8 稳定，勿频繁改结构）

| Gate | 目标 | 状态 |
|------|------|------|
| **5A Configure** | `-- Configuring done` + `-- Generating done` | ✅ PASS |
| **5B Compile** | CodeLite 自身 `.o` 增长；`verify-boot.sh` 门禁 | 🟡 已起步（47 `.o`） |
| **5C Link** | 链出可执行/包 | ⬜ |
| **5D Boot** | 主窗口起来 | ⬜ |

### 5A Exit Criteria（严格）

仅当日志出现：

```text
-- Configuring done
-- Generating done
```

且 **0 Fatal Configure Errors** → 5A **PASS**。  
不要求 Compile / Link / GUI。

原则：真实错误 → Root Cause → 最小 Patch；能关不移植；**不提前 wxOHOS**。

### 5B+ 持续验证（强制）

每个 Patch 完成后必须跑 **完整验证**（不是只看当前报错消失）：

```bash
./scripts/verify-boot.sh
```

- 锁住 5A（Configure/Generate 不回归）
- 全量 `ninja`，记录 `.o` 进度；禁止进度倒退
- 详见 [`docs/verification.md`](verification.md) · CI：`.github/workflows/verify-boot.yml`

---

## 错误驱动优先级

1. ~~**Configure PASS**~~ ✅  
2. **Compile**（首批 .cpp / 首个 `.o`）← 当前  
3. 暴露 **wx GUI / wxOHOS** 缺口再补  
4. 再扩 wxOHOS（禁止提前 100% / 禁止优先 Gate 4.2）

日志：`docs/porting-log.md` · `docs/error-queue.md` · `docs/logs/codelite-cmake-day*.log` · `docs/verification.md`  
功能表：`docs/feature-matrix.md`（P0/P1/P2，交付时再勾完成）

---

## 层次

```text
Harmony CodeLite ← 验收
      ↓
codelite/（HarmonyCodeLite 分支）
      ↓
wx + wxOHOS（手段）
      ↓
HarmonyOS PC
```

## 已完成的基础设施

Platform Bring-up ✅ · `patches/codelite/0001-defer-osxdeps-after-project.patch` ✅

## 文档索引

`porting-log.md` · `error-queue.md` · `feature-matrix.md` · `delivery-mvp.md` · `phase3-wx.md` · ADR-0006
