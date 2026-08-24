# wxWidgets 上游补丁（HarmonyOS / wxOHOS）

**里程碑**：不是「验证能不能跑」，而是 **wxOHOS 长期可维护**。  
平台实现优先只加在 `src/ohos/`、`include/wx/ohos/`；动上游则进本目录。

## 原则一：一个 Patch 只解决一个问题

```text
✅ 0001-ohos-platform-cmake.patch
✅ 0002-ohos-window.patch
✅ 0003-ohos-eventloop.patch
✅ 0004-ohos-display.patch

❌ 0005-fix-all.patch
❌ 把 cmake + window + event 塞进同一个文件
```

## 原则二：Patch 必须可独立应用

```bash
git apply patches/wxwidgets/0003-ohos-eventloop.patch
```

不应依赖大量未记录的隐藏修改。升级 3.3→3.4→3.5 时靠有序 `git apply`。

## 原则三：能不改 Core 就不改

```text
wxAppBase     ← 尽量不动
    │
    ▼
wxOHOSApp     ← 新增
```

## 当前文件

| 文件 | 说明 |
|------|------|
| `0001-ohos-platform-cmake.patch` | toolkit=ohos / OHOS_LOWLEVEL 列表 |
| `0002-ohos-window.patch` | 预留（Gate 4.2 若必须改公共头时） |
| `0003-ohos-eventloop.patch` | 预留（Gate 4.4） |
| `0004-ohos-display.patch` | 预留 |

纯新增的 `src/ohos/*` **不必**进 patch；仅跟踪对上游已有文件的 diff。

## 生成示例

```bash
git -C third_party/wxWidgets diff -- path/to/changed/upstream/file \
  > ../../patches/wxwidgets/0002-ohos-window.patch
```
