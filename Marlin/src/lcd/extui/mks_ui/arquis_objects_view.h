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
 * ARQUIS UI - vista de "Cancelar objeto" (solo LVGL: recibe un modelo, no conoce Marlin ni la SD).
 *
 * El heap de LVGL es de 16 KB: la version original creaba ~70 objetos y lo agotaba. Esta vista usa 6 objetos FIJOS sin importar cuantos
 * objetos tenga el G-code (hasta ARQ_OBJ_MAX = 30, la capacidad de diseno de shared/object_protocol/limits.json):
 *   cabecera (2) | mapa de la cama (1, dibuja marcadores) | lista + paginacion + zoom + estado (1, la dibuja) | boton de accion (1)
 * Mapa y lista comparten UN modelo y UNA seleccion; el modelo NO se copia (la vista guarda el puntero y una firma de lo dibujado).
 *
 * Posiciones: aritmetica entera de arquis_objects.cpp (la misma que bed_map_engine.py de Cura, probada con los vectores compartidos).
 * Orientacion (PROVISIONAL, ORG=FL): X crece a la derecha e Y hacia ARRIBA en pantalla; en la esquina inferior izquierda se rotula
 * "X0 Y0" (el origen del G-code en esta vista). No se invierte ningun eje automaticamente.
 */

#include "arquis_widgets.h"
#include "arquis_objects.h"

#define ARQ_OBJ_MAX   ARQ_OBJ_DESIGN_CAPACITY      // 30
#define ARQ_OBJ_ROWS  5                           // filas visibles de la lista (30 objetos = 6 paginas)

enum { ARQ_OID_BACK = 1, ARQ_OID_MAP, ARQ_OID_GRID, ARQ_OID_ACTION };

// arq_objects_pick() devuelve un objeto (>= 0) o uno de estos valores para los controles de la lista
enum { ARQ_PICK_NONE = -1, ARQ_PICK_PAGE_PREV = -2, ARQ_PICK_PAGE_NEXT = -3, ARQ_PICK_ZOOM1 = -4, ARQ_PICK_ZOOM2 = -5, ARQ_PICK_ZOOM4 = -6 };

enum { ARQ_ACT_NONE = 0, ARQ_ACT_CANCEL, ARQ_ACT_RESTORE, ARQ_ACT_LOCKED };      // sin seleccion / cancelar / restaurar / bloqueado (no validado)
enum { ARQ_SRC_NONE = 0, ARQ_SRC_LEARNED, ARQ_SRC_MANIFEST };                    // de donde salen las posiciones

// Origen de las posiciones y los nombres (lo implementa quien llena el modelo)
typedef struct {
  bool    (*box)(uint8_t i, int32_t *cx, int32_t *cy, int32_t *w, int32_t *d, char *shape);   // centro y tamano en 0,1 mm; false = aun desconocida
  uint8_t (*label)(uint8_t i, char *out, uint8_t n);                                            // "Llavero 3" / "Objeto 3"
} arq_obj_src_t;

typedef struct {
  const arq_obj_src_t *src;
  uint8_t  n;                              // objetos que se muestran (0..ARQ_OBJ_MAX)
  uint8_t  total;                          // objetos que define el G-code (puede ser > n)
  int8_t   active;                         // objeto que se imprime (-1 = ninguno)
  int8_t   sel;                            // objeto seleccionado (-1 = ninguno)
  uint8_t  page;                           // pagina de la lista
  uint8_t  zoom;                           // 1, 2 o 4 (el mapa se centra en el seleccionado)
  uint8_t  action;                         // ARQ_ACT_*
  uint8_t  source;                         // ARQ_SRC_*
  ArqBed   bed;                            // cama (0,1 mm)
  const char *note;                        // texto FIJO (literal) bajo la lista, p. ej. "Cancelacion no validada"; nullptr = ninguno
  uint8_t  state[ARQ_OBJ_MAX];             // ARQ_ST_*
} arq_obj_model_t;

void arq_objects_build(lv_obj_t *scr, lv_event_cb_t cb);            // 6 objetos
void arq_objects_update(const arq_obj_model_t *m);                  // guarda el PUNTERO (sin copia); no redibuja si nada cambio
void arq_objects_forget();                                          // al liberar la pantalla
int8_t arq_objects_pick(lv_obj_t *o, const lv_point_t *p);          // objeto (>= 0), control de la lista (ARQ_PICK_*) o ARQ_PICK_NONE bajo el punto (coord. de pantalla)
uint32_t arq_objects_sign(const arq_obj_model_t *m);                // firma de lo que se dibuja (FNV-1a)
