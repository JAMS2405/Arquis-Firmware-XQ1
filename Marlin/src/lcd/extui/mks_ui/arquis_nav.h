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
 * ARQUIS UI - navegacion. NO es un segundo sistema de navegacion: cada destino llama a la misma funcion lv_draw_xxx()
 * que usa la interfaz original. Cada lv_draw_xxx() registra su pantalla en disp_state / disp_state_stack con
 * lv_screen_create(), y el boton "Atras" original usa draw_return_ui() como siempre.
 * El llamador debe haber liberado antes su propia pantalla (lv_clear_xxx).
 */

typedef enum {
  ARQ_GO_PRINT_FILES,   // explorador de archivos G-code (o seleccion de medio)
  ARQ_GO_PRINTING,      // supervision de la impresion en curso
  ARQ_GO_MOVE,          // movimiento de ejes
  ARQ_GO_TEMP_HOTEND,   // temperaturas (hotend)
  ARQ_GO_TEMP_BED,      // temperaturas (cama)
  ARQ_GO_FANS,          // ventiladores
  ARQ_GO_TOOLS,         // menu de herramientas (precalentar, extruir, mover, home, nivelacion, filamento)
  ARQ_GO_SETTINGS       // configuracion
} arq_dest_t;

void arq_nav_go(arq_dest_t dest);
