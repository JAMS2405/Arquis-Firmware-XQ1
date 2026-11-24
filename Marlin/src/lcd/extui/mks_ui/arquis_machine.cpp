/**
 * ARQUIS - firmware unico XQ1 / Aquila D1: perfil de maquina en tiempo de ejecucion. Ver arquis_machine.h.
 */
#include "arquis_machine.h"

#if ARQ_HAS_MACHINE

#include "../../../MarlinCore.h"
#include "../../../module/settings.h"
#include "../../../module/planner.h"
#include "../../../module/stepper.h"
#include "../../../module/motion.h"
#include "../../../gcode/gcode.h"
#include "../../../HAL/shared/eeprom_api.h"
#if HAS_MEDIA
  #include "../../../sd/cardreader.h"
#endif
#if HAS_TRINAMIC_CONFIG
  #include "../../../module/stepper/trinamic.h"
#endif
#if HAS_LEVELING
  #include "../../../feature/bedlevel/bedlevel.h"
#endif

#include <string.h>

#if !AXIS_DRIVER_TYPE_X(TMC2209) || !AXIS_DRIVER_TYPE_Y(TMC2209) || !AXIS_DRIVER_TYPE_Z(TMC2209) || !AXIS_DRIVER_TYPE_E0(TMC2209)
  #error "ARQUIS firmware unico: las direcciones por perfil usan GCONF.shaft del TMC2209 por UART (X, Y, Z y E0)."
#endif
#if EXTRUDERS != 1 || NUM_AXES != 3
  #error "ARQUIS firmware unico: pensado para X, Y, Z y un extrusor."
#endif
#if X_HOME_DIR >= 0 || Y_HOME_DIR >= 0 || Z_HOME_DIR >= 0 || X_MIN_POS != 0 || Y_MIN_POS != 0 || Z_MIN_POS != 0
  #error "ARQUIS firmware unico: ambos modelos hacen homing al minimo con origen 0 (ver arq_mc_max_pos)."
#endif

// Logica de los finales en tiempo de ejecucion: Configuration.h (via Marlin/machine/machine.h) define
// X/Y/Z_MIN_ENDSTOP_INVERTING como un bit de esta variable; endstops.cpp la compara sin cambios: READ(pin) != INVERTING.
uint8_t arq_mc_es_inv = ARQ_MC_BX | ARQ_MC_BY | ARQ_MC_BZ;

static constexpr uint8_t COMPILED_DIR = (INVERT_X_DIR ? ARQ_MC_BX : 0) | (INVERT_Y_DIR ? ARQ_MC_BY : 0)
                                      | (INVERT_Z_DIR ? ARQ_MC_BZ : 0) | (INVERT_E0_DIR ? ARQ_MC_BE : 0);

static arq_mc_cfg_t cur;                 // en uso: modelo, flags (en RAM), mascaras y medidas (v[BED_X..Z_MAX])
static bool         rec_valid;
static uint8_t      boot_st = ARQ_MC_E_MAGIC;
static uint8_t      shaft_mask;          // drivers con GCONF.shaft = 1
static uint8_t      tmc_st = ARQ_TMC_UNKNOWN, tmc_bad, tmc_nocomm, tmc_seen;
static bool         test_mode;
static const char  *notice;

uint8_t arq_mc_compiled_dir() { return COMPILED_DIR; }

static void set_safe_defaults() {        // sin registro valido: la maquina mas pequena, logica y sentidos compilados
  memset(&cur, 0, sizeof(cur));
  cur.profile = ARQ_MC_NONE;
  cur.v[ARQ_MP_BED_X] = 230; cur.v[ARQ_MP_BED_Y] = 240; cur.v[ARQ_MP_Z_MAX] = 240;
  cur.es_inv = arq_mc_profile(ARQ_MC_XQ1)->es_inv;               // la logica que tenia el firmware hasta ahora
  cur.dir_inv = COMPILED_DIR;                                      // sin inversion en el driver
}

