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

#include "arquis_mesh_ui.h"

#if ARQ_HAS_MESH

#include "arquis_mesh.h"
#include "arquis_mesh_view.h"
#include "draw_ui.h"

#include <stdio.h>
#include <string.h>

extern lv_group_t *g;

static lv_obj_t  *scr;
static lv_task_t *task;
static char       msg[96], dlg_msg[132];
static uint8_t    msg_kind, sel = 0xFF;
static bool       result_pending;      // hay un resultado (DONE) que mostrar con el texto de 'msg'

enum { DLG_START_OK = 101, DLG_START_NO, DLG_CANCEL_OK, DLG_CANCEL_NO, DLG_LOAD_OK, DLG_LOAD_NO, DLG_INFO, DLG_BACK_OK, DLG_BACK_NO };

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

static void info_dialog(const char *title, const char *text, lv_event_cb_t cb);
static void mesh_cb(lv_obj_t *obj, lv_event_t event);

static void say(uint8_t kind, const char *text) { msg_kind = kind; snprintf(msg, sizeof(msg), "%s", text); }

static void info_dialog(const char *title, const char *text, lv_event_cb_t cb) {
  snprintf(dlg_msg, sizeof(dlg_msg), "%s", text);
  arq_dialog_open(title, dlg_msg, "Aceptar", nullptr, false, cb, DLG_INFO, 0);
}
static void error_dialog(uint8_t err) { info_dialog("No se pudo", arq_mesh_err_text(err), mesh_cb); }

// ---- Modelo de la vista a partir del controlador ----------------------------------------------------------------------------------
static void refresh() {
  arq_mesh_info_t in; arq_mesh_ctl_info(&in);
  arq_mesh_view_model_t m; memset(&m, 0, sizeof(m));
  uint8_t phase = ARQ_MVP_IDLE;
  switch (in.st) {
    case ARQ_MESH_PREP:   phase = ARQ_MVP_PREP; break;
    case ARQ_MESH_MOVING: phase = ARQ_MVP_MOVING; break;
    case ARQ_MESH_ADJUST: phase = ARQ_MVP_ADJUST; break;
    case ARQ_MESH_REVIEW: phase = ARQ_MVP_REVIEW; break;
    case ARQ_MESH_DONE: case ARQ_MESH_ABORTED: case ARQ_MESH_ERROR:
      phase = (in.prep == ARQ_PP_END) ? ARQ_MVP_PREP : ARQ_MVP_RESULT;     // mientras se termina (Z a salvo) se muestra "Terminando"
      if (phase == ARQ_MVP_RESULT && !result_pending) {
        if (in.st == ARQ_MESH_ERROR) { snprintf(msg, sizeof(msg), "%s. Se conserva la malla anterior.", arq_mesh_err_text(in.err)); msg_kind = ARQ_MV_MSG_ERR; }
        else if (in.st == ARQ_MESH_ABORTED) { snprintf(msg, sizeof(msg), "Calibraci\xC3\xB3n cancelada. Se conserva la malla anterior."); msg_kind = ARQ_MV_MSG_WARN; }
        result_pending = true;
      }
      break;
    default: break;
  }
  m.phase = phase;
  m.prep = (in.st == ARQ_MESH_DONE || in.st == ARQ_MESH_ABORTED || in.st == ARQ_MESH_ERROR) ? (uint8_t)4 : in.prep;
  m.cur = in.cur; m.sel = sel; m.percent = in.percent; m.count = in.count; m.step_idx = in.step_idx;
  m.soak_s = in.soak_s; m.rel = in.rel; m.busy = in.busy; m.leveling_on = in.leveling_on; m.mesh_ok = in.mesh_ok;
  m.mask = in.mask; m.bed_cur = in.bed_cur; m.bed_set = in.bed_set; m.z_now = in.z_now; m.fade = in.fade_mm;
  memcpy(m.val, in.val, sizeof(m.val));
  m.can_repeat = in.count > 0;
  if (phase == ARQ_MVP_REVIEW && !in.rel) { snprintf(msg, sizeof(msg), "%s. Repite el punto sospechoso.", arq_mesh_err_text(in.err)); msg_kind = ARQ_MV_MSG_WARN; }
  m.msg = msg; m.msg_kind = msg_kind;
  if (sel < ARQ_MESH_PTS && !(in.mask & (1UL << sel))) sel = 0xFF;           // el punto elegido ya no esta medido
  arq_mesh_view_update(&m);
}

