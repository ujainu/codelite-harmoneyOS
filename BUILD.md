# BUILD.md

当前主线：Boot CodeLite（V2.8）。**5A Configure 已 PASS**。

## 一键验证（5B+ 门禁）

```bash
source toolchain/env.sh
./scripts/verify-boot.sh                   # 全量：重配 + ninja（每 Patch 必跑）
./scripts/verify-boot.sh --configure-only  # 仅锁 5A
```

进度与规则：`docs/verification.md` · `docs/logs/compile-progress.json`

## 手动等价命令（5A）

```bash
source toolchain/env.sh
cmake -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$CMAKE_TOOLCHAIN_FILE" \
  -DCMAKE_BUILD_TYPE=Release \
  -DWITH_WX_CONFIG=$PWD/build-wx/wx-config \
  -DSQLite3_INCLUDE_DIR=$PWD/build-sqlite-ohos/install/include \
  -DSQLite3_LIBRARY=$PWD/build-sqlite-ohos/install/lib/libsqlite3.so \
  -DENABLE_SFTP=0 -DENABLE_LLDB=0 -DWITH_PCH=0 -DBUILD_TESTING=OFF \
  -S codelite -B build-codelite-ohos
# 期望：Configuring done + Generating done
```

完整说明随 5B/5C 推进补全；计划以 `docs/roadmap.md` 为准。
