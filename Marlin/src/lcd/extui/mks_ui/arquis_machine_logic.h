/**
 * ARQUIS - firmware unico: perfiles de maquina (logica pura, sin Marlin ni LVGL; se prueba en el PC con tests/machines).
 *
 * Un solo BIN sirve para Arquis XQ1 y Aquila D1. Lo que cambia entre ellas vive en:
 *  - la TABLA de perfiles de fabrica de este modulo (arquis_machine_logic.cpp): valores de fabrica de cada maquina;
 *  - un REGISTRO ARQUIS propio en la EEPROM I2C (2 copias, CRC16): modelo, medidas, logica de finales, direcciones, estado;
 *  - la EEPROM de Marlin, que ya guarda pasos/mm, velocidades, aceleraciones, corrientes, Input Shaping, PID, offsets y malla.
 * La electronica (placa, TMC2209, micropasos, chopper, termistores, temperaturas, protecciones) es comun y no se configura aqui.
 */
#pragma once

#include <stdint.h>
#include <stddef.h>

#define ARQ_MC_MAGIC       0x4D515241UL     // "ARQM" (little endian)
#define ARQ_MC_FMT         1                // version del formato del registro
#define ARQ_MC_REC_MAX     64               // bytes maximos que se leen de una copia (un formato futuro puede crecer)

// Modelos
enum { ARQ_MC_NONE = 0, ARQ_MC_XQ1 = 1, ARQ_MC_AQUILA_D1 = 2, ARQ_MC_NPROF = 2 };

// Estado (flags del registro)
enum {
  ARQ_MCF_DONE   = 0x01,   // asistente completado
  ARQ_MCF_ES_OK  = 0x02,   // finales de carrera X/Y/Z comprobados a mano
  ARQ_MCF_DIR_OK = 0x04,   // direcciones X/Y/Z comprobadas con movimiento corto
  ARQ_MCF_E_OK   = 0x08    // direccion del extrusor comprobada (opcional)
};

// Bits de eje en las mascaras (es_inv: X, Y, Z; dir_inv: X, Y, Z, E0)
enum { ARQ_MC_BX = 0x01, ARQ_MC_BY = 0x02, ARQ_MC_BZ = 0x04, ARQ_MC_BE = 0x08 };

// Parametros configurables (todos se manejan como float en la copia de trabajo)
enum {
  ARQ_MP_BED_X, ARQ_MP_BED_Y, ARQ_MP_Z_MAX,                                   // mm
  ARQ_MP_STEPS_X, ARQ_MP_STEPS_Y, ARQ_MP_STEPS_Z, ARQ_MP_STEPS_E,             // pasos/mm
  ARQ_MP_FEED_X, ARQ_MP_FEED_Y, ARQ_MP_FEED_Z, ARQ_MP_FEED_E,                 // mm/s
  ARQ_MP_ACC_X, ARQ_MP_ACC_Y, ARQ_MP_ACC_Z, ARQ_MP_ACC_E,                     // mm/s2
  ARQ_MP_ACC_PRINT, ARQ_MP_ACC_RETRACT, ARQ_MP_ACC_TRAVEL,                    // mm/s2
  ARQ_MP_JD,                                                                  // mm
  ARQ_MP_CUR_X, ARQ_MP_CUR_Y, ARQ_MP_CUR_Z, ARQ_MP_CUR_E,                     // mA
  ARQ_MP_SHP_FX, ARQ_MP_SHP_FY,                                               // Hz (0 = desactivado)
  ARQ_MP_OFS_X, ARQ_MP_OFS_Y, ARQ_MP_OFS_Z,                                   // mm (M206)
  ARQ_MP_COUNT
};

// Copia de trabajo de una configuracion completa
typedef struct {
  float   v[ARQ_MP_COUNT];
  uint8_t profile, flags, es_inv, dir_inv;
} arq_mc_cfg_t;

// Perfil de fabrica (tabla en Flash)
typedef struct {
  const char *name;           // nombre de maquina (pantalla, M115)
  const char *size_txt;       // "300 x 300 x 450"
  uint8_t     es_inv, dir_inv;
  float       v[ARQ_MP_COUNT];
  uint32_t    pending;        // bits (1 << ARQ_MP_*) de valores provisionales pendientes de validacion fisica
} arq_mc_profile_t;

// Rango permitido de un parametro
typedef struct { float lo, hi; uint8_t dec; bool zero_ok; } arq_mc_range_t;

// Registro en la EEPROM (32 B). El CRC16 cubre todo lo anterior (len - 2 bytes).
typedef struct __attribute__((packed)) {
  uint32_t magic;
  uint8_t  fmt, len;
  uint16_t seq;
  uint8_t  profile, flags, es_inv, dir_inv;
  uint16_t bed_x, bed_y, z_max;   // decimas de mm
  uint8_t  reserved[12];          // a cero; un formato futuro puede usarlos
  uint16_t crc;
} arq_mc_rec_t;
static_assert(sizeof(arq_mc_rec_t) == 32, "registro ARQUIS de 32 bytes");

// Errores
enum {
  ARQ_MC_OK = 0,
  ARQ_MC_E_MAGIC, ARQ_MC_E_FMT, ARQ_MC_E_LEN, ARQ_MC_E_CRC, ARQ_MC_E_PROFILE, ARQ_MC_E_RANGE, ARQ_MC_E_FLAGS
};

