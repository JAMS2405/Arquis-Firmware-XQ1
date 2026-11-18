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

#include "arquis_files.h"
#include "arquis_files_view.h"
#include "draw_ui.h"

extern lv_group_t *g;

static lv_event_cb_t ok_handler, cancel_handler;

static void confirm_cb(lv_obj_t *obj, lv_event_t event) {
  if (event != LV_EVENT_RELEASED) return;
  if (obj->mks_obj_id == ARQ_PC_OK) { if (ok_handler) ok_handler(obj, event); }
  else if (cancel_handler) cancel_handler(obj, event);
}

void arq_print_confirm_draw(lv_obj_t *scr, const char *file_name, lv_event_cb_t ok_cb, lv_event_cb_t cancel_cb) {
  ok_handler = ok_cb; cancel_handler = cancel_cb;
  arq_screen_apply(scr);
  arq_confirm_build(scr, confirm_cb, file_name);
  #if HAS_ROTARY_ENCODER
    if (gCfgItems.encoder_enable) arq_group_add_clickables(g, scr);
  #endif
}

#endif // HAS_TFT_LVGL_UI && ARQUIS_UI
