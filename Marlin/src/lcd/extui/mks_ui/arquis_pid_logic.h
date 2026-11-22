/**
 * Marlin 3D Printer Firmware
 * Copyright (c) 2020 MarlinFirmware [https://github.com/MarlinFirmware/Marlin]
 *
 * Based on Sprinter and grbl.
 * Copyright (c) 2011 Camiel Gubbels / Erik van der Zalm
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#pragma once

/**
 * ARQUIS UI - autoajuste PID (hotend y cama): LOGICA PURA (sin LVGL ni Marlin; se prueba en el PC).
 * Marlin hace el ajuste (M303); aqui solo: limites, sesion, validacion del resultado y formatos de las ordenes M303/M301/M304.
 * El resultado NO se aplica solo: el usuario decide (Aplicar y guardar / Descartar).
 */

#include <stdint.h>
#include <stddef.h>
#include "arquis_pid_status.h"

#define ARQ_PID_HOTEND   0
#define ARQ_PID_BED      1
#define ARQ_PID_HEATERS  2

#define ARQ_PID_STEP     5            // °C por pulsacion
#define ARQ_PID_CYC_MIN  3            // M303 exige >= 3
#define ARQ_PID_CYC_MAX  20

typedef struct { int16_t min, max, def; uint8_t cyc_def; } arq_pid_limits_t;
// max_target = mayor objetivo que Marlin admite para ese calentador (hotend: MAXTEMP - HOTEND_OVERSHOOT; cama: BED_MAX_TARGET).
arq_pid_limits_t arq_pid_limits(uint8_t heater, int16_t max_target);
int16_t arq_pid_clamp_target(uint8_t heater, int16_t t, int16_t max_target);
int8_t  arq_pid_clamp_cycles(int c);

typedef enum {
  ARQ_PID_OK = 0,
  ARQ_PID_E_STATE,       // operacion no valida ahora
  ARQ_PID_E_PARAM,       // calentador/temperatura/ciclos fuera de limites
  ARQ_PID_E_PRINTING,    // impresion en curso
  ARQ_PID_E_BUSY,        // hay otra operacion (nivelacion, movimiento pendiente...)
  ARQ_PID_E_TOO_HIGH,    // Marlin: temperatura demasiado alta (objetivo fuera de limites o sobreimpulso)
  ARQ_PID_E_TIMEOUT,     // Marlin: el ajuste no avanza (tiempo agotado)
  ARQ_PID_E_CANCEL,      // cancelado por el usuario
  ARQ_PID_E_BAD_RESULT,  // los valores calculados no son plausibles: no se ofrecen
  ARQ_PID_E_NOT_RUN,     // la orden termino sin que Marlin iniciara el ajuste
  ARQ_PID_E_STOPPED,     // Marlin detenido
  ARQ_PID_E_SAVE,        // fallo al guardar en la EEPROM
  ARQ_PID_E_COUNT
} arq_pid_err_t;
const char * arq_pid_err_text(uint8_t err);

typedef enum { ARQ_PID_IDLE, ARQ_PID_RUNNING, ARQ_PID_RESULT, ARQ_PID_FAILED } arq_pid_state_t;

typedef struct {
  uint8_t  st, heater, err, cycles, cycle;
  int16_t  target;
  float    old_pid[3], new_pid[3];       // Kp, Ki, Kd (Ki y Kd sin escalar, como M301/M304)
} arq_pid_sess_t;

void    arq_pid_sess_reset(arq_pid_sess_t *s);
// IDLE/RESULT/FAILED -> RUNNING. Valida los parametros. old = PID actual de Marlin.
uint8_t arq_pid_begin(arq_pid_sess_t *s, uint8_t heater, int16_t target, int cycles, int16_t max_target, const float old[3]);
void    arq_pid_progress(arq_pid_sess_t *s, uint8_t cycle);
// Resultado de Marlin (arq_pid_status): DONE -> RESULT (si los valores son plausibles) / otros -> FAILED con el motivo.
uint8_t arq_pid_finish_result(arq_pid_sess_t *s, uint8_t marlin_result, float kp, float ki, float kd);
void    arq_pid_fail(arq_pid_sess_t *s, uint8_t err);
void    arq_pid_discard(arq_pid_sess_t *s);       // RESULT/FAILED -> IDLE (no se aplica nada)
bool    arq_pid_is_running(const arq_pid_sess_t *s);
bool    arq_pid_result_ok(float kp, float ki, float kd);   // finitos, Kp>0, Ki>=0,0001, Kd>=0 y dentro de rangos plausibles

// ---- Ordenes G-code (devuelven la longitud; 0 = no cabe) ------------------------------------------------------------------------
size_t arq_pid_fmt_start(char *buf, size_t n, uint8_t heater, int16_t target, int cycles);     // "M303 E0 S210 C8" / "M303 E-1 S60 C6" (sin U: no se aplica)
size_t arq_pid_fmt_apply(char *buf, size_t n, uint8_t heater, float kp, float ki, float kd);   // "M301 P30.2400 I2.8000 D81.6600" / "M304 ..."
// ---- Textos ---------------------------------------------------------------------------------------------------------------------
size_t arq_pid_fmt_num(char *buf, size_t n, float v, uint8_t decimals);   // "30.24"; no finito: "--"
size_t arq_pid_fmt_time(char *buf, size_t n, uint32_t seconds);           // "2:05"
