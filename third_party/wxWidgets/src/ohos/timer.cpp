/////////////////////////////////////////////////////////////////////////////
// Name:        src/ohos/timer.cpp
/////////////////////////////////////////////////////////////////////////////

#include "wx/ohos/timer.h"

wxOHOSTimer::wxOHOSTimer()
    : m_intervalMs(0)
    , m_oneShot(false)
    , m_running(false)
{
}

wxOHOSTimer::~wxOHOSTimer()
{
    Stop();
}

bool wxOHOSTimer::Start(int milliseconds, bool oneShot)
{
    m_intervalMs = milliseconds;
    m_oneShot = oneShot;
    m_running = milliseconds > 0;
    return m_running;
}

void wxOHOSTimer::Stop()
{
    m_running = false;
}
