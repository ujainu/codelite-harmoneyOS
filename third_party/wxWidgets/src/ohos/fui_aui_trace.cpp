/////////////////////////////////////////////////////////////////////////////
// F-UI-3: wxAui trace helpers (linked into libwx_ohosu_aui, called via patch)
/////////////////////////////////////////////////////////////////////////////

#include <hilog/log.h>

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xF004
#define LOG_TAG "wxOHOS"

extern "C" {

void FuiAuiCtorEnter(void)
{
    OH_LOG_INFO(LOG_APP, "[FUI_AUI] ctor enter");
}

void FuiAuiCtorReturn(void)
{
    OH_LOG_INFO(LOG_APP, "[FUI_AUI] ctor return");
}

void FuiAuiUpdateEnter(void)
{
    OH_LOG_INFO(LOG_APP, "[FUI_AUI] Update enter");
}

void FuiAuiUpdateReturn(void)
{
    OH_LOG_INFO(LOG_APP, "[FUI_AUI] Update return");
}

void FuiAuiNotebookCreateEnter(void)
{
    OH_LOG_INFO(LOG_APP, "[FUI_BOOK] Notebook Create enter");
}

void FuiAuiNotebookCreateReturn(void)
{
    OH_LOG_INFO(LOG_APP, "[FUI_BOOK] Notebook Create return");
}

void FuiEditorScintillaCreateEnter(void)
{
    OH_LOG_INFO(LOG_APP, "[FUI_EDITOR] Scintilla create enter");
}

} // extern "C"
