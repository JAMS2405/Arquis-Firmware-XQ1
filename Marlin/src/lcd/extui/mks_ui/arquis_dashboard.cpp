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

#include "arquis_dashboard.h"
#include "arquis_dashboard_view.h"
#include "arquis_nav.h"
#include "arquis_widgets.h"
#include "arquis_machine.h"   // ARQUIS: modelo de maquina elegido (medidas y nombre)
#include "draw_ui.h"

extern lv_group_t *g;
static lv_obj_t *scr;

// Misma convencion que el resto de pantallas MKS: se actua al soltar el dedo.
static void event_handler(lv_obj_t *obj, lv_event_t event) {
  if (event != LV_EVENT_RELEASED) return;
  const uint16_t id = obj->mks_obj_id;
  const bool job = arq_data_job_active();            // contextual: se lee de Marlin en el momento del toque
  // Con trabajo activo estos destinos estan bloqueados (aunque la tarjeta se haya dibujado antes de que empezara la impresion)
  if (job && (id == ARQ_ID_MOVE || id == ARQ_ID_TOOLS || id == ARQ_ID_SETTINGS)) return;
  arq_dashboard_clear();
  switch (id) {
    case ARQ_ID_PRINT:    arq_nav_go(job ? ARQ_GO_PRINTING : ARQ_GO_PRINT_FILES); break;
    case ARQ_ID_MOVE:     arq_nav_go(ARQ_GO_MOVE); break;
    case ARQ_ID_TEMP:
    case ARQ_ID_HOT:      arq_nav_go(ARQ_GO_TEMP_HOTEND); break;
    case ARQ_ID_BED:      arq_nav_go(ARQ_GO_TEMP_BED); break;
    case ARQ_ID_FANS:     arq_nav_go(ARQ_GO_FANS); break;
    case ARQ_ID_TOOLS:    arq_nav_go(ARQ_GO_TOOLS); break;
    case ARQ_ID_SETTINGS: arq_nav_go(ARQ_GO_SETTINGS); break;
  }
}

void arq_dashboard_draw() {
  disp_state_stack._disp_index = 0;
  ZERO(disp_state_stack._disp_state);
  scr = lv_screen_create(PRINT_READY_UI, "");
  arq_screen_apply(scr);
  arq_snapshot_t s;
  arq_data_read(&s);
  arq_dash_build(scr, event_handler, arq_mc_name(), &s);
  arq_dash_update(&s);
  #if HAS_ROTARY_ENCODER
    if (gCfgItems.encoder_enable) arq_group_add_clickables(g, scr);   // la perilla navega por las tarjetas (como lv_big_button_create)
  #endif
}

void arq_dashboard_refresh() {
  if (!scr) return;
  arq_snapshot_t s;
  arq_data_read(&s);
  arq_dash_update(&s);
}

void arq_dashboard_clear() {
  #if HAS_ROTARY_ENCODER
    if (gCfgItems.encoder_enable) lv_group_remove_all_objs(g);
  #endif
  arq_dialog_close();                                // un dialogo en la capa superior sobreviviria al cambio de pantalla
  arq_dash_forget();
  if (scr) { lv_obj_del(scr); scr = nullptr; }
}

#endif // HAS_TFT_LVGL_UI && ARQUIS_UI
