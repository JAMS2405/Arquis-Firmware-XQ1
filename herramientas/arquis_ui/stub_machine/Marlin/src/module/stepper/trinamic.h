#pragma once
#include <stdint.h>
// Driver TMC2209 simulado con la misma interfaz que TMCStepper 0.8.10 (ver tests/machines/glue_test.cpp):
//  - 'chip': registro GCONF real del driver; 'shadow': copia que guarda la libreria (GCONF_register.sr)
//  - GCONF(): como TMC2208Stepper::read(): sin respuesta valida -> CRCerror = true y devuelve 0
struct ArqFakeTMC {
  uint32_t chip = 0x101, shadow = 0;  // 0x101 = valores de reinicio del TMC2209 (I_scale_analog, multistep_filt)
  bool     noreply = false, CRCerror = false;
  int      reads = 0, writes = 0, fail_reads = 0;   // fail_reads: las N proximas lecturas no traen respuesta valida
  uint16_t ma = 650;
  uint32_t GCONF() { ++reads; if (noreply || fail_reads) { if (fail_reads) --fail_reads; CRCerror = true; return 0; } CRCerror = false; return chip; }
  void     GCONF(uint32_t v) { shadow = v; ++writes; if (!noreply) chip = v; }
  void     shaft(bool b) { GCONF(b ? (shadow | 8u) : (shadow & ~8u)); }
  void     push() { GCONF(shadow); }
  uint16_t getMilliamps() { return ma; }
  void     rms_current(uint16_t m) { ma = m; }
};
extern ArqFakeTMC stepperX, stepperY, stepperZ, stepperE0;
void restore_trinamic_drivers();
