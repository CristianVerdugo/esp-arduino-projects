#pragma once

#include <Arduino.h>

// 1 = motor logic active, 0 = disabled (code kept, calls become no-ops).
#define MOTOR_ENABLED 1

#if MOTOR_ENABLED
void motorSetup();
void motorUpdate();
#else
inline void motorSetup() {}
inline void motorUpdate() {}
#endif
