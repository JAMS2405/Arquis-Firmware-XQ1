#pragma once
#include <stdint.h>
typedef int16_t celsius_t;
class Temperature { public:
  static celsius_t wholeDegHotend(const uint8_t e);
  static celsius_t degTargetHotend(const uint8_t e);
  static void setTargetHotend(const celsius_t celsius, const uint8_t e);
};
extern Temperature thermalManager;