const arq_mc_profile_t * arq_mc_profile(uint8_t id);           // nullptr si no existe
const char * arq_mc_param_name(uint8_t p);                     // nombre corto en espanol
const char * arq_mc_param_unit(uint8_t p);
bool    arq_mc_range(uint8_t profile, uint8_t p, float shaping_min, arq_mc_range_t *out);
bool    arq_mc_value_ok(uint8_t profile, uint8_t p, float v, float shaping_min);
int     arq_mc_validate(const arq_mc_cfg_t *c, float shaping_min);   // -1 = todo valido; si no, el primer parametro invalido
                                                                     // (ARQ_MP_COUNT = modelo/mascaras invalidos)
void    arq_mc_factory(uint8_t profile, arq_mc_cfg_t *c);              // todo de fabrica (offsets 0, flags 0)
void    arq_mc_take_geometry(uint8_t profile, arq_mc_cfg_t *c);        // solo modelo, medidas, finales y direcciones de fabrica
bool    arq_mc_mech_differs(const arq_mc_cfg_t *a, const arq_mc_cfg_t *b, uint8_t p);
uint8_t arq_mc_detect(float steps_z, float steps_e);                   // modelo cuyos pasos Z y E coinciden (0 = ninguno)
float   arq_mc_round(uint8_t p, float v);                              // redondeo a las decimales del parametro

uint16_t arq_mc_crc16(const uint8_t *d, size_t n);
void     arq_mc_rec_pack(arq_mc_rec_t *r, const arq_mc_cfg_t *c, uint16_t seq);
uint8_t  arq_mc_rec_check(const uint8_t *raw, size_t n);               // ARQ_MC_OK o motivo
void     arq_mc_rec_unpack(const uint8_t *raw, arq_mc_cfg_t *c);        // solo tras rec_check OK: modelo, flags, mascaras y medidas
int8_t   arq_mc_rec_pick(const uint8_t *a, const uint8_t *b);           // copia a usar: 0 = A, 1 = B, -1 = ninguna valida
uint16_t arq_mc_rec_seq(const uint8_t *raw);
int8_t   arq_mc_rec_target(const uint8_t *a, const uint8_t *b);         // copia a escribir: la que NO es la vigente (0 = A, 1 = B)

uint8_t  arq_mc_shaft_mask(uint8_t dir_inv, uint8_t compiled_dir);    // bits que el driver TMC debe invertir (bit shaft)

// ---- Eleccion del modelo (paso 1 del asistente) -----------------------------------------------------------------
// Los valores mecanicos que tiene Marlin (pasos/mm, velocidades, aceleraciones, corrientes, Input Shaping, JD) y los
// offsets M206 solo se pueden CONSERVAR si se confirma que son del modelo elegido:
//  - la EEPROM de Marlin es valida, y
//  - los pasos Z/E coinciden exactamente con los de fabrica del modelo elegido (deteccion), o
//    no coinciden con ningun modelo (calibrados) pero el registro ARQUIS confirma que se guardaron con ESE modelo.
// Si los pasos coinciden con los de fabrica del OTRO modelo, nunca se conservan (aunque el registro diga lo contrario).
enum { ARQ_KEEP_OK = 0, ARQ_KEEP_NO_EEPROM, ARQ_KEEP_OTHER_MODEL, ARQ_KEEP_UNCONFIRMED, ARQ_KEEP_NO_MODEL };
uint8_t  arq_mc_keep_check(uint8_t pick, bool marlin_ok, uint8_t rec_profile, uint8_t detected);
// Copia de trabajo para el modelo elegido. 'saved' = lo que hay ahora (geometria del registro, mecanica de Marlin).
//  - keep (solo si keep_status == ARQ_KEEP_OK): mecanica de 'saved'; medidas, finales y sentidos de 'saved' si el registro ya
//    era de ese modelo (saved->profile == pick) y, si no, los de fabrica del modelo.
//  - si no: TODO de fabrica del modelo elegido.
//  - M206: los de 'saved' solo si keep_status == ARQ_KEEP_OK y estan dentro de rango; si no, 0.
// flags = 0 (hay que repetir las comprobaciones). Devuelve true si se conservo la mecanica.
bool     arq_mc_choose(uint8_t pick, uint8_t keep_status, bool want_keep, const arq_mc_cfg_t *saved, arq_mc_cfg_t *out);

// ---- Verificacion del TMC2209 (GCONF) ---------------------------------------------------------------------------
// TMCStepper 0.8.10 (TMC2208Stepper::read): si no llega una respuesta valida (sin respuesta, sincronismo o cuerpo fuera de
// plazo de 8 ms, CRC distinto o CRC = 0) tras 2 intentos, pone CRCerror = true y devuelve 0. CRCerror NO significa por
// fuerza "driver desconectado": tambien ruido o una respuesta corrupta. Por eso se llama "sin respuesta valida".
#define ARQ_GCONF_SHAFT        (1UL << 3)
#define ARQ_GCONF_PDN_DISABLE  (1UL << 6)
#define ARQ_GCONF_MSTEP_REG    (1UL << 7)
enum { ARQ_TMC_R_OK = 0, ARQ_TMC_R_NOREPLY, ARQ_TMC_R_RESET, ARQ_TMC_R_SHAFT };
uint8_t  arq_mc_tmc_classify(uint32_t gconf, bool no_valid_reply, bool want_shaft);
