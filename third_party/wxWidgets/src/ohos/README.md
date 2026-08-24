# Phase 4 wxOHOS platform (Gate 4.1 PASS)

Layout: `src/ohos/` + `include/wx/ohos/` (wxWidgets convention).

**Principles:** add platform code here; do **not** rush changes to `wxAppBase` / Core.
Upstream-required edits → `patches/wxwidgets/`.

**Next: Gate 4.2** — Create + bind OHNativeWindow + Show/Hide/Destroy + 100× stress.

Do **not** skip to wxPaintDC before Gate 4.3A (Surface Present).
