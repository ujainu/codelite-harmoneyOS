# 平台条件编译扫描（HarmonyOS 适配热点）

**范围**：`codelite/` 源码，**排除** `submodules/`  
**方法**：对常见平台宏做全文匹配（文件数 / 出现次数）  
**基线**：`6997f23`

> 鸿蒙侧建议引入统一宏（名称待定，文档中写作 `__HARMONY__` / `CL_HARMONY`），并尽量 **走 UNIX/LINUX 路径再补差异**，而不是复制 `__WXMSW__` 分支。

---

## 1. 宏统计总表

| 宏 | 涉及文件数 | 出现次数 | 含义 |
|----|-----------|---------|------|
| `__WXMSW__` | 210 | 439 | Windows + wxMSW（最大） |
| `__WXMAC__` | 94 | 172 | 旧 mac 宏（仍大量存在） |
| `__WXGTK__` | 79 | 168 | Linux GTK 后端 |
| `__WXOSX__` | 42 | 67 | 现代 macOS wx |
| `__CYGWIN__` | 7 | 9 | Cygwin |
| `_WIN32` | 9 | 14 | 裸 Win32（较少，多被 wx 宏覆盖） |
| `__FreeBSD__` | 5 | 8 | BSD |
| `__APPLE__` | 6 | 6 | Apple |
| `__linux__` | 1 | 1 | 极少直接用 |
| `__UNIX__` | 2 | 2 | 极少 |
| `__WXQT__` | 0 | 0 | 未用 |
| `__ANDROID__` | 0 | 0 | 未用 |

**结论**：平台分支几乎全是 **wx 家族宏**（`__WXMSW__` / `__WXGTK__` / `__WXOSX__`/`__WXMAC__`），不是散落的 `_WIN32`。鸿蒙适配应优先回答：**wx 后端宏是什么**（若走类 GTK/类 Unix，很多 `__WXGTK__`/`!__WXMSW__` 分支可复用）。

---

## 2. 目录热度（出现次数合计）

| 目录 | 次数 | 说明 |
|------|------|------|
| `Plugin/` | 365 | UI、路径、toolbar、terminal 控件 |
| `CodeLite/` | 182 | 进程、路径、Platform、Socket |
| `LiteEditor/` | 177 | `app.cpp` / `frame.cpp` 最热 |
| `Plugins/` | 101 | Debugger/git 等 |
| `wxcrafter/` | 53 | V1 可整模块后置 |

---

## 3. 热点文件（优先审阅）

| 次数 | 文件 | 适配关注 |
|------|------|----------|
| 38 | `LiteEditor/app.cpp` | 启动、单例、路径 |
| 33 | `LiteEditor/frame.cpp` | 主窗、菜单、Dock |
| 24 | `CodeLite/fileutils.cpp` | 文件/路径 |
| 21 | `Plugin/clButtonBase.cpp` | 控件绘制 |
| 17 | `LiteEditor/manager.cpp` | 工作区/构建 |
| 17 | `Plugins/Debugger/debuggergdb.cpp` | **后置** |
| 16 | `LiteEditor/cl_editor.cpp` | 编辑器 |
| 15 | `CodeLite/procutils.cpp` | 进程工具 |
| 15 | `Plugin/globals.cpp` | 全局平台行为 |
| 14 | `CodeLite/cl_standard_paths.cpp` | 标准路径 → **要 `__HARMONY__`** |
| 14 | `Plugin/drawingutils.cpp` | DPI/绘制 |
| 12 | `Plugin/wxterminal.cpp` | Terminal → **Phase 后段再碰** |
| 11 | `Plugin/wxTerminalCtrl/...` | 内置终端 → **后置** |
| 11 | `Plugin/aui/cl_aui_dock_art.cpp` | Dock 外观 |
| 7 | `CodeLite/Console/clConsoleBase.cpp` | 外置终端选择 |
| 6 | `CodeLite/AsyncProcess/unixprocess_impl.cpp` | 进程/PTY |
| 6 | `CodeLite/AsyncProcess/asyncprocess.cpp` | 进程入口 |
| 9 | `CodeLite/Platform/LINUX.cpp` | `ThePlatform` 非 Windows 实现 |

