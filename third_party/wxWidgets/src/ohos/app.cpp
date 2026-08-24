/////////////////////////////////////////////////////////////////////////////
// Name:        src/ohos/app.cpp
// Purpose:     Minimal wxApp for OHOS GUI compile path
// Licence:     wxWindows licence
/////////////////////////////////////////////////////////////////////////////

#include "wx/wxprec.h"

#ifndef WX_PRECOMP
    #include "wx/app.h"
#endif

#include "wx/evtloop.h"

#include <cstdio>

wxIMPLEMENT_DYNAMIC_CLASS(wxApp, wxAppBase);

wxApp::wxApp()
{
    // Same contract as GTK/Qt/MSW: mark fully constructed before Initialize().
    // Without this, wxAppConsoleBase::Initialize() asserts for GUI apps.
    WXAppConstructed();
    std::fprintf(stderr, "[R0] wxApp ctor → WXAppConstructed\n");
    std::fflush(stderr);
}

wxApp::~wxApp() = default;

bool wxApp::Initialize(int& argc, wxChar **argv)
{
    // Runtime Skeleton R0: mark entry into wxApp init chain.
    std::fprintf(stderr, "[R0] wxApp::Initialize enter argc=%d\n", argc);
    std::fflush(stderr);
    const bool ok = wxAppBase::Initialize(argc, argv);
    std::fprintf(stderr, "[R0] wxApp::Initialize %s\n", ok ? "ok" : "fail");
    std::fflush(stderr);
    return ok;
}

void wxApp::CleanUp()
{
    wxAppBase::CleanUp();
}

void wxApp::WakeUpIdle()
{
    if ( wxEventLoopBase* loop = wxEventLoopBase::GetActive() )
        loop->WakeUp();
}