static void apply_masks() {
  arq_mc_es_inv = cur.es_inv;
  shaft_mask = arq_mc_shaft_mask(cur.dir_inv, COMPILED_DIR);
}

// ---- EEPROM ------------------------------------------------------------------------------------------------------
static bool ee_read(const int addr, uint8_t *buf) {
  memset(buf, 0xFF, ARQ_MC_REC_MAX);
  if (addr + ARQ_MC_REC_MAX > (int)persistentStore.capacity()) return false;
  persistentStore.access_start();
  int pos = addr; uint16_t crc = 0;
  const bool err = persistentStore.read_data(pos, buf, ARQ_MC_REC_MAX, &crc);
  persistentStore.access_finish();
  return !err;
}

static bool ee_write(const int addr, const uint8_t *buf, const size_t n) {
  if (addr + (int)n > (int)persistentStore.capacity()) return false;
  persistentStore.access_start();
  int pos = addr; uint16_t crc = 0;
  const bool err = persistentStore.write_data(pos, buf, n, &crc);
  persistentStore.access_finish();
  return !err;
}

void arq_mc_boot_load() {
  uint8_t a[ARQ_MC_REC_MAX], b[ARQ_MC_REC_MAX];
  const bool ra = ee_read(ARQ_MC_EE_A, a), rb = ee_read(ARQ_MC_EE_B, b);
  const int8_t k = arq_mc_rec_pick(ra ? a : nullptr, rb ? b : nullptr);
  if (k < 0) {
    boot_st = arq_mc_rec_check(a, sizeof(a));
    if (boot_st == ARQ_MC_E_MAGIC) { const uint8_t sb = arq_mc_rec_check(b, sizeof(b)); if (sb != ARQ_MC_E_MAGIC) boot_st = sb; }
    rec_valid = false;
    set_safe_defaults();
  }
  else {
    set_safe_defaults();
    arq_mc_rec_unpack(k ? b : a, &cur);
    rec_valid = true;
    boot_st = ARQ_MC_OK;
  }
  apply_masks();
  tmc_st = ARQ_TMC_UNKNOWN; tmc_seen = tmc_bad = tmc_nocomm = 0;
}

uint8_t arq_mc_write_record(const arq_mc_cfg_t *c) {
  uint8_t a[ARQ_MC_REC_MAX], b[ARQ_MC_REC_MAX];
  const bool ra = ee_read(ARQ_MC_EE_A, a), rb = ee_read(ARQ_MC_EE_B, b);
  const uint8_t *pa = ra ? a : nullptr, *pb = rb ? b : nullptr;
  const int8_t live = arq_mc_rec_pick(pa, pb), target = arq_mc_rec_target(pa, pb);
  const uint16_t seq = live < 0 ? 1 : (uint16_t)(arq_mc_rec_seq(live ? b : a) + 1);
  arq_mc_rec_t r; arq_mc_rec_pack(&r, c, seq);
  const int addr = target ? ARQ_MC_EE_B : ARQ_MC_EE_A;
  if (!ee_write(addr, (const uint8_t *)&r, sizeof(r))) return ARQ_MCS_E_RECORD;
  uint8_t chk[ARQ_MC_REC_MAX];                                 // verificacion: se relee y se comprueba
  if (!ee_read(addr, chk) || memcmp(chk, &r, sizeof(r)) || arq_mc_rec_check(chk, sizeof(chk)) != ARQ_MC_OK) return ARQ_MCS_E_RECORD;
  rec_valid = true; boot_st = ARQ_MC_OK;
  cur.profile = c->profile; cur.flags = c->flags;
  return ARQ_MCS_OK;
}

