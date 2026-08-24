// Boot-safe wxGCDCImpl::DoGetTextExtent replacement for in-place lib patch.
#include "wx/wxprec.h"

#include "wx/string.h"
#include "wx/ohos/private/textextent.h"

extern "C" __attribute__((visibility("default")))
void _ZNK10wxGCDCImpl15DoGetTextExtentERK8wxStringPiS3_S3_S3_PK6wxFont(
    const void* WXUNUSED(self),
    const wxString& str,
    int* width,
    int* height,
    int* descent,
    int* externalLeading,
    const void* WXUNUSED(font))
{
    wxOhosTextMeasure::FallbackGetTextExtent(str, width, height, descent, externalLeading);
}

extern "C" __attribute__((visibility("default")))
void _ZNK14wxCairoContext14GetTextExtentERK8wxStringPdS3_S3_S3_(
    const void* WXUNUSED(self),
    const wxString& str,
    double* width,
    double* height,
    double* descent,
    double* externalLeading)
{
    int w = 0, h = 0, d = 0, e = 0;
    wxOhosTextMeasure::FallbackGetTextExtent(str, &w, &h, &d, &e);
    if ( width )
        *width = w;
    if ( height )
        *height = h;
    if ( descent )
        *descent = d;
    if ( externalLeading )
        *externalLeading = e;
}
