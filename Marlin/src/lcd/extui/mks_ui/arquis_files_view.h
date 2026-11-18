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
 * ARQUIS UI - explorador de archivos (solo LVGL: recibe un modelo, no conoce Marlin ni la SD).
 *
 * 5 objetos fijos (~2 KB): cabecera con Atras (2), lista de 6 filas (1, dibuja las filas y calcula la fila tocada) y subir/bajar pagina (2).
 * Distribucion (480x320): cabecera 44 | lista x 10..410 | botones de pagina x 418..470.
 */

#include "arquis_widgets.h"

#define ARQ_FILE_ROWS 6

enum { ARQ_FLID_BACK = 1, ARQ_FLID_LIST, ARQ_FLID_UP, ARQ_FLID_DOWN };

typedef struct {
  const char *name[ARQ_FILE_ROWS];   // nombres a mostrar (el llamador los conserva)
  uint8_t     n;                     // filas con contenido
  uint8_t     dir_mask;              // bit i = la fila i es una carpeta
  bool        can_up, can_down;      // hay pagina anterior / siguiente
  bool        no_card;               // sin tarjeta SD
  const char *title;                 // titulo de la cabecera (el llamador lo conserva)
} arq_files_model_t;

void   arq_files_build(lv_obj_t *scr, lv_event_cb_t cb);
void   arq_files_update(const arq_files_model_t *m);
void   arq_files_forget();
int8_t arq_files_pick(lv_obj_t *o, const lv_point_t *p);      // fila bajo el punto (coord. de pantalla); -1 = ninguna

// Confirmacion de "Imprimir archivo": cabecera con Atras/Cancelar, nombre del archivo y botones [Cancelar] [Imprimir].
// Si el archivo tiene miniatura, la zona ARQ_CONFIRM_THUMB_* (200 x 200) queda LIBRE de objetos LVGL: la miniatura se pinta directamente
// en la pantalla (arquis_files.cpp), igual que hace la interfaz original. Sin miniatura se muestra un recuadro "Sin miniatura".
#define ARQ_CONFIRM_THUMB_X 10
#define ARQ_CONFIRM_THUMB_Y 56
#define ARQ_CONFIRM_THUMB_S 200
enum { ARQ_PC_CANCEL = 11, ARQ_PC_OK };
void arq_confirm_build(lv_obj_t *scr, lv_event_cb_t cb, const char *file_name, bool has_thumb);
