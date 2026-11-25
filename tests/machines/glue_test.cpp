// Prueba del pegamento REAL del firmware unico (Marlin/src/lcd/extui/mks_ui/arquis_machine.cpp) sobre una maquina simulada
// (herramientas/arquis_ui/stub_machine/arq_fake_hw.*): EEPROM I2C en memoria, TMC2209 con la semantica de lectura de
// TMCStepper 0.8.10 y fallos inyectables (sin respuesta valida, driver reiniciado, bit shaft distinto), M500/M501/M502 y
// el gancho de G-code. Es una SIMULACION: no sustituye a las pruebas fisicas. Uso: tests/machines/run.sh
#include "arq_fake_hw.h"
#include "Marlin/src/module/motion.h"
#include "Marlin/src/feature/bedlevel/bedlevel.h"
#include <stdio.h>
#include <math.h>

static int fails = 0, checks = 0;
#define CHECK(c, ...) do { ++checks; if (c) printf("[glue] OK    "); else { printf("[glue] FALLA "); ++fails; } printf(__VA_ARGS__); printf("\n"); } while (0)

static void tick(int n = 1) { for (int i = 0; i < n; ++i) { fake::ms += 1000; arq_mc_idle(); } }
static void cfg_of(uint8_t prof, arq_mc_cfg_t *c) {
  arq_mc_factory(prof, c);
  c->flags = ARQ_MCF_DONE | ARQ_MCF_ES_OK | ARQ_MCF_DIR_OK | ARQ_MCF_E_OK;
}
static int reads() { return stepperX.reads + stepperY.reads + stepperZ.reads + stepperE0.reads; }
static bool shaft_chip(ArqFakeTMC &t) { return t.chip & ARQ_GCONF_SHAFT; }
// Maquina configurada y arrancada con el modelo 'prof' (malla valida, ejes con origen)
static void setup(uint8_t prof) {
  fake::reset_all();
  fake::boot();
  arq_mc_cfg_t c; cfg_of(prof, &c);
  fake::mesh_valid = true; set_bed_leveling_enabled(true);
  arq_mc_mesh_ref(&c.v[ARQ_MP_BED_X]);
  arq_mc_commit(&c, nullptr);
  fake::boot();                                             // reinicio: lee el registro y los drivers
  fake::homed = 7;
}

