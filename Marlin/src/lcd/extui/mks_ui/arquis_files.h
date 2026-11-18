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
 * ARQUIS UI - confirmacion de "Imprimir archivo" (sustituye al dialogo MKS de DIALOG_TYPE_PRINT_FILE).
 * Solo cambia como se dibuja: los botones llaman a los MISMOS manejadores originales de draw_dialog.cpp (btn_ok_event_cb /
 * btn_cancel_event_cb), que son los que inician la impresion; aqui no hay logica de impresion.
 */

#include <lvgl.h>

void arq_print_confirm_draw(lv_obj_t *scr, const char *file_name, lv_event_cb_t ok_cb, lv_event_cb_t cancel_cb);
