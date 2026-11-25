/**
 * ARQUIS - firmware unico: perfiles de maquina (logica pura). Ver arquis_machine_logic.h y docs/arquis/MAQUINAS.md.
 *
 * Valores de fabrica, extraidos de la configuracion real (preprocesador sobre Configuration.h / Configuration_adv.h):
 *  - Arquis XQ1: Configuration.h + Marlin/machine/xq1.h (Z 5 mm/s y 100 mm/s2: PRUEBA pendiente de validacion fisica).
 *  - Aquila D1:  Configuration.h + Marlin/machine/aquila_d1.h (tomado de su Configuration.h original en el commit a415386).
 * tests/machines/test_profiles.py comprueba que esta tabla coincide con esos archivos.
 */
#include "arquis_machine_logic.h"

#include <math.h>
#include <string.h>

#define P(x) (1UL << (x))

static const arq_mc_profile_t profiles[ARQ_MC_NPROF] = {
  { "Arquis XQ1", "300 x 300 x 450",
    ARQ_MC_BX | ARQ_MC_BY | ARQ_MC_BZ,            // X/Y/Z_MIN_ENDSTOP_INVERTING true
    ARQ_MC_BX | ARQ_MC_BY,                        // INVERT_X_DIR, INVERT_Y_DIR true; Z, E0 false
    { 300, 300, 450,
      160, 160, 791.74f, 748.72f,
      500, 500, 5, 60,
      4500, 4500, 100, 10000,
      3000, 2000, 4000,
      0.05f,
      650, 650, 650, 700,
      35, 25,
      0, 0, 0 },
    P(ARQ_MP_FEED_Z) | P(ARQ_MP_ACC_Z) },         // xq1.h: prueba "Z no subia", pendiente de validacion fisica
  { "Aquila D1", "230 x 240 x 240",
    ARQ_MC_BZ,                                    // X/Y_MIN_ENDSTOP_INVERTING false; Z true
    ARQ_MC_BE,                                    // INVERT_X/Y/Z_DIR false; INVERT_E0_DIR true
    { 230, 240, 240,
      160, 160, 803.41f, 781.08f,
      500, 500, 45, 60,
      4500, 3500, 150, 10000,
      3000, 2000, 4000,
      0.05f,
      650, 650, 650, 700,
      35, 25,
      0, 0, 0 },
    0 }
};

static const char * const pnames[ARQ_MP_COUNT] = {
  "Ancho X", "Fondo Y", "Altura Z",
  "Pasos X", "Pasos Y", "Pasos Z", "Pasos E",
  "Vel. m\xC3\xA1x. X", "Vel. m\xC3\xA1x. Y", "Vel. m\xC3\xA1x. Z", "Vel. m\xC3\xA1x. E",
  "Acel. m\xC3\xA1x. X", "Acel. m\xC3\xA1x. Y", "Acel. m\xC3\xA1x. Z", "Acel. m\xC3\xA1x. E",
  "Acel. impresi\xC3\xB3n", "Acel. retracci\xC3\xB3n", "Acel. viaje",
  "Junction dev.",
  "Corriente X", "Corriente Y", "Corriente Z", "Corriente E",
  "Shaping X", "Shaping Y",
  "Offset X", "Offset Y", "Offset Z"
};

static const char *unit_of(uint8_t p) {
  if (p <= ARQ_MP_Z_MAX) return "mm";
  if (p <= ARQ_MP_STEPS_E) return "pasos/mm";
  if (p <= ARQ_MP_FEED_E) return "mm/s";
  if (p <= ARQ_MP_ACC_TRAVEL) return "mm/s\xC2\xB2";
  if (p == ARQ_MP_JD) return "mm";
  if (p <= ARQ_MP_CUR_E) return "mA";
  if (p <= ARQ_MP_SHP_FY) return "Hz";
  return "mm";
}

const arq_mc_profile_t * arq_mc_profile(uint8_t id) { return (id >= 1 && id <= ARQ_MC_NPROF) ? &profiles[id - 1] : nullptr; }
const char * arq_mc_param_name(uint8_t p) { return p < ARQ_MP_COUNT ? pnames[p] : ""; }
const char * arq_mc_param_unit(uint8_t p) { return p < ARQ_MP_COUNT ? unit_of(p) : ""; }