int main() {
  setvbuf(stdout, nullptr, _IONBF, 0);

  // ===== Arranque sin perfil =====
  fake::reset_all(); fake::boot();
  CHECK(arq_mc_needs_wizard() && arq_mc_profile_id() == 0, "placa nueva: sin perfil, el asistente es obligatorio");
  CHECK(fake::gcode("G1 X10") && arq_mc_block_reason() == ARQ_BLK_NOPROFILE, "sin perfil: G1 rechazado");
  CHECK(fake::gcode("G28") && fake::gcode("M24"), "sin perfil: G28 y M24 rechazados");
  CHECK(!fake::gcode("M104 S200"), "sin perfil: las ordenes que no mueven pasan (M104)");

  // ===== Guardar Aquila: registro, bit shaft y persistencia =====
  setup(ARQ_MC_AQUILA_D1);
  CHECK(arq_mc_profile_id() == ARQ_MC_AQUILA_D1 && !arq_mc_needs_wizard(), "Aquila guardado: persiste tras reiniciar");
  CHECK(shaft_chip(stepperX) && shaft_chip(stepperY) && !shaft_chip(stepperZ) && shaft_chip(stepperE0),
        "Aquila: GCONF.shaft en X, Y y E0 (no en Z) tras tmc_init");
  CHECK(arq_mc_tmc_state() == ARQ_TMC_OK, "Aquila: los 4 drivers confirmados en el arranque");
  CHECK(fabsf(planner.settings.axis_steps_per_mm[2] - 803.41f) < 1e-3 && fabsf(planner.settings.axis_steps_per_mm[3] - 781.08f) < 1e-3,
        "Aquila: pasos Z/E del modelo");

  // ===== P5: movimiento manual con drivers correctos =====
  int r0 = reads();
  CHECK(!fake::gcode("G1 X10"), "P5: G1 X10 con el driver X confirmado se mueve");
  CHECK(reads() - r0 == 1, "P5: se lee solo el driver que mueve la orden (X): %d lectura(s)", reads() - r0);
  r0 = reads(); fake::gcode("G1 Z5");
  CHECK(reads() - r0 == 0, "P5: Z no esta invertido en el Aquila: sin lecturas (sin pausas innecesarias)");
  fake::blocks = true; r0 = reads(); CHECK(!fake::gcode("G1 X20") && reads() == r0, "P5: con el planificador ocupado no se lee (movimiento en curso)");
  fake::blocks = false;

  // ===== P5: driver X reiniciado (corte de los 24 V) justo antes de un movimiento manual =====
  stepperX.chip = 0x101;                                    // valores de reinicio: shaft = 0 -> giraria al reves
  const int rs0 = fake::restores;
  CHECK(fake::gcode("G1 X10"), "P5: driver X reiniciado: el movimiento se BLOQUEA (no gira al reves)");
  CHECK(fake::restores == rs0 + 1 && shaft_chip(stepperX) && arq_mc_tmc_state() == ARQ_TMC_OK,
        "P5: se restaura la configuracion (shaft = 1) y se vuelve a verificar");
  CHECK(fake::homed == 0, "P5: los ejes quedan sin origen");
  CHECK(strstr(fake::last_serial, "restaur") != nullptr, "P5: mensaje claro por serie (%s)", fake::last_serial);
  CHECK(!fake::gcode("G1 X10"), "P5: el siguiente movimiento ya se permite");

  // ===== P5: bit shaft distinto (sin reinicio) =====
  stepperY.chip &= ~ARQ_GCONF_SHAFT;
  CHECK(fake::gcode("G1 Y10") && shaft_chip(stepperY), "P5: shaft de Y distinto: bloqueado y corregido");
  CHECK(!fake::gcode("G1 Y10"), "P5: despues, Y se mueve");

  // ===== P5: extrusor (E0 invertido en el Aquila) =====
  stepperE0.chip = 0x101;
  CHECK(fake::gcode("G1 E5"), "P5: extrusor reiniciado: la extrusion manual se bloquea");
  CHECK(!fake::gcode("G1 E5"), "P5: tras restaurar, extruye");

  // ===== P5: driver sin respuesta valida =====
  stepperX.noreply = true;
  CHECK(fake::gcode("G1 X10"), "P5: X sin respuesta valida: bloqueado (no se asume que conserva la configuracion)");
  CHECK(arq_mc_tmc_state() == ARQ_TMC_NOCOMM, "P5: estado 'sin respuesta' (no 'desconectado')");
  CHECK(fake::gcode("G1 Z5"), "P5: con un driver sin confirmar, tampoco se mueve otro eje");
  CHECK(fake::gcode("G28"), "P5: G28 bloqueado sin respuesta del driver X");
  stepperX.noreply = false; stepperX.chip = 0x101;          // vuelve la comunicacion; el driver se habia reiniciado
  CHECK(fake::gcode("G1 X10") && shaft_chip(stepperX), "P5: al volver la respuesta, se recupera y se pide repetir");
  CHECK(!fake::gcode("G1 X10"), "P5: y luego se mueve");
  // fallo puntual (ruido): la primera lectura falla, la segunda responde bien -> se mueve sin restaurar
  int rsp = fake::restores; stepperX.fail_reads = 1;
  CHECK(!fake::gcode("G1 X10") && fake::restores == rsp, "P5: un fallo puntual de lectura se confirma con una segunda lectura y no bloquea");

  // ===== P5: babystep, homing, Wi-Fi =====
  r0 = reads(); fake::gcode("M290 Z0.05");
  CHECK(reads() - r0 == 3, "P5: babystep sin impresion: se comprueban los drivers invertidos (X, Y, E0)");
  r0 = reads(); CHECK(!fake::gcode("G28") && reads() - r0 == 4, "P5: G28 lee los 4 drivers antes de hacer home");
  fake::job = true; r0 = reads();
  CHECK(!fake::gcode("G1 X10 F3000") && reads() - r0 == 4, "P5: impresion empezada sin M24 (Wi-Fi/USB): la primera orden lee los 4");
  r0 = reads(); fake::gcode("G1 X20"); fake::gcode("G1 E1");
  CHECK(reads() == r0, "P5: durante la impresion los G1 no esperan lecturas");
  r0 = reads(); fake::gcode("M290 Z0.02");
  CHECK(reads() == r0, "P5: babystep durante la impresion: sin lecturas (Z no depende del bit shaft)");

  // ===== P5: durante la impresion =====
  int k0 = fake::kills;
  stepperX.noreply = true; tick(3);
  CHECK(fake::kills == k0, "P5: imprimiendo, un driver sin respuesta NO corta la impresion");
  stepperX.noreply = false;
  r0 = stepperZ.reads; tick(6);
  CHECK(stepperZ.reads == r0, "P5: imprimiendo en el Aquila solo se vigilan X, Y y E0 (los que dependen del bit shaft)");
  stepperY.chip = 0x101; tick(3);
  CHECK(fake::kills == k0 + 1, "P5: imprimiendo, driver invertido reiniciado: parada de seguridad (kill) en <= 3 s");
  stepperY.push(); fake::job = false;

  // ===== P5: XQ1 (ningun driver invertido) =====
  setup(ARQ_MC_XQ1);
  CHECK(!shaft_chip(stepperX) && !shaft_chip(stepperY) && !shaft_chip(stepperZ) && !shaft_chip(stepperE0), "XQ1: ningun bit shaft");
  r0 = reads(); CHECK(!fake::gcode("G1 X10") && reads() == r0, "XQ1: el movimiento manual no espera lecturas");
  stepperX.noreply = true;
  CHECK(!fake::gcode("G1 X10"), "XQ1: sin respuesta de un driver se puede mover (el sentido no depende del driver)");
  stepperX.noreply = false;
  fake::job = true; k0 = fake::kills; stepperX.chip = 0x101; tick(5);
  CHECK(fake::kills == k0, "XQ1: imprimiendo, un driver reiniciado no cambia el sentido: no se corta (como antes)");
  fake::job = false; tick(5);
  CHECK(arq_mc_tmc_state() == ARQ_TMC_OK && fake::restores > 0, "XQ1: al terminar, el driver reiniciado se restaura");

  // ===== P5: 24 V apagados y encendidos sin imprimir (Aquila) =====
  setup(ARQ_MC_AQUILA_D1);
  fake::power_drivers(false); tick(2);
  CHECK(fake::gcode("G1 X10"), "24 V apagados: sin respuesta, el Aquila no se mueve");
  fake::power_drivers(true);                                // los drivers arrancan sin configurar (shaft = 0)
  int rs1 = fake::restores; tick(4);
  CHECK(fake::restores > rs1 && shaft_chip(stepperX) && arq_mc_tmc_state() == ARQ_TMC_OK, "24 V de nuevo: se detecta el reinicio y se restaura solo");
  const char *nt = arq_mc_take_notice();
  CHECK(nt && strstr(nt, "reiniciado"), "24 V de nuevo: aviso en pantalla");

  // ===== P3: cambios en RAM sin guardar =====
  setup(ARQ_MC_AQUILA_D1);
  arq_mc_cfg_t s, t; arq_mc_read_saved(&s); t = s; t.v[ARQ_MP_Z_MAX] = 200;
  arq_mc_test_mode(true); arq_mc_apply(&t, false); arq_mc_test_mode(false);   // lo que hace "Probar" del menu
  CHECK(arq_mc_ram_unsaved() && arq_mc_block_reason() == ARQ_BLK_UNSAVED, "P3: medidas de una prueba aplicadas en RAM: movimiento bloqueado");
  CHECK(fake::gcode("G1 X10"), "P3: G1 rechazado mientras haya cambios sin guardar");
  arq_mc_cfg_t rs; arq_mc_read_saved(&rs);
  CHECK(rs.v[ARQ_MP_Z_MAX] == 240, "P3: read_saved da lo guardado, no lo de la prueba");
  CHECK(fake::mesh_valid && !planner.leveling_active, "P2: la prueba con otras medidas NO borra la malla (solo la suspende)");
  const int sv0 = fake::saves, ld0 = fake::loads;
  arq_mc_revert_ram();
  CHECK(!arq_mc_ram_unsaved() && arq_mc_max_pos(Z_AXIS) == 240 && arq_mc_motion_ok(), "P3: descartar vuelve a lo guardado y se puede mover");
  CHECK(fake::saves == sv0 && fake::loads == ld0, "P3: descartar no hace M500 ni M501 (no toca otros ajustes de Marlin)");
  CHECK(planner.leveling_active && fake::mesh_valid, "P3: la nivelacion vuelve a estar activa");
  t = s; t.dir_inv ^= ARQ_MC_BX;
  arq_mc_test_mode(true); arq_mc_apply(&t, false); arq_mc_test_mode(false);
  CHECK(!shaft_chip(stepperX) && !arq_mc_motion_ok(), "P3: probar otro sentido de X lo aplica en el driver y bloquea fuera de la prueba");
  arq_mc_revert_ram();
  CHECK(shaft_chip(stepperX) && arq_mc_motion_ok() && (arq_mc_flags() & ARQ_MCF_DIR_OK), "P3: descartar restaura el sentido guardado y sus comprobaciones");
  // M501 (por ejemplo desde Memoria) tambien vuelve a lo guardado
  arq_mc_test_mode(true); arq_mc_apply(&t, false); arq_mc_test_mode(false);
  settings.load();
  CHECK(shaft_chip(stepperX) && !arq_mc_ram_unsaved(), "P3: M501 tambien vuelve al registro");

  // ===== P2: malla =====
  setup(ARQ_MC_AQUILA_D1);
  arq_mc_cfg_t m; arq_mc_read_saved(&m);
  CHECK(!arq_mc_mesh_will_erase(&m), "P2: guardar con las mismas medidas no borra la malla");
  m.v[ARQ_MP_BED_X] = 220;
  CHECK(arq_mc_mesh_will_erase(&m), "P2: guardar con otro ancho borraria la malla (la pantalla lo advierte)");
  arq_mc_commit(&m, nullptr);
  CHECK(!fake::mesh_valid, "P2: al guardar otras medidas la malla se borra");
  arq_mc_read_saved(&m); fake::mesh_valid = true; set_bed_leveling_enabled(true);
  m.v[ARQ_MP_FEED_X] = 400; arq_mc_commit(&m, nullptr);
  CHECK(fake::mesh_valid, "P2: guardar otros valores (no medidas) conserva la malla");

  // ===== EEPROM: fallo de escritura del registro =====
  setup(ARQ_MC_AQUILA_D1);
  arq_mc_cfg_t e; arq_mc_read_saved(&e); e.v[ARQ_MP_Z_MAX] = 230;
  fake::ee_fail_after = 5;
  CHECK(arq_mc_commit(&e, nullptr) == ARQ_MCS_E_RECORD, "EEPROM: corte a mitad del registro: error");
  CHECK(!arq_mc_motion_ok(), "EEPROM: tras el error, no se mueve con la geometria sin guardar");
  fake::ee_fail_after = -1; fake::boot();
  CHECK(arq_mc_profile_id() == ARQ_MC_AQUILA_D1 && arq_mc_max_pos(Z_AXIS) == 240, "EEPROM: al reiniciar queda la copia anterior intacta");
  CHECK(arq_mc_motion_ok(), "EEPROM: y se puede mover");
  // registro borrado por completo (EEPROM corrupta)
  memset(fake::ee + ARQ_MC_EE_A, 0x00, 256); fake::boot();
  CHECK(arq_mc_needs_wizard() && fake::gcode("G1 X1"), "EEPROM: registro corrupto: asistente y movimiento bloqueado");

  // ===== M502 y M9050 R =====
  setup(ARQ_MC_AQUILA_D1);
  planner.settings.axis_steps_per_mm[3] = 790; settings.save();      // E calibrado y guardado
  const int s502 = fake::saves;
  settings.reset();                                                  // M502 sin M500
  CHECK(fabsf(planner.settings.axis_steps_per_mm[3] - 781.08f) < 1e-3, "M502: mecanica de fabrica del modelo ACTIVO (Aquila), no la compilada");
  CHECK(fake::saves == s502 && arq_mc_profile_id() == ARQ_MC_AQUILA_D1, "M502: no guarda (sin M500) y no toca el registro");
  CHECK(!fake::mesh_valid, "M502: como en Marlin, borra la malla EN RAM");
  settings.load();
  CHECK(fabsf(planner.settings.axis_steps_per_mm[3] - 790) < 1e-3 && fake::mesh_valid, "M502 sin M500: con M501 / reinicio vuelve todo (malla y E calibrado)");
  const int s9050 = fake::saves;
  fake::gcode("M9050 R");
  CHECK(arq_mc_needs_wizard() && fake::saves == s9050, "M9050 R: vuelve al asistente sin escribir la EEPROM de Marlin");
  fake::boot();
  CHECK(arq_mc_needs_wizard() && arq_mc_profile_id() == ARQ_MC_AQUILA_D1 && fake::mesh_valid
        && fabsf(planner.settings.axis_steps_per_mm[3] - 790) < 1e-3, "M9050 R: tras reiniciar conserva modelo, malla y pasos (PID y offsets igual)");

  printf("[glue] %d comprobaciones, %d fallos\n", checks, fails);
  return fails ? 1 : 0;
}
