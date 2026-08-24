# CodeLite 移植日志（Boot · V2.8）

## 进度总表

| 日期 | Gate | 焦点 | Root Cause / 动作 | 状态 |
|------|------|------|-------------------|------|
| Day1–4 | 5A | Configure | E001–E005 | ✅ PASS |
| Day5 | **5B** | 首个 CodeLite `.o` | 开编；E006 SQLite include | ✅ 里程碑 |
| Day5 | 5B | 全量继续 | E007 assistant / wxColour | 🟡 |

---

## Day5 — 5B 真正开始

### 里程碑

已生成 CodeLite 自身模块目标文件（示例）：

```text
CxxParser/.../cpp.cpp.o
sdk/wxsqlite3/.../wxsqlite3.cpp.o
sdk/databaselayer/.../SqliteDatabaseLayer.cpp.o
```

基线：`docs/logs/compile-progress.json` → **47 / ~1650** TU。

### E006 ✅

- **现象**：`#include "sqlite3.h"` fatal（databaselayer）
- **Root Cause**：Configure 找到 SQLite，但 `sdk/databaselayer` 未把 `SQLite3_INCLUDE_DIRS` 加入 include
- **Patch**：`patches/codelite/0004-databaselayer-sqlite-includes.patch`
- **验证**：`libdatabaselayersqlite.so` 已链接成功

### E007 🟡（下一刀）

全量/`libcodelite` 路径上：

1. `assistant`：`std::atomic<double> += double` 在 OHOS libc++ 不可用  
2. `BlockTimer.cpp` → `wxColour` / `wxUSE_GUI=1` 与当前 **base-only** wx 冲突  

仍属错误驱动；**不**因此回头做 Gate 4.2 Demo。

### 工程规范（Day5 锁定）

- Compile Baseline + Dashboard + Patch Acceptance + Build Manifest + L0「OHOS Build N/A」  
- 见 `docs/verification.md`
