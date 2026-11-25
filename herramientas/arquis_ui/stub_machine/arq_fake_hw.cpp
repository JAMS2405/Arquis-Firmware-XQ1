#include "arq_fake_hw.h"
#include "Marlin/src/module/motion.h"
#include "Marlin/src/feature/bedlevel/bedlevel.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace fake {
  uint8_t ee[4096]; int ee_fail_after = -1; ArqFakeMarlinEE marlin_ee;
  bool mesh_valid, leveling_on; int saves, loads, resets, restores, kills, unhomes;
  bool job, paused, blocks; uint32_t ms = 1000; uint8_t homed; float shaping[2] = { 35, 25 };
  char last_serial[256], last_inject[128]; int gcode_rejected, gcode_run;
  static float compiled[ARQ_MP_COUNT];
}
using namespace fake;

// ---- Marlin --------------------------------------------------------------------------------------------------------
uint32_t millis() { return fake::ms; }
bool printJobOngoing() { return fake::job; }
bool printingIsPaused() { return fake::paused; }
void kill(FSTR_P const, FSTR_P const, const bool) { ++fake::kills; }
void arq_fake_serial(const char *s) { snprintf(last_serial, sizeof(last_serial), "%s", s); }
Planner planner; Planner::planner_settings_t Planner::settings; float Planner::junction_deviation_mm = 0.05f; bool Planner::leveling_active;
bool Planner::has_blocks_queued() { return fake::blocks; }
void Planner::refresh_acceleration_rates() {}
void Planner::refresh_positioning() {}
Stepper stepper; uint16_t shaping_min_freq = 25;
float Stepper::get_shaping_frequency(const AxisEnum a) { return shaping[a]; }
void Stepper::set_shaping_frequency(const AxisEnum a, const float f) { shaping[a] = f; }
ArqFakeTMC stepperX, stepperY, stepperZ, stepperE0;
void restore_trinamic_drivers() { ++restores; stepperX.push(); stepperY.push(); stepperZ.push(); stepperE0.push(); }
xyz_pos_stub home_offset;
void set_home_offset(const AxisEnum a, const float v) { (&home_offset.x)[a] = v; }
bool axis_was_homed(const AxisEnum a) { return (homed >> a) & 1; }
void set_axis_unhomed(const AxisEnum a) { homed &= ~(1 << a); }
void set_all_unhomed() { homed = 0; ++unhomes; }
void update_software_endstops(const AxisEnum) {}
bool leveling_is_valid() { return mesh_valid; }
void set_bed_leveling_enabled(const bool e) { leveling_on = e && mesh_valid; planner.leveling_active = leveling_on; }
void reset_bed_level() { mesh_valid = false; set_bed_leveling_enabled(false); }
ArqFakeParser parser;

// EEPROM (semantica de eeprom_wired.cpp)
PersistentStore persistentStore;
size_t PersistentStore::capacity() { return sizeof(ee); }
bool PersistentStore::access_start() { return false; }
bool PersistentStore::access_finish() { return false; }
bool PersistentStore::write_data(int &pos, const uint8_t *v, size_t n, uint16_t *) {
  while (n--) {
    if (ee_fail_after == 0) return true;
    if (ee_fail_after > 0) --ee_fail_after;
    ee[pos++] = *v++;
  }
  return false;
}
bool PersistentStore::read_data(int &pos, uint8_t *v, size_t n, uint16_t *, const bool w) {
  while (n--) { const uint8_t c = ee[pos++]; if (w) *v = c; ++v; }
  return false;
}

