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
 * ARQUIS UI - vista de la pantalla "Rendimiento" (solo LVGL, sin Marlin): rejilla de 8 valores (2 columnas), una fila de ajuste
 * (- valor +; tocar el valor cambia entre paso fino y grueso), una linea de estado y los botones Restaurar / Guardar.
 * 16 objetos como maximo (cabecera 2 + 8 valores + 3 ajuste + estado 1 + 2 botones).
 */

#include "arquis_widgets.h"
#include "arquis_perf_logic.h"

enum { ARQ_PFV_BACK = 1, ARQ_PFV_MINUS, ARQ_PFV_PLUS, ARQ_PFV_STEP, ARQ_PFV_SAVE, ARQ_PFV_RESTORE,
       ARQ_PFV_PICK0 };                 // ARQ_PFV_PICK0 + id = valor id de arq_pf_id_t

typedef struct {
  bool    has[ARQ_PF_COUNT];
  float   val[ARQ_PF_COUNT], ref[ARQ_PF_COUNT];
  float   shaping_min;
  uint8_t sel, why;                     // why: 0 = editable; si no, arq_pf_err_t
  bool    coarse, dirty;
} arq_perf_view_model_t;

void arq_perf_view_build(lv_obj_t *scr, lv_event_cb_t cb);
void arq_perf_view_update(const arq_perf_view_model_t *m);     // solo redibuja lo que cambio
void arq_perf_view_forget();
