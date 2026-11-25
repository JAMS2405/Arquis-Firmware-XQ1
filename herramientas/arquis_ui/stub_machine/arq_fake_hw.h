// Maquina simulada para probar el pegamento REAL (arquis_machine.cpp) y la pantalla (arquis_machine_ui.cpp) en el PC.
// No es Marlin: imita lo que esos archivos le piden (EEPROM, TMC2209, planificador, M500/M501/M502, G-code, impresion).
#pragma once
#include "Marlin/src/MarlinCore.h"
#include "Marlin/src/module/planner.h"
#include "Marlin/src/module/settings.h"
#include "Marlin/src/module/stepper.h"
#include "Marlin/src/gcode/gcode.h"
#include "Marlin/src/gcode/queue.h"
#include "Marlin/src/HAL/shared/eeprom_api.h"
#include "Marlin/src/lcd/extui/mks_ui/arquis_machine.h"

struct ArqFakeMarlinEE { bool valid; float v[ARQ_MP_COUNT]; bool mesh_valid; };   // lo que guarda M500 (simplificado)
namespace fake {
  extern uint8_t  ee[4096];            // EEPROM I2C de la placa
  extern int      ee_fail_after;       // >= 0: la escritura falla tras N bytes (corte de luz / error I2C)
  extern ArqFakeMarlinEE marlin_ee;    // datos de Marlin en la EEPROM
  extern bool     mesh_valid, leveling_on;
  extern int      saves, loads, resets, restores, kills, unhomes;
  extern bool     job, paused, blocks;
  extern uint32_t ms;
  extern uint8_t  homed;
  extern float    shaping[2];
  extern char     last_serial[256];
  extern int      gcode_rejected;      // ordenes consumidas por el gancho (rechazadas)
  extern int      gcode_run;           // ordenes que llegan a Marlin
  extern char     last_inject[128];    // ultimo bloque de G-code inyectado por la pantalla
  void reset_all();                    // placa nueva: EEPROM a 0xFF, drivers apagados, Marlin con lo compilado (XQ1)
  void boot();                         // arranque: settings.first_load() + tmc_init + primer arq_mc_idle
  void power_drivers(bool on);         // 24 V: al volver, los drivers arrancan con los valores de reinicio
  void set_marlin_ram(const arq_mc_cfg_t *c);   // valores mecanicos en RAM (como M92/M203/...)
  void get_marlin_ram(arq_mc_cfg_t *c);
  bool gcode(const char *line);        // como GcodeSuite::process_parsed_command: true si el gancho la consumio
  void tmc_init_all();                 // tmc_init de Marlin con el bit shaft del perfil (trinamic.cpp)
}
