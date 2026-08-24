/////////////////////////////////////////////////////////////////////////////
// Name:        src/ohos/skel_main.cpp
// Purpose:     Phase 4 smoke: Compile → Link → Run platform skeleton
/////////////////////////////////////////////////////////////////////////////

#include "wx/ohos/app.h"
#include "wx/ohos/window.h"
#include "wx/ohos/display.h"
#include "wx/ohos/timer.h"

#include <cstdio>

int main()
{
    std::printf("wxOHOS skeleton smoke start\n");

    wxOHOSDisplay display;
    display.SetGeometry(1280, 800, 1.0);

    wxOHOSWindow window;
    if (!window.Create(display.GetWidth(), display.GetHeight()))
        return 1;
    window.Show(true);

    wxOHOSTimer timer;
    timer.Start(16);

    wxOHOSApp app;
    if (!app.OnInit())
        return 2;
    const int loopRc = app.MainLoop();
    app.OnExit();

    timer.Stop();
    window.Destroy();

    std::printf("wxOHOS skeleton smoke done rc=%d\n", loopRc);
    return loopRc == 0 ? 0 : 3;
}
