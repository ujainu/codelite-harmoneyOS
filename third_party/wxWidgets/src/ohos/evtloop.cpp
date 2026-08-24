/////////////////////////////////////////////////////////////////////////////
// Name:        src/ohos/evtloop.cpp
// Purpose:     OHOS GUI event loop (console/unix FD loop + bring-up probes)
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/evtloop.h"
#include "wx/ohos/evtloop.h"

#include <cstdio>

namespace
{
void App004Once(const char* msg)
{
    static bool logged[8] = {};
    // slot 0=ctor ok/fail, 1=Pending, 2=Dispatch, 3=WakeUp
    int slot = 0;
    if (msg[0] == 'P')
        slot = 1;
    else if (msg[0] == 'D')
        slot = 2;
    else if (msg[0] == 'W')
        slot = 3;
    else if (msg[0] == 'c')
        slot = 0;
    if (slot < 8 && logged[slot])
        return;
    if (slot < 8)
        logged[slot] = true;
    std::fprintf(stderr, "[APP-004] %s\n", msg);
    std::fflush(stderr);
}
} // namespace

wxGUIEventLoop::wxGUIEventLoop()
{
    App004Once(IsOk() ? "ctor IsOk=1" : "ctor IsOk=0 FAIL");
}

bool wxGUIEventLoop::Pending() const
{
    App004Once("Pending enter");
    if (!IsOk()) {
        std::fprintf(stderr, "[APP-004] FAIL Pending: !IsOk (no dispatcher)\n");
        std::fflush(stderr);
        return false;
    }
    return wxConsoleEventLoop::Pending();
}

bool wxGUIEventLoop::Dispatch()
{
    App004Once("Dispatch enter");
    if (!IsOk()) {
        std::fprintf(stderr, "[APP-004] FAIL Dispatch: !IsOk\n");
        std::fflush(stderr);
        return false;
    }
    return wxConsoleEventLoop::Dispatch();
}

void wxGUIEventLoop::WakeUp()
{
    App004Once("WakeUp");
    if (!IsOk())
        return;
    wxConsoleEventLoop::WakeUp();
}
