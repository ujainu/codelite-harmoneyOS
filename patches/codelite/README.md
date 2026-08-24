# CodeLite Port patches

Apply onto `codelite/` @ upstream `HarmonyCodeLite` / tracked HEAD.

| Patch | Problem | Layer |
|-------|---------|-------|
| `0001-defer-osxdeps-after-project.patch` | OSXDeps before `project()` | Build |
| `0004-databaselayer-sqlite-includes.patch` | databaselayer missing SQLite includes | Build |
| `ohos-5a-configure.diff` | GTK skip · wx seed/shim · Assistant TLS OFF | Build / Dependency |

`ohos-5a-configure.diff` is the **5A Configure** stack after 0001 (one apply for reproducibility).  
Split 0002/0003 files may lag the working tree — prefer `ohos-5a-configure.diff` until re-split.

**Do not** merge these into wxOHOS. They belong to **CodeLite Port**.
