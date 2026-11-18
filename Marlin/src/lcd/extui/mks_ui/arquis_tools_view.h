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
 * ARQUIS UI - vistas de Etapa 5 (solo LVGL, sin Marlin): Velocidad, Nivelar cama y Sistema/diagnostico.
 * (Herramientas, Home y Ajustes usan la rejilla de accesos arq_opts_* de arquis_ctrl_view.h; Babystep es arq_move_build(..., true).)
 */

#include "arquis_widgets.h"

// ---- Velocidad (movimiento / extrusion, en %) --------------------------------------------------------------------------------------
enum { ARQ_SID_BACK = 1, ARQ_SID_FEED, ARQ_SID_FLOW, ARQ_SID_S1, ARQ_SID_S5, ARQ_SID_S10, ARQ_SID_DEC, ARQ_SID_INC };
typedef struct { int16_t feed, flow; uint8_t sel; uint8_t step; } arq_speed_model_t;   // sel: 0 = movimiento, 1 = extrusion; step: 1, 5 o 10
void arq_speed_build(lv_obj_t *scr, lv_event_cb_t cb);
void arq_speed_update(const arq_speed_model_t *m);
void arq_speed_forget();

// ---- Nivelar cama (5 puntos sobre un esquema de la cama) ----------------------------------------------------------------------------
enum { ARQ_LID_BACK = 1, ARQ_LID_P1, ARQ_LID_P2, ARQ_LID_P3, ARQ_LID_P4, ARQ_LID_P5 };
typedef struct { int16_t bed_x, bed_y; int16_t x[5], y[5]; } arq_level_model_t;      // mm
void arq_level_build(lv_obj_t *scr, lv_event_cb_t cb, const arq_level_model_t *m);
void arq_level_forget();

// ---- Sistema y diagnostico: dos listas con titulo (hasta 8 filas cada una) ----------------------------------------------------------
enum { ARQ_IID_BACK = 1 };
typedef struct { const char *title; arq_list_row_t rows[8]; uint8_t n; } arq_info_list_t;
void arq_info_build(lv_obj_t *scr, lv_event_cb_t cb, const char *header_title, arq_info_list_t *left, arq_info_list_t *right);   // las listas las conserva el llamador
void arq_info_refresh();                                                                                                      // tras cambiar los valores de las listas
void arq_info_forget();