完整 Top 列表可由仓库内复跑扫描脚本再生（见文末）。

---

## 4. 现有平台抽象（不是 ifdef，但是关键）

`CodeLite/Platform/Platform.hpp`：

```cpp
#ifdef __WXMSW__
  #define ThePlatform MSYS2::Get()
#else
  #define ThePlatform LINUX::Get()
#endif
```

| 现状 | 鸿蒙动作 |
|------|----------|
| 仅 MSYS2 / LINUX 二分 | 增加 `HARMONY` 实现，或扩展 `LINUX` |
| `FindInstallDir` / `Which` / `GetPath` | 按鸿蒙安装布局与 PATH 重写 |
| Console 多后端（CMD/Bash/Gnome/OSX…） | **暂不实现** Harmony Terminal；MVP 可无 Run 终端 |

进程：

| 实现 | 宏/平台 |
|------|---------|
| `winprocess_impl.*` | Windows |
| `unixprocess_impl.*` / `UnixProcess.*` | Unix | → 鸿蒙优先尝试这条 |

---

## 5. 哪些地方需要 `__HARMONY__`（分级）

### P0 — MVP 前必须有策略

| 区域 | 原因 |
|------|------|
| `cl_standard_paths.cpp` | 配置/插件/数据目录 |
| `CodeLite/Platform/*` | 工具发现、HOME、安装前缀 |
| `fileutils.cpp` / `procutils.cpp` | 路径与进程 |
| `LiteEditor/app.cpp` | 启动与环境 |
| 根 `CMakeLists.txt` | `CMAKE_SYSTEM_NAME`、安装路径、裁剪 |

### P1 — 主窗口可用时

| 区域 | 原因 |
|------|------|
| `frame.cpp` / Dock / DPI / `drawingutils` | GUI 差异 |
| `CompilerLocator*` | 发现鸿蒙 SDK Clang |
| 菜单 / 快捷键（若 wx 不全） | 输入与系统集成 |

### P2 — 明确后置（不要在 Phase 前段碰）

| 区域 | 原因 |
|------|------|
| `wxterminal*` / `wxTerminalEmulator` / Console 全后端 | Terminal 风险高 |
| `Plugins/Debugger*` / DAP / LLDB | Debug 后置 |
| `CodeLite/ssh/*`、SFTP 插件 | `-DENABLE_SFTP=0` |
| `__WXMSW__` 独有分支 | 不移植，保持关闭 |

### 策略原则

```text
能复用 !__WXMSW__ / Unix  → 复用
仅路径/包管理差异       → LINUX + Harmony 覆盖
必须分叉的行为         → #ifdef __HARMONY__ / CL_HARMONY
Windows 分支           → 忽略
```

---

## 6. 建议的宏约定（Phase 2/4 落地）

```cpp
// 提案（实现时再定名）
#if defined(__OHOS__) || defined(CL_HARMONYOS)
  #define CL_HARMONY 1
#endif
```

CMake：

```cmake
if(HARMONYOS OR CMAKE_SYSTEM_NAME STREQUAL "OHOS" OR ...)
  add_compile_definitions(CL_HARMONYOS=1)
endif()
```

与 wx 后端宏的关系：**先确定 wx 在鸿蒙上定义 `__WXGTK__` 还是自定义 port 宏**，再决定有多少 GUI ifdef 要新开分支。

---

## 7. 复现扫描

```bash
cd codelite
python3 - <<'PY'
# 见本次生成统计所用脚本逻辑：遍历非 submodules，匹配
# __WXMSW__ __WXGTK__ __WXOSX__ __WXMAC__ _WIN32 __linux__ __APPLE__ ...
PY
```

Phase 4 开始前应重跑一次，避免上游漂移。