bool arq_mc_range(uint8_t profile, uint8_t p, float shaping_min, arq_mc_range_t *o) {
  const arq_mc_profile_t *pr = arq_mc_profile(profile);
  if (!o || p >= ARQ_MP_COUNT) return false;
  o->zero_ok = false;
  switch (p) {
    case ARQ_MP_BED_X: case ARQ_MP_BED_Y: case ARQ_MP_Z_MAX:
      if (!pr) return false;
      *o = { 100, pr->v[p] + 10, 1, false }; break;                   // nunca mas de 10 mm sobre la medida de fabrica
    case ARQ_MP_STEPS_X: case ARQ_MP_STEPS_Y: *o = { 40, 400, 2, false }; break;
    case ARQ_MP_STEPS_Z:                      *o = { 200, 1600, 2, false }; break;
    case ARQ_MP_STEPS_E:                      *o = { 50, 2000, 2, false }; break;
    case ARQ_MP_FEED_X: case ARQ_MP_FEED_Y:   *o = { 50, 500, 0, false }; break;
    case ARQ_MP_FEED_Z:                       *o = { 1, 45, 1, false }; break;   // 45 mm/s ~ 35.600 pasos/s con 791 pasos/mm
    case ARQ_MP_FEED_E:                       *o = { 10, 120, 0, false }; break;
    case ARQ_MP_ACC_X: case ARQ_MP_ACC_Y:     *o = { 100, 10000, 0, false }; break;
    case ARQ_MP_ACC_Z:                        *o = { 10, 500, 0, false }; break;
    case ARQ_MP_ACC_E:                        *o = { 500, 10000, 0, false }; break;
    case ARQ_MP_ACC_PRINT: case ARQ_MP_ACC_RETRACT: case ARQ_MP_ACC_TRAVEL:
                                              *o = { 100, 10000, 0, false }; break;
    case ARQ_MP_JD:                           *o = { 0.01f, 0.3f, 3, false }; break;
    case ARQ_MP_CUR_X: case ARQ_MP_CUR_Y: case ARQ_MP_CUR_Z: case ARQ_MP_CUR_E:
                                              *o = { 300, 1000, 0, false }; break;   // TMC2209 con Rsense 0,11 ohm
    case ARQ_MP_SHP_FX: case ARQ_MP_SHP_FY:   *o = { shaping_min > 1 ? shaping_min : 1, 150, 1, true }; break;   // 0 = desactivado
    case ARQ_MP_OFS_X: case ARQ_MP_OFS_Y:     *o = { -50, 50, 2, false }; break;
    case ARQ_MP_OFS_Z:                        *o = { -20, 20, 2, false }; break;
    default: return false;
  }
  return true;
}

bool arq_mc_value_ok(uint8_t profile, uint8_t p, float v, float shaping_min) {
  arq_mc_range_t r;
  if (!arq_mc_range(profile, p, shaping_min, &r) || isnan(v) || isinf(v)) return false;
  if (r.zero_ok && v == 0) return true;
  const float eps = (r.dec >= 3) ? 0.0005f : 0.005f;
  return v >= r.lo - eps && v <= r.hi + eps;
}

int arq_mc_validate(const arq_mc_cfg_t *c, float shaping_min) {
  if (!c || !arq_mc_profile(c->profile) || c->es_inv > 7 || c->dir_inv > 15) return ARQ_MP_COUNT;
  for (uint8_t p = 0; p < ARQ_MP_COUNT; ++p)
    if (!arq_mc_value_ok(c->profile, p, c->v[p], shaping_min)) return p;
  return -1;
}

void arq_mc_factory(uint8_t profile, arq_mc_cfg_t *c) {
  const arq_mc_profile_t *pr = arq_mc_profile(profile);
  if (!c || !pr) return;
  memcpy(c->v, pr->v, sizeof(c->v));
  c->profile = profile; c->flags = 0; c->es_inv = pr->es_inv; c->dir_inv = pr->dir_inv;
}

void arq_mc_take_geometry(uint8_t profile, arq_mc_cfg_t *c) {
  const arq_mc_profile_t *pr = arq_mc_profile(profile);
  if (!c || !pr) return;
  c->profile = profile; c->es_inv = pr->es_inv; c->dir_inv = pr->dir_inv;
  for (uint8_t p = ARQ_MP_BED_X; p <= ARQ_MP_Z_MAX; ++p) c->v[p] = pr->v[p];
}

bool arq_mc_mech_differs(const arq_mc_cfg_t *a, const arq_mc_cfg_t *b, uint8_t p) {
  if (!a || !b || p >= ARQ_MP_COUNT) return false;
  return fabsf(a->v[p] - b->v[p]) > 0.0005f;
}