// ---- Estado ------------------------------------------------------------------------------------------------------
uint8_t arq_mc_profile_id()  { return rec_valid ? cur.profile : ARQ_MC_NONE; }
uint8_t arq_mc_flags()       { return cur.flags; }
uint8_t arq_mc_boot_status() { return boot_st; }
const char *arq_mc_name()    { const arq_mc_profile_t *p = arq_mc_profile(arq_mc_profile_id()); return p ? p->name : "ARQUIS"; }
bool arq_mc_needs_wizard()   { return !rec_valid || !arq_mc_profile(cur.profile) || !(cur.flags & ARQ_MCF_DONE); }
uint8_t arq_mc_tmc_state()   { return tmc_st; }
uint8_t arq_mc_tmc_mask()    { return tmc_bad | tmc_nocomm; }
uint8_t arq_mc_shaft_bit(const uint8_t axis) { return axis < 4 ? (shaft_mask >> axis) & 1 : 0; }
void arq_mc_test_mode(const bool on) { test_mode = on; }
void arq_mc_notice(const char *t) { notice = t; }
const char *arq_mc_take_notice() { const char *t = notice; notice = nullptr; return t; }

float arq_mc_max_pos(const uint8_t axis) {
  switch (axis) {
    case X_AXIS: return cur.v[ARQ_MP_BED_X];
    case Y_AXIS: return cur.v[ARQ_MP_BED_Y];
    case Z_AXIS: return cur.v[ARQ_MP_Z_MAX];
    default:     return 0;
  }
}

bool arq_mc_printing() { return printJobOngoing() || printingIsPaused() || planner.has_blocks_queued(); }

// strict: tambien el estado de los TMC aunque haya una impresion (G28, M24, M1000: justo despues de leer GCONF).
// Durante una impresion un G0/G1 NO se bloquea por el estado de los TMC: un fallo puntual de comunicacion no debe cortar
// la impresion; si la direccion deja de ser fiable, arq_mc_idle() detiene la maquina (kill) tras dos lecturas validas.
static uint8_t block_reason(const bool strict) {
  if (!test_mode) {
    if (!rec_valid || !arq_mc_profile(cur.profile)) return ARQ_BLK_NOPROFILE;
    if (!(cur.flags & ARQ_MCF_DONE)) return ARQ_BLK_WIZARD;
    if ((cur.flags & (ARQ_MCF_ES_OK | ARQ_MCF_DIR_OK)) != (ARQ_MCF_ES_OK | ARQ_MCF_DIR_OK)) return ARQ_BLK_UNVERIFIED;
  }
  if (strict || !arq_mc_printing()) {
    if (tmc_st == ARQ_TMC_BAD) return ARQ_BLK_TMC_BAD;
    if (shaft_mask && tmc_st != ARQ_TMC_OK) return ARQ_BLK_TMC_COMM;   // con drivers invertidos hay que confirmar GCONF
  }
  return ARQ_BLK_NONE;
}
uint8_t arq_mc_block_reason() { return block_reason(false); }
bool arq_mc_motion_ok() { return arq_mc_block_reason() == ARQ_BLK_NONE; }

const char *arq_mc_block_text(const uint8_t r) {
  switch (r) {
    case ARQ_BLK_NOPROFILE:  return "M\xC3\xA1quina sin configurar: completa el asistente.";
    case ARQ_BLK_WIZARD:     return "Asistente de configuraci\xC3\xB3n sin terminar.";
    case ARQ_BLK_UNVERIFIED: return "Faltan comprobar finales o motores.";
    case ARQ_BLK_TMC_COMM:   return "Sin lectura de los drivers TMC (GCONF).";
    case ARQ_BLK_TMC_BAD:    return "Un driver TMC perdi\xC3\xB3 su configuraci\xC3\xB3n.";
    default:                 return "";
  }
}

