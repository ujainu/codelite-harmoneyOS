/////////////////////////////////////////////////////////////////////////////
// MV-4 present chain probe (Host / compositor only — not CodeLite Paint).
//
//   MV-4.1  NativeWindow surface (w/h/format)
//   MV-4.2  eglCreateWindowSurface
//   MV-4.3  EGLContext + eglMakeCurrent
//   MV-4.4  glClear + eglSwapBuffers → first pixel
//   MV-4.5  official clMainFrame visible (eye / later)
/////////////////////////////////////////////////////////////////////////////

#pragma once

#include <cstdint>

// Run once after Attach. Stops at first FAIL. Returns 0 on first failure, 1 if
// MV-4.1…4.4 all OK (solid clear color presented).
int Fw2_MV4_ProbePresent(void* ohNativeWindow, int32_t width, int32_t height);
