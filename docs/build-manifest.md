# Build Manifest（复现环境）

> Release / 他人 clone 构建时对照此表。版本变化时更新。

| 项 | 值 |
|----|-----|
| **Date** | 2026-07-23 |
| **CodeLite commit** | `6997f23`（分支 `HarmonyCodeLite`，版本 18.5.0） |
| **CodeLite remote** | https://github.com/eranif/codelite.git |
| **wxWidgets tree** | `third_party/wxWidgets`（构建产物 `build-wx/`，`wxUSE_GUI=OFF` / base） |
| **wxWidgets version** | 3.3.4（`wx-config --version`） |
| **Harmony CodeLite patches** | `patches/codelite/0001-…` + 工作区 OHOS CMake（见 `ohos-5a-configure.diff`） |
| **wxOHOS patches** | `patches/wxwidgets/` |
| **SQLite** | amalgamation 3.46.1 → `build-sqlite-ohos/install` |
| **OHOS SDK Native** | DevEco bundled OpenHarmony native（本机默认路径见 `toolchain/env.sh`） |
| **OHOS_ARCH** | `arm64-v8a` |
| **DevEco Studio** | 本机安装（路径：`/Applications/DevEco-Studio.app`） |
| **Clang (OHOS)** | 15.0.4（`aarch64-unknown-linux-ohos-clang++`） |
| **CMake** | DevEco `build-tools/cmake` 3.28.x |
| **Ninja** | 本机 `ninja`（随 PATH） |
| **Host OS** | macOS（darwin）交叉编译 → OHOS |

刷新方式：

```bash
./scripts/write-build-manifest.sh   # 若存在
# 或手动：source toolchain/env.sh && clang++ --version && cmake --version
```
