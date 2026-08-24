// Minimal F-4 build stub — full input in fw2_input.cpp (blocked on OHOS wx header gaps).
#include "fw2_input.h"

void Fw2_DispatchPointer(int /*action*/, int /*button*/, float /*x*/, float /*y*/) {}

void Fw2_DispatchWheel(int /*rotation*/, float /*x*/, float /*y*/) {}

bool Fw2_ShouldForwardAllMoves() { return false; }

bool Fw2_IsAuiDragActive() { return false; }

void Fw2_DispatchKey(int /*action*/, int /*keyCode*/, uint64_t /*modifierKeys*/) {}
