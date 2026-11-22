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
 * ARQUIS UI - vista de la nivelacion manual 5x5 (solo LVGL, sin Marlin).
 *
 * Una sola pantalla con un mapa de 25 celdas (UN objeto que dibuja todo) y un panel derecho que cambia segun la fase.
 * Las celdas muestran: pendiente (numero de orden), en curso (Z actual), medida (Z absoluta) o, con malla completa/guardada,
 * la DESVIACION relativa al centro en color (azul = mas baja, ambar = mas alta). Nunca se pinta una interpolacion como si fuese medida.
 * Memoria: ~13 objetos como maximo (heap de LVGL de 16 KB).
 */

#include "arquis_widgets.h"
#include "arquis_mesh_logic.h"

enum {
  ARQ_MV_BACK = 1, ARQ_MV_MAP, ARQ_MV_START, ARQ_MV_CANCEL, ARQ_MV_CONFIRM, ARQ_MV_REPEAT, ARQ_MV_SAVE, ARQ_MV_LOAD, ARQ_MV_TOGGLE,
  ARQ_MV_UP, ARQ_MV_DOWN, ARQ_MV_TEMP_DEC, ARQ_MV_TEMP_INC, ARQ_MV_OK,
  ARQ_MV_STEP0                      // ARQ_MV_STEP0 + i = paso i de arq_mesh_step_mm
};

typedef enum { ARQ_MVP_IDLE, ARQ_MVP_PREP, ARQ_MVP_MOVING, ARQ_MVP_ADJUST, ARQ_MVP_REVIEW, ARQ_MVP_RESULT } arq_mv_phase_t;
typedef enum { ARQ_MV_MSG_INFO, ARQ_MV_MSG_OK, ARQ_MV_MSG_WARN, ARQ_MV_MSG_ERR } arq_mv_msg_t;

typedef struct {
  uint8_t  phase;                    // arq_mv_phase_t
  uint8_t  cur;                      // punto en curso (MOVING / ADJUST)
  uint8_t  sel;                      // punto seleccionado para repetir (0xFF = ninguno)
  uint8_t  percent, count;
  uint8_t  step_idx;
  uint8_t  prep;                     // 1 calentando, 2 estabilizando, 3 homing, 4 terminando
  uint8_t  soak_s;
  bool     rel;                      // val[] son desviaciones relativas al centro
  bool     busy, leveling_on, mesh_ok, can_repeat;
  uint8_t  msg_kind;                 // arq_mv_msg_t
  uint32_t mask;                     // puntos con valor
  int16_t  bed_cur, bed_set;
  float    z_now;
  float    fade;
  float    val[ARQ_MESH_PTS];
  const char *msg;                   // linea de resultado/error (cadena del llamador; vive mientras dure la pantalla)
} arq_mesh_view_model_t;

void arq_mesh_view_build(lv_obj_t *scr, lv_event_cb_t cb);
void arq_mesh_view_update(const arq_mesh_view_model_t *m);     // reconstruye el panel solo si cambia la fase; si no, actualiza textos
int8_t arq_mesh_view_hit();                                    // celda (indice p) bajo el ultimo toque del mapa, -1 si ninguna
void arq_mesh_view_forget();