// M500 / M501 / M502 simplificados: lo que importa aqui es el orden de las llamadas al pegamento (como settings.cpp)
void fake::set_marlin_ram(const arq_mc_cfg_t *c) {
  for (int i = 0; i < 4; ++i) {
    planner.settings.axis_steps_per_mm[i] = c->v[ARQ_MP_STEPS_X + i];
    planner.settings.max_feedrate_mm_s[i] = c->v[ARQ_MP_FEED_X + i];
    planner.settings.max_acceleration_mm_per_s2[i] = (uint32_t)c->v[ARQ_MP_ACC_X + i];
  }
  planner.settings.acceleration = c->v[ARQ_MP_ACC_PRINT]; planner.settings.retract_acceleration = c->v[ARQ_MP_ACC_RETRACT];
  planner.settings.travel_acceleration = c->v[ARQ_MP_ACC_TRAVEL]; planner.junction_deviation_mm = c->v[ARQ_MP_JD];
  stepperX.ma = (uint16_t)c->v[ARQ_MP_CUR_X]; stepperY.ma = (uint16_t)c->v[ARQ_MP_CUR_Y];
  stepperZ.ma = (uint16_t)c->v[ARQ_MP_CUR_Z]; stepperE0.ma = (uint16_t)c->v[ARQ_MP_CUR_E];
  shaping[0] = c->v[ARQ_MP_SHP_FX]; shaping[1] = c->v[ARQ_MP_SHP_FY];
  home_offset.x = c->v[ARQ_MP_OFS_X]; home_offset.y = c->v[ARQ_MP_OFS_Y]; home_offset.z = c->v[ARQ_MP_OFS_Z];
}
void fake::get_marlin_ram(arq_mc_cfg_t *c) { arq_mc_read_current(c); }
MarlinSettings settings;
static void tmc_init_one(ArqFakeTMC &t, uint8_t axis) {   // trinamic.cpp: pdn_disable, mstep_reg_select, shaft del perfil
  t.GCONF((uint32_t)(ARQ_GCONF_PDN_DISABLE | ARQ_GCONF_MSTEP_REG | (arq_mc_shaft_bit(axis) ? ARQ_GCONF_SHAFT : 0)));
}
void fake::tmc_init_all() { tmc_init_one(stepperX, 0); tmc_init_one(stepperY, 1); tmc_init_one(stepperZ, 2); tmc_init_one(stepperE0, 3); }
bool MarlinSettings::validate() { return marlin_ee.valid; }
bool MarlinSettings::save() {
  ++saves; arq_mc_cfg_t c; arq_mc_read_current(&c);
  marlin_ee.valid = true; memcpy(marlin_ee.v, c.v, sizeof(c.v)); marlin_ee.mesh_valid = mesh_valid;
  return true;
}
void MarlinSettings::reset() {             // M502: valores compilados, sin malla, tmc_init y gancho ARQUIS
  ++resets; arq_mc_cfg_t c; memset(&c, 0, sizeof(c)); memcpy(c.v, compiled, sizeof(c.v));
  fake::set_marlin_ram(&c); reset_bed_level();
  fake::tmc_init_all();
  arq_mc_on_reset();
}
bool MarlinSettings::load() {              // M501: el pegamento lee su registro ANTES que nada (settings.cpp)
  ++loads; arq_mc_boot_load();
  if (!marlin_ee.valid) { reset(); return false; }
  arq_mc_cfg_t c; memset(&c, 0, sizeof(c)); memcpy(c.v, marlin_ee.v, sizeof(c.v)); fake::set_marlin_ram(&c);
  mesh_valid = marlin_ee.mesh_valid; set_bed_leveling_enabled(mesh_valid);
  fake::tmc_init_all();                    // _load() -> reset_stepper_drivers() -> tmc_init
  return true;
}

// G-code: el gancho primero (gcode.cpp), luego "Marlin" (solo lo necesario: G28 deja el eje con origen)
GCodeQueue queue;
bool GCodeQueue::has_commands_queued() { return false; }
void GCodeQueue::inject(FSTR_P const g) {
  char buf[128]; snprintf(buf, sizeof(buf), "%s", g); snprintf(last_inject, sizeof(last_inject), "%s", g);
  for (char *l = strtok(buf, "\n"); l; l = strtok(nullptr, "\n")) fake::gcode(l);
}
bool fake::gcode(const char *line) {
  const char L = line[0]; const int n = atoi(line + 1);
  const char *sp = line + 1; while (*sp >= '0' && *sp <= '9') ++sp;
  parser.args = sp;
  const bool consumed = arq_mc_gcode_hook(L, n);
  if (consumed) { if (!(L == 'M' && n == 9050)) ++gcode_rejected; return true; }
  ++gcode_run;
  if (L == 'G' && n == 28) { if (!*sp || !strpbrk(sp, "XYZ")) homed = 7; else { if (strchr(sp, 'X')) homed |= 1; if (strchr(sp, 'Y')) homed |= 2; if (strchr(sp, 'Z')) homed |= 4; } }
  return false;
}

void fake::reset_all() {
  memset(ee, 0xFF, sizeof(ee)); ee_fail_after = -1;
  arq_mc_cfg_t x; arq_mc_factory(ARQ_MC_XQ1, &x); memcpy(compiled, x.v, sizeof(compiled));   // lo compilado = XQ1
  compiled[ARQ_MP_FEED_Z] = 45; compiled[ARQ_MP_ACC_Z] = 150;                                 // (Configuration.h sin xq1.h)
  memset(&marlin_ee, 0, sizeof(marlin_ee));
  mesh_valid = leveling_on = false; saves = loads = resets = restores = kills = unhomes = 0;
  job = paused = blocks = false; homed = 0; gcode_rejected = gcode_run = 0;
  for (ArqFakeTMC *t : { &stepperX, &stepperY, &stepperZ, &stepperE0 }) *t = ArqFakeTMC();
}
void fake::power_drivers(bool on) {
  for (ArqFakeTMC *t : { &stepperX, &stepperY, &stepperZ, &stepperE0 }) { t->noreply = !on; if (on) t->chip = 0x101; }
}
void fake::boot() { settings.load(); ms += 2000; arq_mc_idle(); }
