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
 * ARQUIS UI - Dashboard conectado a Marlin. Sustituye a la pantalla principal original (PRINT_READY_UI) cuando
 * ARQUIS_UI esta activo en Configuration.h; draw_ready_print.cpp delega aqui.
 */

void arq_dashboard_draw();      // crea la pantalla (registra PRINT_READY_UI en disp_state / disp_state_stack)
void arq_dashboard_refresh();   // refresco periodico (lo llama GUI_RefreshPage via lv_temp_refr)
void arq_dashboard_clear();     // libera la pantalla
