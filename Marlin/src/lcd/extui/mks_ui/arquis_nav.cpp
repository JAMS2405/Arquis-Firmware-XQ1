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

#include "../../../inc/MarlinConfigPre.h"

#if HAS_TFT_LVGL_UI && ENABLED(ARQUIS_UI)

#include "arquis_nav.h"
#include "arquis_data.h"
#include "draw_ui.h"

extern bool flash_preview_begin, default_preview_flg;

void arq_nav_go(arq_dest_t dest) {
  // SEGURIDAD (defensa en profundidad, independiente de la pantalla que llame):
  //  - lv_draw_print_file() ejecuta card.mount(), que cierra y reinicia la SD: NUNCA durante una impresion.
  //  - Mover, Herramientas y Ajustes (que incluye apagar motores) no se abren con un trabajo activo.
  if (arq_data_job_active()) {
    if (dest == ARQ_GO_PRINT_FILES) dest = ARQ_GO_PRINTING;
    else if (dest == ARQ_GO_MOVE || dest == ARQ_GO_TOOLS || dest == ARQ_GO_SETTINGS) return;
  }
  switch (dest) {
    case ARQ_GO_PRINT_FILES: TERN(MULTI_VOLUME, lv_draw_media_select(), lv_draw_print_file()); break;
    case ARQ_GO_PRINTING:    // igual que draw_return_ui() para PRINTING_UI
      if (gCfgItems.from_flash_pic) flash_preview_begin = true; else default_preview_flg = true;
      lv_draw_printing();
      break;
    case ARQ_GO_MOVE:        lv_draw_move_motor(); break;
    case ARQ_GO_TEMP_HOTEND: uiCfg.curTempType = 0; lv_draw_preHeat(); break;
    case ARQ_GO_TEMP_BED:    uiCfg.curTempType = 1; lv_draw_preHeat(); break;
    case ARQ_GO_FANS:        lv_draw_fan(); break;
    case ARQ_GO_TOOLS:       lv_draw_tool(); break;
    case ARQ_GO_SETTINGS:    lv_draw_set(); break;
  }
}

#endif // HAS_TFT_LVGL_UI && ARQUIS_UI
