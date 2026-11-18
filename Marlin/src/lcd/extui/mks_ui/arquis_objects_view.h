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
 * ARQUIS UI - vista de "Cancelar objeto" (solo LVGL: recibe un modelo, no conoce Marlin).
 *
 * La version anterior creaba ~70 objetos LVGL (16 fichas + 16 rectangulos, cada uno con su etiqueta) y el heap de LVGL es de 16 KB:
 * se agotaba y la placa se reiniciaba. Esta version usa 6 objetos FIJOS sin importar cuantos objetos tenga el G-code:
 *   cabecera (2) | mapa de la cama (1, dibuja los rectangulos) | fichas + estado (1, dibuja las fichas) | boton de accion (1)
 * Mapa y fichas son un solo objeto cada uno: lo que se toca se calcula con la posicion del toque (arq_objects_pick).
 */

#include "arquis_widgets.h"

#define ARQ_OBJ_MAX 16

enum { ARQ_OID_BACK = 1, ARQ_OID_MAP, ARQ_OID_GRID, ARQ_OID_ACTION };

typedef struct {
  uint8_t  n;                              // objetos que se muestran (0..ARQ_OBJ_MAX)
  uint8_t  total;                          // objetos que define el G-code (puede ser > n)
  int8_t   active;                         // objeto que se esta imprimiendo (-1 = ninguno)
  int8_t   sel;                            // objeto seleccionado (-1 = ninguno)
  uint32_t canceled;                       // bit i = objeto i cancelado
  uint32_t has_box;                        // bit i = se conoce la posicion del objeto i
  int16_t  x0[ARQ_OBJ_MAX], y0[ARQ_OBJ_MAX], x1[ARQ_OBJ_MAX], y1[ARQ_OBJ_MAX];   // mm sobre la cama
  int16_t  bed_x, bed_y;                   // tamano de la cama (mm)
} arq_obj_model_t;

void arq_objects_build(lv_obj_t *scr, lv_event_cb_t cb);            // 6 objetos
void arq_objects_update(const arq_obj_model_t *m);                  // no redibuja si nada cambio
void arq_objects_forget();                                          // al liberar la pantalla
int8_t arq_objects_pick(lv_obj_t *o, const lv_point_t *p);          // objeto bajo el punto (coord. de pantalla) en el mapa o las fichas; -1 = ninguno
