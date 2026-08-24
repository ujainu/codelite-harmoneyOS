# Error Queue（Boot CodeLite）

> V2.8 稳定。**5A PASS** · **5B 已开始**（首个 CodeLite `.o` 已产出）。

| ID | Error | Layer | Status | Notes |
|----|-------|-------|--------|-------|
| E001 | OSXDeps / OpenSSL | Build | ✅ | `0001-…` |
| E002 | SQLite3 not found | Dependency | ✅ | amalgamation |
| E003 | Could not locate GTK | Build | ✅ | OHOS 跳过 |
| E004 | `find_package(wxWidgets)` | Build | ✅ | seed + shim |
| E005 | Assistant OpenSSL | Dependency | ✅ | TLS OFF |
| — | **5A Configure** | — | ✅ | Configuring + Generating done |
| E006 | `sqlite3.h` not found（databaselayer） | Build | ✅ | `0004-databaselayer-sqlite-includes.patch` |
| E007 | assistant `atomic<double> +=` / 随后 `wxColour`@base | Compile | 🟡 | 全量 ninja 下一刀；勿提前 Gate 4.2 Demo |

## Gates

| Gate | Exit | Status |
|------|------|--------|
| **5A** | Configuring + Generating done | ✅ |
| **5B** | CodeLite 自身 `.o` 增长；每 Patch `verify-boot.sh` | 🟡 已起步（47 `.o`） |
| **5C** | Link | ⬜ |
| **5D** | Boot | ⬜ |

`docs/verification.md` · `docs/compile-dashboard.md` · `docs/build-manifest.md`
