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

#if HAS_TFT_LVGL_UI && ENABLED(ARQUIS_UI) && (ENABLED(PIDTEMP) || ENABLED(PIDTEMPBED))

#include "arquis_pid.h"
#include "arquis_pid_status.h"
#include "arquis_mesh_ui.h"
#include "draw_ui.h"

#include "../../../inc/MarlinConfig.h"
#include "../../../MarlinCore.h"
#include "../../../module/temperature.h"
#include "../../../module/planner.h"
#include "../../../gcode/queue.h"
#include "../../../gcode/gcode.h"
#if ENABLED(EEPROM_SETTINGS)
  #include "../../../module/settings.h"
#endif
#if ARQ_HAS_MESH
  #include "arquis_mesh.h"
#endif

#include <stdio.h>
#include <string.h>
#include <math.h>

arq_pid_status_t arq_pid_status;                       // lo rellena Temperature::PID_autotune (parche de temperature.cpp)
void arq_pid_idle() { LV_TASK_HANDLER(); }             // atiende la pantalla mientras M303 bloquea la cola

static arq_pid_sess_t sess;
static bool     inited, cmd_sent, cancel_req;
static uint8_t  sel_heater = ARQ_PID_HOTEND;
static int16_t  sel_target[ARQ_PID_HEATERS];
static uint8_t  sel_cycles[ARQ_PID_HEATERS];
static millis_t t0;
static char     cmd[24];

static bool has_heater(uint8_t h) {
  if (h == ARQ_PID_BED) return ENABLED(HAS_HEATED_BED) && ENABLED(PIDTEMPBED);
  return ENABLED(HAS_HOTEND) && ENABLED(PIDTEMP);
}

static int16_t max_target(uint8_t h) {
  #if HAS_HEATED_BED
    if (h == ARQ_PID_BED) return (int16_t)(BED_MAX_TARGET);
  #endif
  return (int16_t)((HEATER_0_MAXTEMP) - (HOTEND_OVERSHOOT));        // el mismo limite que aplica Marlin en PID_autotune
}

static void ensure_init() {
  if (inited) return;
  arq_pid_sess_reset(&sess);
  for (uint8_t h = 0; h < ARQ_PID_HEATERS; ++h) {
    const arq_pid_limits_t l = arq_pid_limits(h, max_target(h));
    sel_target[h] = l.def; sel_cycles[h] = l.cyc_def;
  }
  if (!has_heater(ARQ_PID_HOTEND) && has_heater(ARQ_PID_BED)) sel_heater = ARQ_PID_BED;
  inited = true;
}

static void read_pid(uint8_t h, float out[3]) {
  out[0] = out[1] = out[2] = 0;
  #if ENABLED(PIDTEMP)
    if (h == ARQ_PID_HOTEND) { out[0] = thermalManager.temp_hotend[0].pid.p(); out[1] = thermalManager.temp_hotend[0].pid.i(); out[2] = thermalManager.temp_hotend[0].pid.d(); }
  #endif
  #if ENABLED(PIDTEMPBED)
    if (h == ARQ_PID_BED) { out[0] = thermalManager.temp_bed.pid.p(); out[1] = thermalManager.temp_bed.pid.i(); out[2] = thermalManager.temp_bed.pid.d(); }
  #endif
  (void)h;
}

static bool printing() {
  return printJobOngoing() || printingIsActive() || printingIsPaused()
      || uiCfg.print_state == WORKING || uiCfg.print_state == PAUSING || uiCfg.print_state == PAUSED
      || uiCfg.print_state == RESUMING || uiCfg.print_state == REPRINTING || uiCfg.print_state == REPRINTED;
}

bool arq_pid_ctl_busy() { ensure_init(); return arq_pid_is_running(&sess); }

void arq_pid_ctl_select(uint8_t h) { ensure_init(); if (!arq_pid_is_running(&sess) && h < ARQ_PID_HEATERS && has_heater(h)) sel_heater = h; }

void arq_pid_ctl_target_delta(int8_t dir) {
  ensure_init();
  if (arq_pid_is_running(&sess)) return;
  sel_target[sel_heater] = arq_pid_clamp_target(sel_heater, (int16_t)(sel_target[sel_heater] + (dir > 0 ? ARQ_PID_STEP : -ARQ_PID_STEP)), max_target(sel_heater));
}

void arq_pid_ctl_cycles_delta(int8_t dir) {
  ensure_init();
  if (arq_pid_is_running(&sess)) return;
  sel_cycles[sel_heater] = (uint8_t)arq_pid_clamp_cycles(sel_cycles[sel_heater] + (dir > 0 ? 1 : -1));
}

uint8_t arq_pid_ctl_can_start() {
  ensure_init();
  if (!IsRunning()) return ARQ_PID_E_STOPPED;
  if (printing()) return ARQ_PID_E_PRINTING;
  if (arq_pid_is_running(&sess) || queue.has_commands_queued() || planner.has_blocks_queued()) return ARQ_PID_E_BUSY;
  #if ARQ_HAS_MESH
    if (arq_mesh_ctl_session_active()) return ARQ_PID_E_BUSY;        // calibrando la cama: no se calienta ni se apaga nada
  #endif
  if (!has_heater(sel_heater)) return ARQ_PID_E_PARAM;
  return ARQ_PID_OK;
}

