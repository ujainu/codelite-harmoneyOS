# HarmonyOS → wxWidgets 事件映射（Gate 4.4）

> Phase 4 Gate 4.4 的**规格文档**：先对齐语义，再写派发代码。  
> 实现落在 `src/ohos/`（及必要时的独立 patch），不在 Spike 里扩张。

状态：⬜ 规格初稿（随 4.2/4.3 实现可修订）

## 指针 / 触摸

| Harmony / Native（Spike 已见） | wxWidgets | 备注 |
|--------------------------------|-----------|------|
| Pointer / Touch Down | `wxEVT_LEFT_DOWN`（主按钮） | 多键时再分 `RIGHT`/`MIDDLE` |
| Pointer / Touch Move | `wxEVT_MOTION` | 可节流，与 Spike MOVE 策略类似 |
| Pointer / Touch Up | `wxEVT_LEFT_UP` | |
| Hover（若有） | `wxEVT_ENTER_WINDOW` / `wxEVT_LEAVE_WINDOW` | PC 模拟器优先 |

触点坐标：客户区相对窗口；需与 `wxOHOSWindow` 尺寸 / DPI 一致。

## 鼠标（PC）

| Harmony | wxWidgets |
|---------|-----------|
| Mouse button down/up | `wxEVT_*_DOWN` / `wxEVT_*_UP` |
| Mouse move | `wxEVT_MOTION` |
| Wheel（若有） | `wxEVT_MOUSEWHEEL` |

## 键盘

| Harmony | wxWidgets |
|---------|-----------|
| KeyDown | `wxEVT_KEY_DOWN` |
| KeyUp | `wxEVT_KEY_UP` |
| 字符输入（IME / Text） | `wxEVT_CHAR` / 后续 IME 专题 | Spike 的 ARK TEXT 仅为证明通道，正式路径走 Key/Char |

## 焦点 / 窗口

| Harmony | wxWidgets |
|---------|-----------|
| Focus gained | `wxEVT_SET_FOCUS` |
| Focus lost | `wxEVT_KILL_FOCUS` |
| Show / Hide | `wxEVT_SHOW`（若适用） |
| Resize | `wxEVT_SIZE` |
| Close / Destroy | `wxEVT_CLOSE_WINDOW` → Destroy 路径 |

## Ability 生命周期（非经典 wx 桌面，但必须映射）

| Ability | wxOHOS 行为（建议） |
|---------|---------------------|
| onForeground | 恢复 EventLoop / 继续 Present |
| onBackground | 暂停 Present；可选 `wxEVT_ACTIVATE` |
| onDestroy | `OnExit` / 拆窗口 |

## 验收（Gate 4.4）

- [ ] 上表主路径均有 hilog 或 wx 断言可追踪  
- [ ] 与 Gate 4.2 窗口生命周期联调（含 Hide 后再 Show 的事件）  
- [ ] 不依赖 Spike HAP 作为唯一载体（可仍用 HAP 作宿主，但代码在 `ohos/`）

## 非目标（本 Gate）

完整 IME、拖放、手势识别、无障碍 API。
