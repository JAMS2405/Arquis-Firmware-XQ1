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
 * ARQUIS UI - autoajuste PID: CONTROLADOR (pegamento con Marlin; sin LVGL).
 * Lanza M303 por la cola normal SIN el parametro U (no cambia nada por si solo), muestra el progreso y el resultado y, si el
 * usuario acepta, aplica con M301 (hotend) / M304 (cama) y guarda con M500. Cancelar usa el mecanismo de Marlin (wait_for_heatup = false).
 */

#include "arquis_pid_logic.h"

#include "../../../inc/MarlinConfigPre.h"

#if HAS_TFT_LVGL_UI && ENABLED(ARQUIS_UI) && (ENABLED(PIDTEMP) || ENABLED(PIDTEMPBED))
  #define ARQ_HAS_PID 1
#else
  #define ARQ_HAS_PID 0
#endif

#if ARQ_HAS_PID

typedef struct {
  uint8_t  st, heater, err, cycle, cycles;
  int16_t  target;                       // objetivo de la sesion (en curso) o el elegido (en reposo)
  int16_t  temp_cur;                     // temperatura actual del calentador elegido
  int16_t  max_target, min_target;
  bool     has[ARQ_PID_HEATERS];         // calentador con PID disponible
  float    cur_pid[ARQ_PID_HEATERS][3];  // PID actual de Marlin
  float    old_pid[3], new_pid[3];
  uint32_t elapsed_s;
} arq_pid_info_t;

void    arq_pid_ctl_info(arq_pid_info_t *out);
void    arq_pid_ctl_select(uint8_t heater);
void    arq_pid_ctl_target_delta(int8_t dir);
void    arq_pid_ctl_cycles_delta(int8_t dir);
uint8_t arq_pid_ctl_can_start();
uint8_t arq_pid_ctl_start();                  // con el calentador/objetivo/ciclos elegidos
void    arq_pid_ctl_cancel();
void    arq_pid_ctl_poll();                   // desde GUI_RefreshPage (tambien durante M303)
uint8_t arq_pid_ctl_apply();                  // aplica (M301/M304) y guarda (M500); devuelve ARQ_PID_OK o ARQ_PID_E_SAVE (aplicado pero NO guardado)
void    arq_pid_ctl_discard();                // descarta el resultado / acepta el aviso: vuelve a IDLE
bool    arq_pid_ctl_busy();                   // ajuste en marcha

#endif
