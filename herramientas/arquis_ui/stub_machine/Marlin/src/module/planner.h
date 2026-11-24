#pragma once
#include <stdint.h>
class Planner { public:
  struct planner_settings_t { uint32_t max_acceleration_mm_per_s2[4]; float axis_steps_per_mm[4]; float max_feedrate_mm_s[4]; float acceleration, retract_acceleration, travel_acceleration; };
  static planner_settings_t settings;
  static bool has_blocks_queued();
};
extern Planner planner;