uint8_t arq_pid_ctl_start() {
  uint8_t e = arq_pid_ctl_can_start();
  if (e) return e;
  float old[3]; read_pid(sel_heater, old);
  e = arq_pid_begin(&sess, sel_heater, sel_target[sel_heater], sel_cycles[sel_heater], max_target(sel_heater), old);
  if (e) return e;
  if (!arq_pid_fmt_start(cmd, sizeof cmd, sel_heater, sess.target, sess.cycles)) { arq_pid_fail(&sess, ARQ_PID_E_PARAM); return ARQ_PID_E_PARAM; }
  arq_pid_status.result = ARQ_PID_R_NONE; arq_pid_status.cycle = 0; arq_pid_status.total = sess.cycles;
  cmd_sent = false; cancel_req = false; t0 = millis();
  return ARQ_PID_OK;
}

void arq_pid_ctl_cancel() {
  ensure_init();
  if (!arq_pid_is_running(&sess)) return;
  cancel_req = true;
  if (!cmd_sent) { arq_pid_fail(&sess, ARQ_PID_E_CANCEL); cancel_req = false; }     // aun no habia empezado
  else wait_for_heatup = false;                                                      // como M108: el bucle de Marlin termina y apaga los calentadores
}

void arq_pid_ctl_poll() {
  ensure_init();
  if (!arq_pid_is_running(&sess)) return;
  if (!IsRunning()) { arq_pid_fail(&sess, ARQ_PID_E_STOPPED); return; }
  arq_pid_progress(&sess, arq_pid_status.cycle);
  if (!cmd_sent) {
    if (!queue.has_commands_queued()) { queue.inject(cmd); cmd_sent = true; t0 = millis(); }
    return;
  }
  if (cancel_req && arq_pid_status.result == ARQ_PID_R_RUNNING) wait_for_heatup = false;   // (repetido por si M303 aun no habia arrancado)
  if (!queue.has_commands_queued()) {                                                        // M303 termino
    arq_pid_finish_result(&sess, arq_pid_status.result, arq_pid_status.kp, arq_pid_status.ki, arq_pid_status.kd);
    cancel_req = false;
  }
}

uint8_t arq_pid_ctl_apply() {
  ensure_init();
  if (sess.st != ARQ_PID_RESULT) return ARQ_PID_E_STATE;
  if (printing()) return ARQ_PID_E_PRINTING;
  char c[48];
  if (!arq_pid_fmt_apply(c, sizeof c, sess.heater, sess.new_pid[0], sess.new_pid[1], sess.new_pid[2])) return ARQ_PID_E_BAD_RESULT;
  gcode.process_subcommands_now(c);                    // M301 / M304: Marlin valida y actualiza su regulador
  float now[3]; read_pid(sess.heater, now);
  if (fabsf(now[0] - sess.new_pid[0]) > 0.01f * fabsf(sess.new_pid[0]) + 0.001f) return ARQ_PID_E_STATE;   // Marlin no tomo los valores
  uint8_t r = ARQ_PID_OK;
  #if ENABLED(EEPROM_SETTINGS)
    if (!settings.save() || !settings.validate()) r = ARQ_PID_E_SAVE;
  #else
    r = ARQ_PID_E_SAVE;
  #endif
  arq_pid_discard(&sess);                              // los valores ya estan aplicados: la sesion termina
  return r;
}

void arq_pid_ctl_discard() { ensure_init(); arq_pid_discard(&sess); }

void arq_pid_ctl_info(arq_pid_info_t *o) {
  ensure_init();
  memset(o, 0, sizeof(*o));
  const bool run = sess.st != ARQ_PID_IDLE;
  o->st = sess.st; o->err = sess.err;
  o->heater = run ? sess.heater : sel_heater;
  o->cycle = sess.cycle; o->cycles = run ? sess.cycles : sel_cycles[o->heater];
  o->target = run ? sess.target : sel_target[o->heater];
  o->max_target = max_target(o->heater); o->min_target = arq_pid_limits(o->heater, o->max_target).min;
  for (uint8_t h = 0; h < ARQ_PID_HEATERS; ++h) { o->has[h] = has_heater(h); read_pid(h, o->cur_pid[h]); }
  memcpy(o->old_pid, sess.old_pid, sizeof o->old_pid); memcpy(o->new_pid, sess.new_pid, sizeof o->new_pid);
  #if HAS_HOTEND
    if (o->heater == ARQ_PID_HOTEND) o->temp_cur = thermalManager.wholeDegHotend(0);
  #endif
  #if HAS_HEATED_BED
    if (o->heater == ARQ_PID_BED) o->temp_cur = thermalManager.wholeDegBed();
  #endif
  o->elapsed_s = sess.st == ARQ_PID_RUNNING ? (uint32_t)((millis() - t0) / 1000UL) : 0;
}

#endif // HAS_TFT_LVGL_UI && ARQUIS_UI && PID