uint8_t arq_mc_detect(float sz, float se) {
  for (uint8_t i = 1; i <= ARQ_MC_NPROF; ++i) {
    const arq_mc_profile_t *pr = arq_mc_profile(i);
    if (fabsf(sz - pr->v[ARQ_MP_STEPS_Z]) < 0.01f && fabsf(se - pr->v[ARQ_MP_STEPS_E]) < 0.01f) return i;
  }
  return ARQ_MC_NONE;
}

float arq_mc_round(uint8_t p, float v) {
  arq_mc_range_t r;
  if (!arq_mc_range(ARQ_MC_XQ1, p, 1, &r)) return v;
  float m = 1; for (uint8_t i = 0; i < r.dec; ++i) m *= 10;
  return roundf(v * m) / m;
}

// ---- Registro en la EEPROM --------------------------------------------------------------------------------------

uint16_t arq_mc_crc16(const uint8_t *d, size_t n) {         // CRC-16/CCITT-FALSE
  uint16_t crc = 0xFFFF;
  while (n--) {
    crc ^= (uint16_t)(*d++) << 8;
    for (uint8_t i = 0; i < 8; ++i) crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
  }
  return crc;
}

static uint16_t dmm(float mm) { const float d = mm * 10.0f + 0.5f; return d < 0 ? 0 : (d > 65535 ? 65535 : (uint16_t)d); }

void arq_mc_rec_pack(arq_mc_rec_t *r, const arq_mc_cfg_t *c, uint16_t seq) {
  memset(r, 0, sizeof(*r));
  r->magic = ARQ_MC_MAGIC; r->fmt = ARQ_MC_FMT; r->len = sizeof(arq_mc_rec_t); r->seq = seq;
  r->profile = c->profile; r->flags = c->flags; r->es_inv = c->es_inv; r->dir_inv = c->dir_inv;
  r->bed_x = dmm(c->v[ARQ_MP_BED_X]); r->bed_y = dmm(c->v[ARQ_MP_BED_Y]); r->z_max = dmm(c->v[ARQ_MP_Z_MAX]);
  r->crc = arq_mc_crc16((const uint8_t *)r, sizeof(*r) - 2);
}

// Longitud minima de cada formato conocido (migracion: un formato posterior solo puede anadir campos al final)
static uint8_t min_len(uint8_t fmt) { return fmt == 1 ? (uint8_t)sizeof(arq_mc_rec_t) : 0; }

uint8_t arq_mc_rec_check(const uint8_t *raw, size_t n) {
  if (!raw || n < 8) return ARQ_MC_E_LEN;
  arq_mc_rec_t r; memset(&r, 0, sizeof(r));
  uint32_t magic; memcpy(&magic, raw, 4);
  if (magic != ARQ_MC_MAGIC) return ARQ_MC_E_MAGIC;
  const uint8_t fmt = raw[4], len = raw[5];
  if (fmt == 0 || fmt > ARQ_MC_FMT) return ARQ_MC_E_FMT;            // formato futuro: no se interpreta (no se borra)
  if (len < min_len(fmt) || len > n || len > ARQ_MC_REC_MAX) return ARQ_MC_E_LEN;
  uint16_t stored; memcpy(&stored, raw + len - 2, 2);
  if (arq_mc_crc16(raw, len - 2) != stored) return ARQ_MC_E_CRC;
  memcpy(&r, raw, sizeof(r) < len ? sizeof(r) : len);
  if (!arq_mc_profile(r.profile)) return ARQ_MC_E_PROFILE;
  if ((r.flags & ~(ARQ_MCF_DONE | ARQ_MCF_ES_OK | ARQ_MCF_DIR_OK | ARQ_MCF_E_OK)) || r.es_inv > 7 || r.dir_inv > 15) return ARQ_MC_E_FLAGS;
  if (!arq_mc_value_ok(r.profile, ARQ_MP_BED_X, r.bed_x / 10.0f, 1) || !arq_mc_value_ok(r.profile, ARQ_MP_BED_Y, r.bed_y / 10.0f, 1)
      || !arq_mc_value_ok(r.profile, ARQ_MP_Z_MAX, r.z_max / 10.0f, 1)) return ARQ_MC_E_RANGE;
  return ARQ_MC_OK;
}

