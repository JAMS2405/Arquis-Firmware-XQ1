// STUB: motion.h real -> extern xyze_pos_t current_position (float x,y,z,e); axis_was_homed(AxisEnum); feedrate_mm_s; active_extruder
#pragma once
struct xyze_stub { float x, y, z, e; }; extern xyze_stub current_position;
enum AxisEnum : uint8_t { X_AXIS, Y_AXIS, Z_AXIS };
inline bool axis_was_homed(const AxisEnum) { return false; }
extern float feedrate_mm_s; extern uint8_t active_extruder;
extern int16_t feedrate_percentage;
