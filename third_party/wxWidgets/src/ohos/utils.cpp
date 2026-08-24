/////////////////////////////////////////////////////////////////////////////
// Miscellaneous OHOS utility functions
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#include "wx/utils.h"

#include "wx/ohos/nativewindow.h"

#ifndef WX_PRECOMP
    #include "wx/window.h"
#endif

wxWindow *wxGetActiveWindow()
{
    return wxWindow::FindFocus();
}

void wxGetMousePosition(int *x, int *y)
{
    if ( x )
        *x = wxOhos_GetLastMouseX();
    if ( y )
        *y = wxOhos_GetLastMouseY();
}

wxMouseState wxGetMouseState()
{
    wxMouseState ms;
    int x = 0, y = 0;
    wxGetMousePosition(&x, &y);
    ms.SetX(x);
    ms.SetY(y);
    return ms;
}

wxWindow* wxFindWindowAtPoint(const wxPoint& pt)
{
    return wxGenericFindWindowAtPoint(pt);
}

void wxBell()
{
}

bool wxGetKeyState(wxKeyCode key)
{
    wxASSERT_MSG(key != WXK_LBUTTON && key != WXK_RBUTTON && key != WXK_MBUTTON,
                 "can't use wxGetKeyState() for mouse buttons");
    wxUnusedVar(key);
    return false;
}