void arq_mc_rec_unpack(const uint8_t *raw, arq_mc_cfg_t *c) {
  arq_mc_rec_t r; memset(&r, 0, sizeof(r));
  memcpy(&r, raw, sizeof(r) < raw[5] ? sizeof(r) : raw[5]);
  c->profile = r.profile; c->flags = r.flags; c->es_inv = r.es_inv; c->dir_inv = r.dir_inv;
  c->v[ARQ_MP_BED_X] = r.bed_x / 10.0f; c->v[ARQ_MP_BED_Y] = r.bed_y / 10.0f; c->v[ARQ_MP_Z_MAX] = r.z_max / 10.0f;
}

uint16_t arq_mc_rec_seq(const uint8_t *raw) { uint16_t s; memcpy(&s, raw + 6, 2); return s; }

int8_t arq_mc_rec_pick(const uint8_t *a, const uint8_t *b) {
  const bool va = a && arq_mc_rec_check(a, ARQ_MC_REC_MAX) == ARQ_MC_OK,
             vb = b && arq_mc_rec_check(b, ARQ_MC_REC_MAX) == ARQ_MC_OK;
  if (va && vb) return (int16_t)(arq_mc_rec_seq(b) - arq_mc_rec_seq(a)) > 0 ? 1 : 0;   // la mas reciente (con vuelta de contador)
  return va ? 0 : (vb ? 1 : -1);
}

int8_t arq_mc_rec_target(const uint8_t *a, const uint8_t *b) { return arq_mc_rec_pick(a, b) == 0 ? 1 : 0; }

uint8_t arq_mc_shaft_mask(uint8_t dir_inv, uint8_t compiled_dir) { return (uint8_t)((dir_inv ^ compiled_dir) & 0x0F); }

// ---- Eleccion del modelo -----------------------------------------------------------------------------------------
uint8_t arq_mc_keep_check(uint8_t pick, bool marlin_ok, uint8_t rec_profile, uint8_t detected) {
  if (!arq_mc_profile(pick)) return ARQ_KEEP_NO_MODEL;
  if (!marlin_ok) return ARQ_KEEP_NO_EEPROM;
  if (detected != ARQ_MC_NONE) return detected == pick ? ARQ_KEEP_OK : ARQ_KEEP_OTHER_MODEL;   // pasos de fabrica de un modelo
  if (!arq_mc_profile(rec_profile)) return ARQ_KEEP_UNCONFIRMED;                              // calibrados y sin registro
  return rec_profile == pick ? ARQ_KEEP_OK : ARQ_KEEP_OTHER_MODEL;                            // calibrados: decide el registro
}

bool arq_mc_choose(uint8_t pick, uint8_t keep_status, bool want_keep, const arq_mc_cfg_t *saved, arq_mc_cfg_t *out) {
  if (!out || !arq_mc_profile(pick)) return false;
  const bool keep = want_keep && keep_status == ARQ_KEEP_OK && saved;
  if (keep) {
    *out = *saved;
    if (saved->profile != pick) arq_mc_take_geometry(pick, out);   // sin registro de este modelo: geometria de fabrica
    out->profile = pick;
  }
  else {
    arq_mc_factory(pick, out);
    if (keep_status == ARQ_KEEP_OK && saved)                       // M206 del usuario: solo si los datos son de este modelo
      for (uint8_t p = ARQ_MP_OFS_X; p <= ARQ_MP_OFS_Z; ++p)
        if (arq_mc_value_ok(pick, p, saved->v[p], 1)) out->v[p] = saved->v[p];
  }
  if (keep) for (uint8_t p = ARQ_MP_OFS_X; p <= ARQ_MP_OFS_Z; ++p)
    if (!arq_mc_value_ok(pick, p, out->v[p], 1)) out->v[p] = 0;
  out->flags = 0;
  return keep;
}

// ---- Verificacion del TMC2209 ------------------------------------------------------------------------------------
uint8_t arq_mc_tmc_classify(uint32_t g, bool no_valid_reply, bool want_shaft) {
  if (no_valid_reply) return ARQ_TMC_R_NOREPLY;
  if (!(g & ARQ_GCONF_PDN_DISABLE) || !(g & ARQ_GCONF_MSTEP_REG)) return ARQ_TMC_R_RESET;   // valores de reinicio (UART/micropasos)
  return (bool(g & ARQ_GCONF_SHAFT) != want_shaft) ? ARQ_TMC_R_SHAFT : ARQ_TMC_R_OK;
}
