#pragma once
#include "motion.h"
#include "stepper/trinamic.h"
class Stepper { public: static float get_shaping_frequency(const AxisEnum a); static void set_shaping_frequency(const AxisEnum a, const float f); };
extern Stepper stepper;
extern uint16_t shaping_min_freq;
