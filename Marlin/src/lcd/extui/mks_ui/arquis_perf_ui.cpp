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

#include "arquis_perf_ui.h"

#if ARQ_HAS_PERF

#include "arquis_perf_view.h"
#include "draw_ui.h"

#include "../../../inc/MarlinConfig.h"

#include <stdio.h>
#include <string.h>

extern lv_group_t *g;

static lv_obj_t  *scr;
static lv_task_t *task;
static char       dlg_msg[96];

enum { DLG_BACK_OK = 101, DLG_BACK_NO, DLG_INFO };

static void add_to_group(lv_obj_t *s) {
  #if HAS_ROTARY_ENCODER
    if (gCfgItems.encoder_enable) arq_group_add_clickables(g, s);
  #endif
}
static void remove_group() {
  #if HAS_ROTARY_ENCODER
    if (gCfgItems.encoder_enable) lv_group_remove_all_objs(g);
  #endif
}

static void perf_cb(lv_obj_t *obj, lv_event_t event);

static void info_dialog(const char *title, uint8_t err) {
  snprintf(dlg_msg, sizeof(dlg_msg), "%s", arq_pf_err_text(err));
  arq_dialog_open(title, dlg_msg, "Aceptar", nullptr, false, perf_cb, DLG_INFO, 0);
}

static void refresh() {
  arq_perf_info_t in; arq_perf_ctl_info(&in);
  arq_perf_view_model_t m; memset(&m, 0, sizeof(m));
  memcpy(m.has, in.has, sizeof m.has); memcpy(m.val, in.val, sizeof m.val); memcpy(m.ref, in.ref, sizeof m.ref);
  m.shaping_min = in.shaping_min; m.sel = in.sel; m.why = in.why; m.coarse = in.coarse; m.dirty = in.dirty;
  arq_perf_view_update(&m);
}

static void task_cb(lv_task_t *) { refresh(); }

static void leave() { arq_perf_clear(); lv_draw_set(); }

static void perf_cb(lv_obj_t *obj, lv_event_t event) {
  if (event != LV_EVENT_RELEASED) return;
  const int id = obj->mks_obj_id;
  if (id >= DLG_BACK_OK) {
    arq_dialog_close();
    if (id == DLG_BACK_OK) { leave(); return; }
    refresh();
    return;
  }
  if (arq_dialog_is_open()) return;
  if (id >= ARQ_PFV_PICK0) { arq_perf_ctl_select((uint8_t)(id - ARQ_PFV_PICK0)); refresh(); return; }
  switch (id) {
    case ARQ_PFV_BACK: {
      arq_perf_info_t in; arq_perf_ctl_info(&in);
      if (in.dirty) arq_dialog_open("Cambios sin guardar", "Los valores nuevos siguen aplicados hasta que apagues la impresora. Pulsa Guardar para conservarlos.",
                                    "Salir", "Volver", false, perf_cb, DLG_BACK_OK, DLG_BACK_NO);
      else { leave(); return; }
    } break;
    case ARQ_PFV_MINUS: { const uint8_t e = arq_perf_ctl_delta(-1); if (e && e != ARQ_PF_E_BUSY && e != ARQ_PF_E_PRINTING) info_dialog("No se pudo cambiar", e); } break;
    case ARQ_PFV_PLUS:  { const uint8_t e = arq_perf_ctl_delta(+1); if (e && e != ARQ_PF_E_BUSY && e != ARQ_PF_E_PRINTING) info_dialog("No se pudo cambiar", e); } break;
    case ARQ_PFV_STEP:  arq_perf_ctl_toggle_step(); break;
    case ARQ_PFV_SAVE:  { const uint8_t e = arq_perf_ctl_save(); if (e) info_dialog(e == ARQ_PF_E_SAVE ? "Aplicado, pero no guardado" : "No se pudo guardar", e); } break;
    case ARQ_PFV_RESTORE: { const uint8_t e = arq_perf_ctl_restore(); if (e) info_dialog("No se pudo restaurar", e); } break;
    default: break;
  }
  refresh();
}

void arq_perf_draw() {
  scr = lv_screen_create(PERF_UI, "");
  arq_screen_apply(scr);
  arq_perf_view_build(scr, perf_cb);
  task = lv_task_create(task_cb, 250, LV_TASK_PRIO_LOWEST, 0);
  refresh();
  add_to_group(scr);
}

void arq_perf_clear() {
  remove_group();
  arq_dialog_close();
  if (task) lv_task_del(task);
  task = nullptr;
  arq_perf_view_forget();
  if (scr) lv_obj_del(scr);
  scr = nullptr;
}

#endif // ARQ_HAS_PERF
