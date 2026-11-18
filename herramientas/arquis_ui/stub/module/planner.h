// STUB: planner.h real: static int16_t flow_percentage[EXTRUDERS]; static void refresh_e_factor(const uint8_t e)
#pragma once
#include <stdint.h>
class Planner { public: static int16_t flow_percentage[1]; static void refresh_e_factor(const uint8_t e); }; extern Planner planner;
