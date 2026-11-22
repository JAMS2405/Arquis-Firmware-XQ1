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

#include "arquis_pid_ui.h"

#if ARQ_HAS_PID

#include "arquis_pid_view.h"
#include "draw_ui.h"

#include "../../../inc/MarlinConfig.h"
#include "../../../module/temperature.h"

#include <stdio.h>
#include <string.h>

extern lv_group_t *g;

static lv_obj_t  *scr;
static lv_task_t *task;
static char       msg[100], dlg_msg[140];
static uint8_t    msg_kind;
static bool       msg_pending;          // hay un aviso (fallo / aplicado) que el usuario debe aceptar

enum { DLG_START_OK = 101, DLG_START_NO, DLG_CANCEL_OK, DLG_CANCEL_NO, DLG_INFO, DLG_BACK_OK, DLG_BACK_NO };

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

static void pid_cb(lv_obj_t *obj, lv_event_t event);

static void info_dialog(const char *title, const char *text) {
  snprintf(dlg_msg, sizeof(dlg_msg), "%s", text);
  arq_dialog_open(title, dlg_msg, "Aceptar", nullptr, false, pid_cb, DLG_INFO, 0);
}

static void refresh() {
  arq_pid_info_t in; arq_pid_ctl_info(&in);
  arq_pid_view_model_t m; memset(&m, 0, sizeof(m));
  uint8_t phase = ARQ_PVP_SETUP;
  switch (in.st) {
    case ARQ_PID_RUNNING: phase = ARQ_PVP_RUNNING; break;
    case ARQ_PID_RESULT:  phase = ARQ_PVP_RESULT; break;
    case ARQ_PID_FAILED:
      if (!msg_pending) { snprintf(msg, sizeof(msg), "%s. No se ha cambiado ning\xC3\xBAn valor.", arq_pid_err_text(in.err)); msg_kind = in.err == ARQ_PID_E_CANCEL ? ARQ_PV_MSG_WARN : ARQ_PV_MSG_ERR; msg_pending = true; }
      phase = ARQ_PVP_MSG;
      break;
    default: if (msg_pending) phase = ARQ_PVP_MSG; break;      // aviso de "aplicado" tras volver a IDLE
  }
  m.phase = phase; m.heater = in.heater; m.cycle = in.cycle; m.cycles = in.cycles; m.target = in.target; m.temp_cur = in.temp_cur; m.elapsed_s = in.elapsed_s;
  for (uint8_t h = 0; h < ARQ_PID_HEATERS; ++h) { m.has[h] = in.has[h]; memcpy(m.cur_pid[h], in.cur_pid[h], sizeof m.cur_pid[h]); }
  memcpy(m.old_pid, in.old_pid, sizeof m.old_pid); memcpy(m.new_pid, in.new_pid, sizeof m.new_pid);
  #if HAS_HOTEND
    m.temp_hot = thermalManager.wholeDegHotend(0);
  #endif
  #if HAS_HEATED_BED
    m.temp_bed = thermalManager.wholeDegBed();
  #endif
  m.msg = msg; m.msg_kind = msg_kind;
  arq_pid_view_update(&m);
}

static void task_cb(lv_task_t *) { refresh(); }

static void leave() { arq_pid_clear(); lv_draw_set(); }

static void error_dialog(uint8_t e) { info_dialog("No se pudo", arq_pid_err_text(e)); }