static void task_cb(lv_task_t *) { refresh(); }

// ---- Acciones -----------------------------------------------------------------------------------------------------------------
static void leave() {
  arq_mesh_clear();
  lv_draw_tool();
}

static uint8_t pick_repeat(const arq_mesh_info_t &in) {
  if (sel < ARQ_MESH_PTS && (in.mask & (1UL << sel))) return sel;
  if (in.st == ARQ_MESH_ADJUST) {                         // por defecto: el punto medido justo antes del actual
    const uint8_t sq = arq_mesh_p_to_seq(in.cur);
    if (sq > 0 && sq != 0xFF) { const uint8_t p = arq_mesh_seq_to_p(sq - 1); if (in.mask & (1UL << p)) return p; }
  }
  return 0xFF;
}

static void on_save() {
  arq_mesh_info_t in; arq_mesh_ctl_info(&in);
  if (in.st == ARQ_MESH_REVIEW) {
    uint8_t e = arq_mesh_ctl_commit();
    if (e) { error_dialog(e); return; }
    e = arq_mesh_ctl_save();
    if (e) { snprintf(msg, sizeof(msg), "Malla ACTIVA pero NO guardada: %s. Se perder\xC3\xA1 al apagar.", arq_mesh_err_text(e)); msg_kind = ARQ_MV_MSG_WARN; }
    else { say(ARQ_MV_MSG_OK, "Malla 5x5 aplicada, activa y guardada en la EEPROM."); }
    result_pending = true; sel = 0xFF;
    return;
  }
  const uint8_t e = arq_mesh_ctl_save();
  if (e) error_dialog(e); else info_dialog("Guardado", "La malla se guard\xC3\xB3 en la EEPROM (junto al resto de ajustes).", mesh_cb);
}

