// STUB: planner.h real: static int16_t flow_percentage[EXTRUDERS]; static void refresh_e_factor(const uint8_t e)
#pragma once
#include <stdint.h>
#include "motion.h"   // AxisEnum
class Planner { public: static float get_axis_position_mm(const AxisEnum axis);   // firma real de planner.h (2.1.2.10): posicion segun los pasos del stepper
  static int16_t flow_percentage[1]; static void refresh_e_factor(const uint8_t e);
  static bool leveling_active; static float z_fade_height; static bool has_blocks_queued(); static void synchronize();
  // firmas reales de planner.h (2.1.2.10): planner_settings_t { max_acceleration_mm_per_s2[], acceleration, retract_acceleration, travel_acceleration }, extruder_advance_K[], junction_deviation_mm
  struct planner_settings_t { uint32_t max_acceleration_mm_per_s2[4]; float acceleration, retract_acceleration, travel_acceleration; };
  static planner_settings_t settings; static float extruder_advance_K[1]; static float junction_deviation_mm; };  /* firmas de planner.h real */ extern Planner planner;
