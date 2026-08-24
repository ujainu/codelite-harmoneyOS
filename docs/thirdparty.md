# 第三方依赖清单（HarmonyOS PC 可行性）

**基线**：CodeLite 18.5.0 @ `6997f23`  
**图例**

| 标记 | 含义 |
|------|------|
| ✔ | MVP 需要，且预期可按源码/系统库编过（POSIX 友好） |
| ⚠ | 需要验证、升级、裁剪或找替代 |
| ❌ | V1 / MVP **不做** 或平台高风险，默认关闭 |
| ？ | 未知：必须尽早 spike（尤其 wx） |

> 「可直接编」指依赖本身偏跨平台；**不表示**已在鸿蒙验证通过。

---

## 1. 总表

| 组件 | 来源 | MVP | 状态 | 说明 |
|------|------|-----|------|------|
| **wxWidgets** (≥3.1.6) | 外部 / `build.sh` 自建 | 要 | **？→ 关键** | 无成熟鸿蒙 port 则项目难度跳档 |
| **SQLite3** | 系统 `find_package` | 要 | ✔/⚠ | 需 sysroot 或自带；开发包要齐 |
| **wxsqlite3** | `sdk/wxsqlite3` | 要 | ✔ | 源码在树内，跟 wx |
| **databaselayer** | `sdk/databaselayer` | 视插件 | ⚠ | DatabaseExplorer 用；MVP 可尽量不链到 |
| **bison / flex** | 主机工具 | 要 | ✔ | 生成词法；可 host 执行 |
| **lexilla** | `submodules/lexilla` | 要 | ✔ | 编辑器词法，跟上游 |
| **yaml-cpp** | `submodules/yaml-cpp` | 多半要 | ✔ | 纯 C++，风险低 |
| **zlib** | submodule / 系统 | 间接 | ✔ | 常见 |
| **libssh** | submodule + 系统 | 否 | ❌ | `-DENABLE_SFTP=0` |
| **OpenSSL** | 系统 / openssl-cmake | 否 | ❌ | assistant/SFTP 相关；后置 |
| **hunspell** | submodule | 否 | ❌ | SpellChecker OFF |
| **wxTerminalEmulator** | submodule | 否 | ❌ | Terminal 后置（Phase 后段） |
| **wxdap** | submodule | 否 | ❌ | DAP/Debug 后置 |
| **LLDB** | 系统 LLVM | 否 | ❌ | `-DENABLE_LLDB=0` |
| **clang / LLVM 工具链** | SDK | 要（编译器用） | ⚠ | 编 CodeLite 用 Clang；IDE 内调用后置 |
| **GTK3** | 系统 | 视 wx 后端 | ⚠/？ | 上游 Linux wx 默认 GTK3；鸿蒙未必走这条 |
| **lua / LuaBridge** | submodule | 非 MVP | ⚠ | 可后置 |
| **doctest** | submodule | 否 | ❌ | `BUILD_TESTING=OFF` |
| **dtl** | submodule | 间接 | ✔ | diff 算法，轻量 |
| **websocketpp** | submodule | 视功能 | ⚠ | 后置 |
| **assistant / agent-sop** | submodule | 否 | ❌ | AI 助手整块后置 |
| **cc-wrapper / wx-config-msys2** | submodule | 否 | ❌ | 他平台构建辅助 |
| **TinyXML** | — | — | — | **仓库未使用**；XML 走 `wxXml` + `CodeLite/xml` |
| **外置 ctags** | 运行时 Which | 后置 | ⚠ | 非链接依赖 |
| **外置 gdb** | 运行时 | 后置 | ❌ MVP | Debug 阶段再谈 |

---

## 2. 按风险分组

### ？ 必须最先 spike

1. **wxWidgets on HarmonyOS PC**  
   - 有 port / 能自编 / 只能用子集？  
   - 后端宏是 `__WXGTK__` 还是新 port？  
   - **不过这一关，不进入 libcodelite 大海捞针。**

### ⚠ 编内核时就会碰到

| 库 | 动作 |
|----|------|
| SQLite3 | 确认 SDK 是否提供；否则 bundle 或交叉 find |
| bison/flex | 用宿主机生成，或 SDK 提供 |
| lexilla / yaml-cpp / wxsqlite3 | 随工程编；注意 C++20/线程 |
| GTK | 仅当 wx 选 GTK 后端时；否则标 N/A |

### ❌ MVP 直接关掉

```text
libssh / SFTP / Remoty
LLDB / Debugger / wxdap
hunspell / SpellChecker
wxTerminalEmulator / 内置 Terminal
assistant
Subversion / cscope / Docker / Git GUI（插件级 OFF）
doctest
```

---

## 3. Submodules 一览

| path | 用途 | MVP |
|------|------|-----|
| `lexilla` | 高亮词法 | ON |
| `yaml-cpp` | YAML | ON |
| `zlib` | 压缩 | ON/间接 |
| `dtl` | diff | ON/间接 |
| `libssh` | SSH/SFTP | OFF |
| `hunspell` | 拼写 | OFF |
| `wxTerminalEmulator` | 终端 | OFF |
| `wxdap` | DAP | OFF |
| `lua` / `LuaBridge` | 脚本 | OFF |
| `openssl-cmake` | OpenSSL 构建 | OFF |
| `assistant` / `agent-sop` | AI | OFF |
| `doctest` | 测试 | OFF |
| `websocketpp` | WS | OFF |
| `cc-wrapper` / `wx-config-msys2` | 他平台 | OFF |

---

## 4. 与「升级」相关的备注

| 组件 | 是否要升级 |
|------|------------|
| wxWidgets | 不先谈升级，先谈 **有没有鸿蒙后端**；版本跟上游要求 ≥3.1.6 |
| SQLite | 一般无需追新；能 link 即可 |
| yaml-cpp / lexilla | 跟 CodeLite 钉死的 submodule 走，勿随意升级 |
| LLVM/LLDB | MVP 不用；日后若做调试再选 gdb vs lldb |

---

## 5. 决策记录

| 日期 | 项 | 结论 |
|------|----|------|
| 2026-07-17 | wxWidgets 3.1 Configure | ✅ `wxUSE_GUI=OFF` → toolkit=base |
| 2026-07-17 | GTK3 路径 | ❌ **正式关闭**；见 `archive/docs/gui-backend-investigation.md` |
| 2026-07-22 | 主线纠正 | ✅ ADR-0006：交付 = Harmony CodeLite；Boot CodeLite 为主线 |
| 2026-07-21 | Spike 结束 / Phase 4 | ✅ ADR-0004 · ADR-0005 · `src/ohos` 骨架 Compile/Link/Run |
| 2026-07-21 | wx_base | ✅ `libwx_baseu-3.3-OHOS.so` · Risk 🟢 |
| 2026-07-21 | GUI Backend Risk | 🔴 验证方向 **B Spike**（非正式架构）；选型未完成 |
| 2026-07-21 | 验证方向 | ✅ B；A/C/E 暂不采用；B1–B4 + Stop Gate 见 `archive/docs/gui-backend-investigation.md` |
| （待） | GUI 路线成立 | 仅当 B1–B4 PASS |
| （待） | SQLite 来源 | 系统 / 源码捆绑 |

Spike 结果请回写本表，避免 Phase 4 口头传说。