static void pid_cb(lv_obj_t *obj, lv_event_t event) {
  if (event != LV_EVENT_RELEASED) return;
  const int id = obj->mks_obj_id;
  if (id >= DLG_START_OK) {
    arq_dialog_close();
    switch (id) {
      case DLG_START_OK: { const uint8_t e = arq_pid_ctl_start(); if (e) error_dialog(e); } break;
      case DLG_CANCEL_OK: arq_pid_ctl_cancel(); break;
      case DLG_BACK_OK:   arq_pid_ctl_cancel(); break;       // el aviso de "cancelado" se vera al volver
      default: break;
    }
    refresh();
    return;
  }
  if (arq_dialog_is_open()) return;
  arq_pid_info_t in; arq_pid_ctl_info(&in);
  switch (id) {
    case ARQ_PV_BACK:
      if (in.st == ARQ_PID_RUNNING) arq_dialog_open("Autoajuste en curso", "Si sales ahora se cancela el ajuste y los calentadores se apagan.", "Cancelar ajuste", "Seguir", true, pid_cb, DLG_BACK_OK, DLG_BACK_NO);
      else { arq_pid_ctl_discard(); msg_pending = false; leave(); return; }
      break;
    case ARQ_PV_HOT: arq_pid_ctl_select(ARQ_PID_HOTEND); break;
    case ARQ_PV_BED: arq_pid_ctl_select(ARQ_PID_BED); break;
    case ARQ_PV_TDEC: arq_pid_ctl_target_delta(-1); break;
    case ARQ_PV_TINC: arq_pid_ctl_target_delta(+1); break;
    case ARQ_PV_CDEC: arq_pid_ctl_cycles_delta(-1); break;
    case ARQ_PV_CINC: arq_pid_ctl_cycles_delta(+1); break;
    case ARQ_PV_START: {
      const uint8_t e = arq_pid_ctl_can_start();
      if (e) { error_dialog(e); break; }
      snprintf(dlg_msg, sizeof(dlg_msg), "Se calentar\xC3\xA1 %s a %d\xC2\xB0" "C durante %d ciclos y se apagar\xC3\xA1n los dem\xC3\xA1s calentadores. Tarda varios minutos. No se aplica nada hasta que lo aceptes.",
               in.heater == ARQ_PID_BED ? "la cama" : "el hotend", (int)in.target, (int)in.cycles);
      arq_dialog_open("Iniciar autoajuste", dlg_msg, "Iniciar", "Volver", false, pid_cb, DLG_START_OK, DLG_START_NO);
    } break;
    case ARQ_PV_CANCEL:
      arq_dialog_open("Cancelar autoajuste", "Se detiene el ajuste, se apagan los calentadores y no se cambia ning\xC3\xBAn valor.", "Cancelar ajuste", "Seguir", true, pid_cb, DLG_CANCEL_OK, DLG_CANCEL_NO);
      break;
    case ARQ_PV_APPLY: {
      const uint8_t e = arq_pid_ctl_apply();
      if (e == ARQ_PID_OK) { snprintf(msg, sizeof(msg), "PID aplicado y guardado en la EEPROM."); msg_kind = ARQ_PV_MSG_OK; msg_pending = true; }
      else if (e == ARQ_PID_E_SAVE) { snprintf(msg, sizeof(msg), "PID aplicado pero NO guardado: %s. Se perder\xC3\xA1 al apagar.", arq_pid_err_text(e)); msg_kind = ARQ_PV_MSG_WARN; msg_pending = true; }
      else error_dialog(e);
    } break;
    case ARQ_PV_DISCARD: arq_pid_ctl_discard(); break;
    case ARQ_PV_OK: arq_pid_ctl_discard(); msg_pending = false; break;
    default: break;
  }
  refresh();
}

void arq_pid_draw() {
  scr = lv_screen_create(PID_UI, "");
  arq_screen_apply(scr);
  arq_pid_view_build(scr, pid_cb);
  task = lv_task_create(task_cb, 250, LV_TASK_PRIO_LOWEST, 0);
  refresh();
  add_to_group(scr);
}

void arq_pid_clear() {
  remove_group();
  arq_dialog_close();
  if (task) lv_task_del(task);
  task = nullptr;
  arq_pid_view_forget();
  if (scr) lv_obj_del(scr);
  scr = nullptr;
}

void arq_pid_poll() { arq_pid_ctl_poll(); }

#endif // ARQ_HAS_PID
