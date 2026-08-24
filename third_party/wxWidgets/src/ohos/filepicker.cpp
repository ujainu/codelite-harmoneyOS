/////////////////////////////////////////////////////////////////////////////
// wx/ohos/filepicker.cpp
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/ohos/filepicker.h"

namespace {
wxOhos_PickOpenFileFn g_pickOpenFileFn = nullptr;
}

extern "C" void wxOhos_RegisterPickOpenFileFn(wxOhos_PickOpenFileFn fn)
{
    g_pickOpenFileFn = fn;
}

extern "C" int wxOhos_PickOpenFile(const char* defaultDir,
                                   int allowMultiple,
                                   char* outPath,
                                   int outPathSize,
                                   char* outUri,
                                   int outUriSize)
{
    if ( !g_pickOpenFileFn )
        return -1;
    return g_pickOpenFileFn(defaultDir, allowMultiple, outPath, outPathSize, outUri, outUriSize);
}