// ---- TMC2209: bit shaft y verificacion de GCONF ------------------------------------------------------------------
// 0 = correcto, 1 = sin comunicacion (CRC), 2 = driver reiniciado (UART/micropasos sin configurar), 3 = shaft distinto
template<typename T> static uint8_t tmc_check(T &st, const bool want_shaft) {
  const uint32_t g = st.GCONF();
  if (st.CRCerror) return 1;
  TMC2208_n::GCONF_t r; r.sr = g;
  if (!r.pdn_disable || !r.mstep_reg_select) return 2;
  return (bool(r.shaft) != want_shaft) ? 3 : 0;
}

static uint8_t check_axis_once(const uint8_t i) {
  switch (i) {
    case 0: return tmc_check(stepperX, arq_mc_shaft_bit(0));
    case 1: return tmc_check(stepperY, arq_mc_shaft_bit(1));
    case 2: return tmc_check(stepperZ, arq_mc_shaft_bit(2));
    default: return tmc_check(stepperE0, arq_mc_shaft_bit(3));
  }
}

static uint8_t check_axis(const uint8_t i) {             // hasta 3 intentos si no hay comunicacion (CRC / sin respuesta)
  uint8_t r = check_axis_once(i);
  for (uint8_t k = 0; r == 1 && k < 2; ++k) r = check_axis_once(i);
  return r;
}

static void record_axis(const uint8_t i, const uint8_t res) {
  const uint8_t bit = 1 << i;
  tmc_seen |= bit;
  if (res == 1) tmc_nocomm |= bit; else tmc_nocomm &= ~bit;
  if (res >= 2) tmc_bad |= bit; else tmc_bad &= ~bit;
  tmc_st = tmc_bad ? ARQ_TMC_BAD : (tmc_nocomm ? ARQ_TMC_NOCOMM : (tmc_seen == 0x0F ? ARQ_TMC_OK : ARQ_TMC_UNKNOWN));
}

void arq_mc_tmc_apply_shaft() {
  stepperX.shaft(arq_mc_shaft_bit(0));
  stepperY.shaft(arq_mc_shaft_bit(1));
  stepperZ.shaft(arq_mc_shaft_bit(2));
  stepperE0.shaft(arq_mc_shaft_bit(3));
}

uint8_t arq_mc_tmc_verify(const bool repair) {
  for (uint8_t i = 0; i < 4; ++i) record_axis(i, check_axis(i));
  if (tmc_st == ARQ_TMC_BAD && repair && !arq_mc_printing()) {
    restore_trinamic_drivers();          // reenvia TODOS los registros guardados (corriente, micropasos, chopper, GCONF con shaft)
    for (uint8_t i = 0; i < 4; ++i) record_axis(i, check_axis(i));
    if (tmc_st != ARQ_TMC_BAD) {
      set_all_unhomed();                 // la posicion ya no es fiable
      arq_mc_notice("Un driver de motor se hab\xC3\xAD" "a reiniciado. Se restaur\xC3\xB3 su configuraci\xC3\xB3n: haz home antes de mover.");
    }
  }
  return tmc_st;
}

void arq_mc_idle() {
  static millis_t next_ms = 0;
  static uint8_t  rr = 0;
  static bool     first = true;
  const millis_t ms = millis();
  if (!ELAPSED(ms, next_ms)) return;
  next_ms = ms + 1000;                   // una lectura de ~10 ms por segundo (X, Y, Z, E por turnos)
  if (first) { first = false; arq_mc_tmc_verify(true); return; }
  const uint8_t i = rr; rr = (rr + 1) & 3;
  const uint8_t res = check_axis(i);
  record_axis(i, res);
  if (res < 2) return;
  if (!arq_mc_printing()) { arq_mc_tmc_verify(true); return; }
  // Imprimiendo: se confirma con una segunda lectura. Se detiene si la DIRECCION puede ser incorrecta
  // (shaft distinto, o reinicio de un driver que debia estar invertido). Un reinicio de un driver sin invertir no
  // cambia la direccion (igual que el firmware anterior) y solo se registra.
  const uint8_t res2 = check_axis(i);
  record_axis(i, res2);
  if (res2 == 3 || (res2 == 2 && arq_mc_shaft_bit(i)))
    kill(F("Driver TMC: direccion no fiable"), nullptr, true);
}

