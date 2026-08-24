/////////////////////////////////////////////////////////////////////////////
// Name:        src/ohos/display.cpp
/////////////////////////////////////////////////////////////////////////////

#include "wx/ohos/display.h"

wxOHOSDisplay::wxOHOSDisplay()
    : m_width(0)
    , m_height(0)
    , m_scale(1.0)
{
}

void wxOHOSDisplay::SetGeometry(int width, int height, double scale)
{
    m_width = width;
    m_height = height;
    m_scale = scale;
}
