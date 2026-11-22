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
 * ARQUIS UI - lectura de datos reales de Marlin (solo lectura, sin efectos sobre el firmware).
 * La vista del Dashboard solo recibe este "snapshot"; no toca Marlin directamente.
 */

#include "arquis_logic.h"

typedef enum { ARQ_FAN_LAYER, ARQ_FAN_HEATSINK, ARQ_FAN_AUX } arq_fan_kind_t;

typedef struct { bool present; int16_t cur, tgt, tol; } arq_heater_t;
typedef struct { uint8_t kind, index, pct; } arq_fan_t;

typedef struct {
  arq_heater_t hot, bed;
  uint8_t      fan_n;
  arq_fan_t    fan[3];
  arq_pstate_t state;
  bool         job;          // hay un trabajo de impresion (imprimiendo o en pausa)
  bool         sd_job;       // el trabajo es un archivo de la SD: nombre y porcentaje son reales
  uint8_t      pct;          // 0..100, valido solo si sd_job
  uint32_t     elapsed_s;    // tiempo de impresion de Marlin (print_job_timer)
  uint32_t     remaining_s;  // 0 = desconocido (no se inventa)
  int32_t      z_cmm;        // altura Z actual en centesimas de mm (current_position.z, el mismo valor que muestra la interfaz original)
  uint16_t     speed_mms;    // velocidad real del cabezal en XY (mm/s), solo durante una impresion; 0 si esta quieto
  uint16_t     peak_mms;     // velocidad maxima alcanzada en esta impresion (mm/s)
  char         file[44];
} arq_snapshot_t;

#include <string.h>
static inline bool arq_snapshot_equal(const arq_snapshot_t *a, const arq_snapshot_t *b) { return memcmp(a, b, sizeof(arq_snapshot_t)) == 0; }

// Lectura real (arquis_data.cpp): solo se enlaza dentro de Marlin; el simulador de PC no la usa.
void arq_data_read(arq_snapshot_t *s);   // rellena el snapshot con los valores actuales de Marlin
bool arq_data_job_active();              // imprimiendo o en pausa
void arq_data_speed_poll();              // medir la velocidad real del cabezal; llamar en cada pasada del bucle de la pantalla (arq_print_poll)
