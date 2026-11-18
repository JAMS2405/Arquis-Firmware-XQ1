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
 * ARQUIS UI - vista del Dashboard (solo LVGL: recibe un snapshot, no conoce Marlin).
 * Maximo 10 objetos LVGL (~3,8 KB de heap). Los valores se actualizan solo si cambian.
 */

#include "arquis_widgets.h"
#include "arquis_data.h"

enum { ARQ_ID_PRINT = 1, ARQ_ID_MOVE, ARQ_ID_TEMP, ARQ_ID_TOOLS, ARQ_ID_SETTINGS, ARQ_ID_HOT, ARQ_ID_BED, ARQ_ID_FANS };

void arq_dash_build(lv_obj_t *scr, lv_event_cb_t cb, const char *static_title, const arq_snapshot_t *first);
void arq_dash_update(const arq_snapshot_t *s);     // refresco: no hace nada si el snapshot no cambio
void arq_dash_forget();                            // llamar al liberar la pantalla (los objetos los borra LVGL con ella)
