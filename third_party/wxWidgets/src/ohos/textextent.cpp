/////////////////////////////////////////////////////////////////////////////
// OHOS boot-safe text measurement fallback.
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/ohos/private/textextent.h"

#include <hilog/log.h>

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0xF004
#define LOG_TAG "wxOHOS"

namespace wxOhosTextMeasure
{

namespace
{

constexpr int kOhosCharWidth = 8;
constexpr int kOhosLineHeight = 16;

} // namespace

void FallbackGetTextExtent(const wxString& str,
                           int* width,
                           int* height,
                           int* descent,
                           int* externalLeading)
{
    OH_LOG_INFO(LOG_APP, "[FULL_UI] OHOS GetTextExtent enter");

    const int w = static_cast<int>(str.length()) * kOhosCharWidth;
    const int h = kOhosLineHeight;

    if ( width )
        *width = w;
    if ( height )
        *height = h;
    if ( descent )
        *descent = 0;
    if ( externalLeading )
        *externalLeading = 0;

    OH_LOG_INFO(LOG_APP,
                "[FULL_UI] OHOS GetTextExtent return width=%{public}d height=%{public}d",
                w, h);
}

} // namespace wxOhosTextMeasure
