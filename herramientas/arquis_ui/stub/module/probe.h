// STUB: probe.h real: class Probe { static float min_x(); max_x(); min_y(); max_y(); } (con HAS_BED_PROBE || HAS_LEVELING)
#pragma once
class Probe { public: static float min_x(); static float max_x(); static float min_y(); static float max_y(); };
extern Probe probe;
