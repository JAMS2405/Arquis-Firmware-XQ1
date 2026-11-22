// STUB: stepper.h real (2.1.2.10): constexpr uint16_t shaping_min_freq (global, solo con HAS_ZV_SHAPING);
//   static float Stepper::get_shaping_frequency(const AxisEnum axis)  /  get_shaping_damping_ratio(const AxisEnum axis)
#pragma once
#include <stdint.h>
#include "motion.h"
constexpr uint16_t shaping_min_freq = 25;
class Stepper { public: static float get_shaping_frequency(const AxisEnum axis); static float get_shaping_damping_ratio(const AxisEnum axis); }; extern Stepper stepper;
