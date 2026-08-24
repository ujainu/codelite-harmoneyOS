// Link stubs for wxTopLevelWindowOHOS vtable slots referenced by wxDialog
// when linking against build-tree wx (OHOS TLW methods live in patched HAP wx).

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/iconbndl.h"
    #include "wx/toplevel.h"
#endif

#include "wx/ohos/toplevel.h"

void wxTopLevelWindowOHOS::SetIcons(const wxIconBundle&)
{
}

void wxTopLevelWindowOHOS::DoCentre(int)
{
}
