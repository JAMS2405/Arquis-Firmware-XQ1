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
 * ARQUIS UI - supervision de la impresion (union con Marlin y con la maquina de estados original de MKS).
 *
 * No hay logica de impresion propia: pausar, reanudar y cancelar ejecutan EXACTAMENTE las mismas llamadas que la pantalla original
 * (draw_printing.cpp / draw_dialog.cpp). printer_state_polling() sigue completando las pausas y reanudaciones como siempre.
 * Esta pantalla solo muestra datos de Marlin y navega.
 */

void arq_print_draw();      // sustituye a lv_draw_printing()
void arq_print_clear();     // sustituye a lv_clear_printing()
void arq_print_refresh();   // refresco (1 vez por segundo desde GUI_RefreshPage)
void arq_print_poll();      // fin de impresion: se llama en cada pasada de GUI_RefreshPage, desde cualquier pantalla
