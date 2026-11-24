#pragma once
#include "../inc/MarlinConfigPre.h"
enum AxisEnum : uint8_t { X_AXIS, Y_AXIS, Z_AXIS, E_AXIS };
bool axis_was_homed(const AxisEnum axis);
void set_axis_unhomed(const AxisEnum axis);