// ---- Configuracion completa --------------------------------------------------------------------------------------
float arq_mc_shaping_min() {
  #if HAS_ZV_SHAPING
    return (float)shaping_min_freq;
  #else
    return 1;
  #endif
}

void arq_mc_read_current(arq_mc_cfg_t *c) {
  memset(c, 0, sizeof(*c));
  c->profile = arq_mc_profile_id(); c->flags = cur.flags; c->es_inv = cur.es_inv; c->dir_inv = cur.dir_inv;
  c->v[ARQ_MP_BED_X] = cur.v[ARQ_MP_BED_X]; c->v[ARQ_MP_BED_Y] = cur.v[ARQ_MP_BED_Y]; c->v[ARQ_MP_Z_MAX] = cur.v[ARQ_MP_Z_MAX];
  for (uint8_t i = 0; i < 4; ++i) {
    c->v[ARQ_MP_STEPS_X + i] = planner.settings.axis_steps_per_mm[i];
    c->v[ARQ_MP_FEED_X + i]  = planner.settings.max_feedrate_mm_s[i];
    c->v[ARQ_MP_ACC_X + i]   = planner.settings.max_acceleration_mm_per_s2[i];
  }
  c->v[ARQ_MP_ACC_PRINT] = planner.settings.acceleration;
  c->v[ARQ_MP_ACC_RETRACT] = planner.settings.retract_acceleration;
  c->v[ARQ_MP_ACC_TRAVEL] = planner.settings.travel_acceleration;
  c->v[ARQ_MP_JD] = TERN(HAS_JUNCTION_DEVIATION, planner.junction_deviation_mm, 0.05f);
  c->v[ARQ_MP_CUR_X] = stepperX.getMilliamps(); c->v[ARQ_MP_CUR_Y] = stepperY.getMilliamps();
  c->v[ARQ_MP_CUR_Z] = stepperZ.getMilliamps(); c->v[ARQ_MP_CUR_E] = stepperE0.getMilliamps();
  c->v[ARQ_MP_SHP_FX] = TERN(INPUT_SHAPING_X, stepper.get_shaping_frequency(X_AXIS), 0);
  c->v[ARQ_MP_SHP_FY] = TERN(INPUT_SHAPING_Y, stepper.get_shaping_frequency(Y_AXIS), 0);
  #if HAS_HOME_OFFSET
    c->v[ARQ_MP_OFS_X] = home_offset.x; c->v[ARQ_MP_OFS_Y] = home_offset.y; c->v[ARQ_MP_OFS_Z] = home_offset.z;
  #endif
  for (uint8_t p = 0; p < ARQ_MP_COUNT; ++p) c->v[p] = arq_mc_round(p, c->v[p]);
}

static void apply_mech(const arq_mc_cfg_t *c) {
  for (uint8_t i = 0; i < 4; ++i) {
    planner.settings.axis_steps_per_mm[i] = c->v[ARQ_MP_STEPS_X + i];
    planner.settings.max_feedrate_mm_s[i] = c->v[ARQ_MP_FEED_X + i];
    planner.settings.max_acceleration_mm_per_s2[i] = (uint32_t)c->v[ARQ_MP_ACC_X + i];
  }
  planner.settings.acceleration = c->v[ARQ_MP_ACC_PRINT];
  planner.settings.retract_acceleration = c->v[ARQ_MP_ACC_RETRACT];
  planner.settings.travel_acceleration = c->v[ARQ_MP_ACC_TRAVEL];
  #if HAS_JUNCTION_DEVIATION
    planner.junction_deviation_mm = c->v[ARQ_MP_JD];
  #endif
  planner.refresh_acceleration_rates();
  planner.refresh_positioning();
  TERN_(HAS_LINEAR_E_JERK, planner.recalculate_max_e_jerk());
  stepperX.rms_current((uint16_t)c->v[ARQ_MP_CUR_X]); stepperY.rms_current((uint16_t)c->v[ARQ_MP_CUR_Y]);
  stepperZ.rms_current((uint16_t)c->v[ARQ_MP_CUR_Z]); stepperE0.rms_current((uint16_t)c->v[ARQ_MP_CUR_E]);
  TERN_(INPUT_SHAPING_X, stepper.set_shaping_frequency(X_AXIS, c->v[ARQ_MP_SHP_FX]));
  TERN_(INPUT_SHAPING_Y, stepper.set_shaping_frequency(Y_AXIS, c->v[ARQ_MP_SHP_FY]));
  #if HAS_HOME_OFFSET
    set_home_offset(X_AXIS, c->v[ARQ_MP_OFS_X]); set_home_offset(Y_AXIS, c->v[ARQ_MP_OFS_Y]); set_home_offset(Z_AXIS, c->v[ARQ_MP_OFS_Z]);
  #endif
}

