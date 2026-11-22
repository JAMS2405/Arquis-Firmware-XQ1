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
 * ARQUIS UI - pantalla "NIVELACION DE CAMA" (malla manual bilineal 5x5). Pegamento entre la vista (arquis_mesh_view) y el
 * controlador (arquis_mesh). Solo existe con ARQUIS_UI + AUTO_BED_LEVELING_BILINEAR + PROBE_MANUALLY (ARQ_HAS_MESH).
 */

#include "../../../inc/MarlinConfigPre.h"

#if HAS_TFT_LVGL_UI && ENABLED(ARQUIS_UI) && ENABLED(AUTO_BED_LEVELING_BILINEAR) && ENABLED(PROBE_MANUALLY)
  #define ARQ_HAS_MESH 1
#else
  #define ARQ_HAS_MESH 0
#endif

#if ARQ_HAS_MESH
  void arq_mesh_draw();     // crea la pantalla (MESHLEVELING_UI); si hay una sesion en curso, recupera su estado
  void arq_mesh_clear();    // la libera (la sesion, si la hay, sigue en el controlador)
  void arq_mesh_poll();     // desde GUI_RefreshPage: hace avanzar el controlador aunque la pantalla no este visible
  bool arq_mesh_is_leveling_on();    // compensacion activa en Marlin (para Sistema y diagnostico)
  bool arq_mesh_is_valid();          // Marlin tiene una malla 5x5 valida
#endif
