// STUB: motion.h real -> extern xyze_pos_t current_position (float x,y,z,e); axis_was_homed(AxisEnum); feedrate_mm_s; active_extruder
#pragma once
struct xyze_stub { float x, y, z, e; }; extern xyze_stub current_position;
enum AxisEnum : uint8_t { X_AXIS, Y_AXIS, Z_AXIS };
inline bool axis_was_homed(const AxisEnum) { return false; }
extern float feedrate_mm_s; extern uint8_t active_extruder;
extern int16_t feedrate_percentage;
inline bool all_axes_homed() { return false; }   // motion.h real
struct soft_endstops_stub { bool _enabled, _loose; }; extern soft_endstops_stub soft_endstop;
#define SET_SOFT_ENDSTOP_LOOSE(loose) (soft_endstop._loose = loose)   // motion.h real (con HAS_SOFTWARE_ENDSTOPS)