void arq_mc_apply(const arq_mc_cfg_t *c, const bool mech) {
  const bool bed_changed = arq_mc_mech_differs(c, &cur, ARQ_MP_BED_X) || arq_mc_mech_differs(c, &cur, ARQ_MP_BED_Y);
  const bool geo_changed = bed_changed || arq_mc_mech_differs(c, &cur, ARQ_MP_Z_MAX);
  const bool es_changed = c->es_inv != cur.es_inv, dir_changed = c->dir_inv != cur.dir_inv;
  cur.es_inv = c->es_inv; cur.dir_inv = c->dir_inv;
  cur.v[ARQ_MP_BED_X] = c->v[ARQ_MP_BED_X]; cur.v[ARQ_MP_BED_Y] = c->v[ARQ_MP_BED_Y]; cur.v[ARQ_MP_Z_MAX] = c->v[ARQ_MP_Z_MAX];
  if (es_changed)  cur.flags &= ~ARQ_MCF_ES_OK;                     // hay que volver a comprobar los finales
  if (dir_changed) cur.flags &= ~(ARQ_MCF_DIR_OK | ARQ_MCF_E_OK);   // y las direcciones
  apply_masks();
  if (dir_changed) { arq_mc_tmc_apply_shaft(); arq_mc_tmc_verify(false); }
  if (mech) apply_mech(c);
  LOOP_NUM_AXES(a) update_software_endstops((AxisEnum)a);
  if (geo_changed || es_changed || dir_changed) set_all_unhomed();
  #if HAS_LEVELING
    if (bed_changed) reset_bed_level();                             // la malla guardada era de otra cama
  #endif
}

uint8_t arq_mc_commit(const arq_mc_cfg_t *c, int *bad) {
  if (bad) *bad = -1;
  if (arq_mc_printing()) return ARQ_MCS_E_PRINTING;
  const int b = arq_mc_validate(c, arq_mc_shaping_min());
  if (b >= 0) { if (bad) *bad = b; return ARQ_MCS_E_INVALID; }
  arq_mc_apply(c, true);
  cur.flags = c->flags;
  if (!settings.save()) return ARQ_MCS_E_MARLIN;
  return arq_mc_write_record(c);                                   // ultimo paso: punto de confirmacion
}

void arq_mc_on_reset() {
  if (!arq_mc_profile(arq_mc_profile_id())) return;                // sin modelo: quedan los de compilacion (no se mueve)
  arq_mc_cfg_t c; arq_mc_factory(arq_mc_profile_id(), &c);         // offsets 0, como deja reset()
  apply_mech(&c);                                                  // (postprocess() recalcula despues planificador y limites)
}

