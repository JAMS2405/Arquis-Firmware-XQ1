// STUB: firmas de temperature.h real (Marlin 2.1.2.10): leidas en Marlin/src/module/temperature.h
#pragma once
typedef int16_t celsius_t;
struct pid_stub { float p() const; float i() const; float d() const; };   // PID_t real: p(), i(), d()
struct hotend_info_t_stub { celsius_t target; pid_stub pid; };
struct bed_info_t_stub { celsius_t target; pid_stub pid; };
class Temperature {
public:
  static hotend_info_t_stub temp_hotend[1];
  static bed_info_t_stub temp_bed;
  static celsius_t extrude_min_temp;
  static celsius_t wholeDegHotend(const uint8_t) { return 0; }
  static celsius_t degTargetHotend(const uint8_t) { return 0; }
  static celsius_t wholeDegBed() { return 0; }
  static celsius_t degTargetBed() { return 0; }
  static uint8_t fanSpeedPercent(const uint8_t) { return 0; }
  static void set_fan_speed(const uint8_t fan, const uint16_t speed);
  static bool hotEnoughToExtrude(const uint8_t e);
  static void setTargetHotend(const celsius_t celsius, const uint8_t E_NAME);
  static void start_watching_hotend(const uint8_t E_NAME);
  static void setTargetBed(const celsius_t celsius);
  static void start_watching_bed();
};
extern Temperature thermalManager;
