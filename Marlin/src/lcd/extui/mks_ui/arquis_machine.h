/**
 * ARQUIS - firmware unico XQ1 / Aquila D1: perfil de maquina en tiempo de ejecucion (pegamento con Marlin).
 *
 *  - El modelo, las medidas, la logica de finales y las direcciones se guardan en un REGISTRO ARQUIS de la EEPROM I2C
 *    (2 copias de 32 B en ARQ_MC_EE_A / ARQ_MC_EE_B, fuera de los datos de Marlin) y se cargan al principio de
 *    MarlinSettings::load(), antes de que Marlin inicialice los drivers.
 *  - Los valores mecanicos (pasos/mm, velocidades, aceleraciones, corrientes, Input Shaping, offsets) los guarda Marlin
 *    en SU EEPROM (M500), cuya estructura no cambia. MarlinSettings::reset() (M502 o EEPROM invalida) aplica los de
 *    fabrica del modelo activo.
 *  - Direcciones: sin tocar stepper.cpp. Las del XQ1 son las compiladas; para otro sentido se invierte el bit
 *    GCONF.shaft del TMC2209 (tmc_init). Se verifica leyendo GCONF por UART antes de G28 / imprimir, y de forma
 *    periodica; si el driver se reinicio o el bit no coincide, se repara (si no se imprime) o se detiene.
 *  - Sin perfil valido y comprobado, el pegamento de G-code rechaza los movimientos (ver arq_mc_gcode_hook).
 */
#pragma once

#include "arquis_machine_logic.h"
#include "../../../inc/MarlinConfigPre.h"

#if HAS_TFT_LVGL_UI && ENABLED(ARQUIS_UI)
  #define ARQ_HAS_MACHINE 1
#else
  #define ARQ_HAS_MACHINE 0
#endif

// Direcciones del registro ARQUIS en la EEPROM I2C (4 KB). Marlin usa EEPROM_OFFSET (100) + sizeof(SettingsData) (689 B
// en esta configuracion): 100..788. settings.cpp comprueba en compilacion que no llega a ARQ_MC_EE_A.
// En un chip de 2 KB estas direcciones se reflejarian en 0x700 / 0x780, que tampoco usa Marlin.
#define ARQ_MC_EE_A     0x0F00
#define ARQ_MC_EE_B     0x0F80
#define ARQ_MC_GCODE    9050           // M9050: estado; M9050 R: volver al asistente (recuperacion por USB / Wi-Fi / microSD)

// Estado de la verificacion de los TMC2209
enum { ARQ_TMC_UNKNOWN = 0, ARQ_TMC_OK, ARQ_TMC_NOCOMM, ARQ_TMC_BAD };

// Motivo por el que no se permite mover (0 = se permite)
enum { ARQ_BLK_NONE = 0, ARQ_BLK_NOPROFILE, ARQ_BLK_WIZARD, ARQ_BLK_UNVERIFIED, ARQ_BLK_TMC_COMM, ARQ_BLK_TMC_BAD,
       ARQ_BLK_UNSAVED,          // medidas, finales o sentidos aplicados en RAM (pruebas) distintos de lo guardado
       ARQ_BLK_TMC_RESTORED };   // el driver no se pudo confirmar antes de mover: se restauro y se comprobo; repetir

#ifdef __cplusplus

uint8_t     arq_mc_profile_id();                 // modelo activo (ARQ_MC_NONE sin perfil valido)
uint8_t     arq_mc_flags();
uint8_t     arq_mc_boot_status();                // resultado de la carga del registro (ARQ_MC_OK o motivo)
const char *arq_mc_name();                       // nombre del modelo activo ("ARQUIS" sin perfil)
float       arq_mc_max_pos(const uint8_t axis);  // X/Y = medida de la cama, Z = altura (min = 0 en ambos modelos)
bool        arq_mc_needs_wizard();               // sin perfil valido o asistente sin completar
bool        arq_mc_motion_ok();                  // se puede mover/imprimir
uint8_t     arq_mc_block_reason();
const char *arq_mc_block_text(uint8_t reason);
uint8_t     arq_mc_tmc_state();
uint8_t     arq_mc_tmc_mask();                   // drivers con problema en la ultima verificacion (bits X, Y, Z, E)
uint8_t     arq_mc_shaft_bit(const uint8_t axis);// para tmc_init: 1 = invertir ese driver
uint8_t     arq_mc_compiled_dir();               // INVERT_X/Y/Z/E0_DIR compilados (mascara)
bool        arq_mc_printing();                   // impresion activa o en pausa / movimiento en curso

void    arq_mc_boot_load();                      // MarlinSettings::load()
void    arq_mc_on_reset();                       // MarlinSettings::reset(): valores de fabrica del modelo activo
bool    arq_mc_gcode_hook(const char letter, const int code);   // GcodeSuite: true = orden consumida (rechazada o M9050)
void    arq_mc_idle();                           // tarea periodica (verificacion de los TMC, avisos)

uint8_t arq_mc_tmc_verify(const bool repair);    // lee GCONF de X, Y, Z y E0 ahora; devuelve ARQ_TMC_*
void    arq_mc_tmc_apply_shaft();                // escribe el bit shaft del modelo activo en los 4 drivers

void    arq_mc_read_current(arq_mc_cfg_t *c);    // configuracion en uso (Marlin + geometria en RAM)
void    arq_mc_read_saved(arq_mc_cfg_t *c);      // mecanica de Marlin + geometria, finales, sentidos y flags del REGISTRO guardado
float   arq_mc_shaping_min();
// Aplica en RAM (no guarda): medidas, finales, direcciones y, si 'mech', los valores mecanicos. Si algo de la geometria,
// finales o direcciones cambia, los ejes pasan a "sin origen" (hay que hacer G28). NO borra la malla: si cambian las
// medidas, la nivelacion se suspende en RAM hasta guardar (arq_mc_commit decide si la malla se borra) o descartar.
void    arq_mc_apply(const arq_mc_cfg_t *c, const bool mech);
bool    arq_mc_ram_unsaved();                    // medidas, finales o sentidos en RAM distintos de los del registro
void    arq_mc_revert_ram();                     // vuelve a lo del registro (sin tocar la EEPROM ni los valores de Marlin)
bool    arq_mc_mesh_valid();                     // hay una malla de nivelacion valida en RAM
bool    arq_mc_mesh_will_erase(const arq_mc_cfg_t *c);   // guardar 'c' borraria la malla (medidas distintas de las de la malla)
void    arq_mc_mesh_ref(const float *dims);      // primer arranque: medidas para las que se hizo la malla (nullptr = desconocidas)
// Valida, aplica, guarda la EEPROM de Marlin (M500) y, al final, el registro ARQUIS. Devuelve ARQ_MCS_*.
enum { ARQ_MCS_OK = 0, ARQ_MCS_E_INVALID, ARQ_MCS_E_PRINTING, ARQ_MCS_E_MARLIN, ARQ_MCS_E_RECORD };
uint8_t arq_mc_commit(const arq_mc_cfg_t *c, int *bad_param);
uint8_t arq_mc_write_record(const arq_mc_cfg_t *c);   // solo el registro (2 copias; escribe la no vigente y la verifica)
uint8_t arq_mc_restore_all();                    // M502 con el perfil activo + M500 (conserva modelo y comprobaciones)
void    arq_mc_test_mode(const bool on);         // el asistente prueba direcciones y homing antes de guardar
void    arq_mc_notice(const char *text);         // aviso a la pantalla (p. ej. movimiento rechazado)
const char *arq_mc_take_notice();                // el aviso pendiente (o nullptr); lo consume

#endif