uint8_t arq_mc_restore_all() {
  if (arq_mc_printing()) return ARQ_MCS_E_PRINTING;
  const uint8_t prof = arq_mc_profile_id();
  if (!arq_mc_profile(prof)) return ARQ_MCS_E_INVALID;
  arq_mc_cfg_t c; arq_mc_read_current(&c);
  arq_mc_take_geometry(prof, &c);                                  // medidas, finales y direcciones de fabrica
  c.flags = cur.flags;
  if (c.es_inv != cur.es_inv)   c.flags &= ~ARQ_MCF_ES_OK;
  if (c.dir_inv != cur.dir_inv) c.flags &= ~(ARQ_MCF_DIR_OK | ARQ_MCF_E_OK);
  arq_mc_apply(&c, false);
  settings.reset();                                                // M502 (con el gancho: mecanica de fabrica del modelo)
  if (!settings.save()) return ARQ_MCS_E_MARLIN;
  return arq_mc_write_record(&c);
}

// ---- G-code -----------------------------------------------------------------------------------------------------
static bool is_motion(const char L, const int n) {
  if (L == 'G') switch (n) {
    case 0: case 1: case 2: case 3: case 5: case 12: case 26: case 27: case 28: case 29: case 30: case 33: case 34:
    case 35: case 38: case 42: case 61: case 76: case 425: return true;
    default: return false;
  }
  if (L == 'M') switch (n) {
    case 24: case 32: case 48: case 125: case 290: case 600: case 701: case 702: case 1000: return true;
    default: return false;
  }
  return false;
}

static void report() {
  SERIAL_ECHOLNPGM("ARQUIS: ", arq_mc_name(), " reg=", boot_st, " flags=", cur.flags, " fin=", cur.es_inv, " dir=", cur.dir_inv,
                   " shaft=", shaft_mask, " tmc=", tmc_st, " bloqueo=", arq_mc_block_reason());
}

bool arq_mc_gcode_hook(const char L, const int n) {
  if (L == 'M' && n == ARQ_MC_GCODE) {                             // M9050 [R]
    if (parser.seen('R')) {
      if (arq_mc_printing()) SERIAL_ERROR_MSG("ARQUIS: no se puede durante una impresion");
      else {
        arq_mc_cfg_t c; arq_mc_read_current(&c);
        c.profile = cur.profile; c.flags = 0;                       // se conserva el modelo; el asistente vuelve a aparecer
        const bool ok = arq_mc_profile(c.profile) ? arq_mc_write_record(&c) == ARQ_MCS_OK : true;
        if (!arq_mc_profile(c.profile)) rec_valid = false;
        cur.flags = 0;
        SERIAL_ECHOLNPGM(ok ? "ARQUIS: asistente reactivado (vuelve a Inicio o reinicia)" : "ARQUIS: error al escribir la EEPROM");
        arq_mc_notice("Configuraci\xC3\xB3n de m\xC3\xA1quina reiniciada por M9050 R: completa el asistente.");
      }
    }
    report();
    return true;
  }
  if (!is_motion(L, n)) return false;
  if (test_mode && !(L == 'G' && (n == 0 || n == 1 || n == 28))) {   // durante las pruebas del asistente solo G0/G1/G28
    SERIAL_ERROR_MSG("ARQUIS: orden no permitida durante las pruebas del asistente");
    return true;
  }
  const bool strict = (L == 'G' && n == 28) || (L == 'M' && (n == 24 || n == 1000));
  if (strict) arq_mc_tmc_verify(!arq_mc_printing());               // GCONF justo antes de hacer home o empezar a imprimir
  const uint8_t r = block_reason(strict);
  if (r == ARQ_BLK_NONE) return false;
  SERIAL_ERROR_START(); SERIAL_ECHOLNPGM("ARQUIS: movimiento bloqueado. ", arq_mc_block_text(r));
  #if HAS_MEDIA
    if (card.isFileOpen() && IS_SD_PRINTING()) card.abortFilePrintSoon();   // una impresion no puede seguir sin moverse
  #endif
  arq_mc_notice(arq_mc_block_text(r));
  return true;
}

#endif // ARQ_HAS_MACHINE
