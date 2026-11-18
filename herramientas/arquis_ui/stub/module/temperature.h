#pragma once
typedef int16_t celsius_t;
class Temperature {
public:
  static celsius_t wholeDegHotend(const uint8_t) { return 0; }
  static celsius_t degTargetHotend(const uint8_t) { return 0; }
  static celsius_t wholeDegBed() { return 0; }
  static celsius_t degTargetBed() { return 0; }
  static uint8_t fanSpeedPercent(const uint8_t) { return 0; }
};
extern Temperature thermalManager;
