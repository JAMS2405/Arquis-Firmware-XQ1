#pragma once
#include "../inc/MarlinConfigPre.h"
enum AxisEnum : uint8_t { X_AXIS, Y_AXIS, Z_AXIS, E_AXIS };
bool axis_was_homed(const AxisEnum axis);
void set_axis_unhomed(const AxisEnum axis);
void set_all_unhomed();
void update_software_endstops(const AxisEnum axis);
struct xyz_pos_stub { float x, y, z; };
extern xyz_pos_stub home_offset;
void set_home_offset(const AxisEnum axis, const float v);