static void mesh_cb(lv_obj_t *obj, lv_event_t event) {
  if (event != LV_EVENT_RELEASED) return;
  const int id = obj->mks_obj_id;
  arq_mesh_info_t in; arq_mesh_ctl_info(&in);
  if (id >= DLG_START_OK) {                              // botones de los dialogos
    arq_dialog_close();
    switch (id) {
      case DLG_START_OK: { const uint8_t e = arq_mesh_ctl_start(in.bed_set); if (e) error_dialog(e); } break;
      case DLG_CANCEL_OK: arq_mesh_ctl_cancel(); sel = 0xFF; result_pending = false; break;
      case DLG_LOAD_OK: {
        const uint8_t e = arq_mesh_ctl_load();
        if (e) error_dialog(e); else info_dialog("Cargada", "Malla cargada de la EEPROM y activada.", mesh_cb);
      } break;
      case DLG_BACK_OK: arq_mesh_ctl_cancel(); sel = 0xFF; result_pending = false; leave(); return;
      default: break;
    }
    refresh();
    return;
  }
  if (arq_dialog_is_open()) return;
  switch (id) {
    case ARQ_MV_BACK:
      if (arq_mesh_ctl_session_active()) {
        if (in.prep == ARQ_PP_END) break;               // terminando: espera
        arq_dialog_open("Calibraci\xC3\xB3n en curso", "Si sales ahora se cancela la medici\xC3\xB3n y se conserva la malla anterior.", "Cancelar medici\xC3\xB3n", "Seguir", true, mesh_cb, DLG_BACK_OK, DLG_BACK_NO);
      }
      else { arq_mesh_ctl_ack(); result_pending = false; sel = 0xFF; leave(); return; }
      break;
    case ARQ_MV_TEMP_DEC: arq_mesh_ctl_bed_delta(-1); break;
    case ARQ_MV_TEMP_INC: arq_mesh_ctl_bed_delta(+1); break;
    case ARQ_MV_START: {
      const uint8_t e = arq_mesh_ctl_can_start();
      if (e) { error_dialog(e); break; }
      snprintf(dlg_msg, sizeof(dlg_msg), "La cama se calentar\xC3\xA1 a %d\xC2\xB0" "C, el cabezal har\xC3\xA1 Home y recorrer\xC3\xA1 la cama. Retira objetos y limpia la boquilla.", (int)in.bed_set);
      arq_dialog_open("Iniciar medici\xC3\xB3n", dlg_msg, "Iniciar", "Volver", false, mesh_cb, DLG_START_OK, DLG_START_NO);
    } break;
    case ARQ_MV_CANCEL:
      arq_dialog_open("Cancelar calibraci\xC3\xB3n", "Se descartan las mediciones y se conserva la \xC3\xBAltima malla v\xC3\xA1lida.", "Cancelar medici\xC3\xB3n", "Seguir", true, mesh_cb, DLG_CANCEL_OK, DLG_CANCEL_NO);
      break;
    case ARQ_MV_UP:   arq_mesh_ctl_jog(+1); break;
    case ARQ_MV_DOWN: arq_mesh_ctl_jog(-1); break;
    case ARQ_MV_CONFIRM: { const uint8_t e = arq_mesh_ctl_confirm(); if (e) error_dialog(e); } break;
    case ARQ_MV_REPEAT: {
      const uint8_t p = pick_repeat(in);
      if (p == 0xFF) { info_dialog("Repetir punto", "Toca en el mapa el punto que quieres volver a medir.", mesh_cb); break; }
      const uint8_t e = arq_mesh_ctl_repeat(p);
      if (e) error_dialog(e); else sel = 0xFF;
    } break;
    case ARQ_MV_MAP: {
      const int8_t p = arq_mesh_view_hit();
      if (p >= 0 && (in.st == ARQ_MESH_ADJUST || in.st == ARQ_MESH_REVIEW) && (in.mask & (1UL << p))) sel = (sel == (uint8_t)p) ? 0xFF : (uint8_t)p;
    } break;
    case ARQ_MV_SAVE: on_save(); break;
    case ARQ_MV_LOAD:
      arq_dialog_open("Cargar de la EEPROM", "Se leen TODOS los ajustes guardados (M501): se pierden los cambios no guardados.", "Cargar", "Volver", false, mesh_cb, DLG_LOAD_OK, DLG_LOAD_NO);
      break;
    case ARQ_MV_TOGGLE: { const uint8_t e = arq_mesh_ctl_set_leveling(!in.leveling_on); if (e) error_dialog(e); } break;
    case ARQ_MV_OK: arq_mesh_ctl_ack(); result_pending = false; sel = 0xFF; break;
    default:
      if (id >= ARQ_MV_STEP0 && id < ARQ_MV_STEP0 + ARQ_MESH_STEPS) arq_mesh_ctl_select_step((uint8_t)(id - ARQ_MV_STEP0));
      break;
  }
  refresh();
}

// ---- Pantalla ----------------------------------------------------------------------------------------------------------------------
void arq_mesh_draw() {
  scr = lv_screen_create(MESHLEVELING_UI, "");
  arq_screen_apply(scr);
  arq_mesh_view_build(scr, mesh_cb);
  task = lv_task_create(task_cb, 250, LV_TASK_PRIO_LOWEST, 0);
  refresh();
  add_to_group(scr);
}

void arq_mesh_clear() {
  remove_group();
  arq_dialog_close();
  if (task) lv_task_del(task);
  task = nullptr;
  arq_mesh_view_forget();
  if (scr) lv_obj_del(scr);
  scr = nullptr;
}

void arq_mesh_poll() { arq_mesh_ctl_poll(); }

bool arq_mesh_is_leveling_on() { arq_mesh_info_t i; arq_mesh_ctl_info(&i); return i.leveling_on; }
bool arq_mesh_is_valid()       { arq_mesh_info_t i; arq_mesh_ctl_info(&i); return i.mesh_ok; }

#endif // ARQ_HAS_MESH
