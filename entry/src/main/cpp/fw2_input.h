#pragma once

#include <cstdint>

// action: 0=move, 1=down, 2=up, 3=cancel. button: 0=left, 1=right, 2=middle.
void Fw2_DispatchPointer(int action, int button, float x, float y);

// rotation: wheel delta (positive = scroll up / away from user). position in surface space.
void Fw2_DispatchWheel(int rotation, float x, float y);

// When true, host forwards every move event (no XComponent move throttling).
bool Fw2_ShouldForwardAllMoves();

// True while an AUI sash drag is in progress (set synchronously on sash Down).
bool Fw2_IsAuiDragActive();

// I-7.1/I-7.2: OHOS key action, KEY_* code, modifier bitmask from GetKeyEventModifierKeyStates.
void Fw2_DispatchKey(int action, int keyCode, uint64_t modifierKeys);
