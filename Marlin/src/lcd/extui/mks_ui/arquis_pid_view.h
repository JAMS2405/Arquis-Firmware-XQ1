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
 * ARQUIS UI - vista del autoajuste PID (solo LVGL, sin Marlin). Cuatro fases: elegir (hotend/cama, temperatura, ciclos),
 * en curso (temperatura, ciclo, tiempo, Cancelar), resultado (valores nuevos y anteriores; Aplicar y guardar / Descartar) y aviso.
 */

#include "arquis_widgets.h"
#include "arquis_pid_logic.h"

enum { ARQ_PV_BACK = 1, ARQ_PV_HOT, ARQ_PV_BED, ARQ_PV_TDEC, ARQ_PV_TINC, ARQ_PV_CDEC, ARQ_PV_CINC, ARQ_PV_START, ARQ_PV_CANCEL, ARQ_PV_APPLY, ARQ_PV_DISCARD, ARQ_PV_OK };
typedef enum { ARQ_PVP_SETUP, ARQ_PVP_RUNNING, ARQ_PVP_RESULT, ARQ_PVP_MSG } arq_pv_phase_t;
typedef enum { ARQ_PV_MSG_INFO, ARQ_PV_MSG_OK, ARQ_PV_MSG_WARN, ARQ_PV_MSG_ERR } arq_pv_msg_t;

typedef struct {
  uint8_t  phase, heater, cycle, cycles, msg_kind;
  bool     has[ARQ_PID_HEATERS];
  int16_t  temp_hot, temp_bed;            // temperaturas actuales
  int16_t  target, temp_cur;              // objetivo y temperatura del calentador en uso
  uint32_t elapsed_s;
  float    cur_pid[ARQ_PID_HEATERS][3];   // PID actual (reposo)
  float    old_pid[3], new_pid[3];        // resultado
  const char *msg;                        // cadena del llamador
} arq_pid_view_model_t;

void arq_pid_view_build(lv_obj_t *scr, lv_event_cb_t cb);
void arq_pid_view_update(const arq_pid_view_model_t *m);
void arq_pid_view_forget();
