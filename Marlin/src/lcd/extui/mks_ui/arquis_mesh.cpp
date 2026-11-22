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
 * MERCHANTABILITY or FITNESS FOR area PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "../../../inc/MarlinConfigPre.h"

#if HAS_TFT_LVGL_UI && ENABLED(ARQUIS_UI) && ENABLED(AUTO_BED_LEVELING_BILINEAR) && ENABLED(PROBE_MANUALLY)

#include "arquis_mesh.h"
#include "draw_ui.h"

#include "../../../inc/MarlinConfig.h"
#include "../../../MarlinCore.h"
#include "../../../module/motion.h"
#include "../../../module/planner.h"
#include "../../../module/temperature.h"
#include "../../../module/probe.h"
#include "../../../feature/bedlevel/bedlevel.h"
#include "../../../gcode/queue.h"
#if ENABLED(EEPROM_SETTINGS)
  #include "../../../module/settings.h"
#endif

#include <stdio.h>
#include <string.h>
#include <math.h>

#ifndef MANUAL_PROBE_START_Z
  #define MANUAL_PROBE_START_Z 1.0
#endif
#define ARQ_MESH_START_Z      ((float)(MANUAL_PROBE_START_Z))
#define ARQ_MESH_TRAVEL_Z     ((float)(Z_CLEARANCE_BETWEEN_PROBES))   // altura de desplazamiento entre puntos (mm)
#define ARQ_MESH_SAFE_Z       10.0f                                    // altura a la que se deja el cabezal al terminar
#define ARQ_MESH_Z_TOL        0.005f

#if ANY(ENABLE_LEVELING_AFTER_G28, RESTORE_LEVELING_AFTER_G28)
  #define ARQ_HOME_CMD "G90\nG28L0"          // homea sin activar la compensacion (ENABLE_LEVELING_AFTER_G28 la activaria)
#else
  #define ARQ_HOME_CMD "G90\nG28"
#endif

static arq_mesh_sess_t sess;
static bool     inited, move_sent, home_sent, lift_sent, end_lift_sent, bed_changed, prev_leveling, restore_feed;
static uint8_t  prep;
static int16_t  bed_sel = ARQ_MESH_BED_DEFAULT, prev_bed;
static millis_t t_mark, soak_end;
static float    prev_feed, start_z_now;
static arq_mesh_area_t area;

static void ensure_init() {
  if (inited) return;
  arq_mesh_sess_reset(&sess);
  inited = true;
}

static void area_update() {
  area.x_min = probe.min_x(); area.x_max = probe.max_x();
  area.y_min = probe.min_y(); area.y_max = probe.max_y();
}
const arq_mesh_area_t * arq_mesh_ctl_area() { area_update(); return &area; }

static bool printing() {
  return printJobOngoing() || printingIsActive() || printingIsPaused()
      || uiCfg.print_state == WORKING || uiCfg.print_state == PAUSING || uiCfg.print_state == PAUSED
      || uiCfg.print_state == RESUMING || uiCfg.print_state == REPRINTING || uiCfg.print_state == REPRINTED;
}

static bool busy() { return queue.has_commands_queued() || planner.has_blocks_queued(); }

// Inyecta G-code por la cola normal. Solo si no hay otra orden pendiente (queue.inject() sobrescribe la anterior).
static bool inj(const char *cmd) {
  if (queue.has_commands_queued()) return false;
  queue.inject(cmd);
  return true;
}

// Durante la medicion hay que poder bajar de Z=0. OJO: SET_SOFT_ENDSTOP_LOOSE solo afecta a enabled() (limites manuales del LCD de Marlin);
// los G1 pasan por apply_motion_limits(), que solo mira soft_endstop._enabled y recorta Z a soft_endstop.min.z (=Z_MIN_POS=0). Por eso
// ademas se baja el minimo de Z a ARQ_MESH_Z_MIN y, al terminar, se recalcula con update_software_endstops() (valor original de Marlin).
static void set_loose(bool on) {
  SET_SOFT_ENDSTOP_LOOSE(on);
  #if HAS_SOFTWARE_ENDSTOPS
    if (on) soft_endstop.min.z = ARQ_MESH_Z_MIN;
    else    update_software_endstops(Z_AXIS);
  #endif
}

int16_t arq_mesh_ctl_bed_max() {
  #if HAS_HEATED_BED
    return (int16_t)(BED_MAX_TARGET > 110 ? 110 : BED_MAX_TARGET);
  #else
    return 0;
  #endif
}

void arq_mesh_ctl_bed_delta(int8_t dir) {
  ensure_init();
  if (arq_mesh_is_active(&sess)) return;
  int16_t v = bed_sel + (dir > 0 ? ARQ_MESH_BED_STEP : -ARQ_MESH_BED_STEP);
  const int16_t mx = arq_mesh_ctl_bed_max();
  if (v > mx) v = mx;
  if (v < 0) v = 0;
  bed_sel = v;
}

void arq_mesh_ctl_select_step(uint8_t idx) { ensure_init(); arq_mesh_select_step(&sess, idx); }
bool arq_mesh_ctl_session_active() { ensure_init(); return arq_mesh_is_active(&sess) || prep == ARQ_PP_END; }

// ---- Final de sesion: devolver la maquina a un estado seguro ---------------------------------------------------------------------
// quiet: no tocar nada de la maquina (hay una impresion en curso o Marlin esta detenido): solo soltar lo que este asistente cambio.
static void cleanup_begin(bool quiet) {
  set_loose(false);
  move_sent = home_sent = lift_sent = false;
  end_lift_sent = quiet;
  prep = ARQ_PP_END;
  if (quiet) { bed_changed = false; prev_leveling = false; restore_feed = false; }   // con una impresion en curso no se toca nada de lo suyo
}

static void cleanup_poll() {
  if (busy()) return;
  if (!end_lift_sent) {
    if (all_axes_homed() && current_position.z < ARQ_MESH_SAFE_Z) {
      char b[24], z[10]; arq_mesh_fmt_z(z, sizeof z, ARQ_MESH_SAFE_Z);
      snprintf(b, sizeof b, "G1 Z%s F600", z);
      if (!inj(b)) return;
    }
    end_lift_sent = true;
    return;
  }
  // Z a salvo: restablecer el estado anterior
  if (prev_leveling && leveling_is_valid()) set_bed_leveling_enabled(true);
  #if HAS_HEATED_BED
    if (bed_changed) thermalManager.setTargetBed(prev_bed);
  #endif
  bed_changed = false;
  if (restore_feed) feedrate_mm_s = prev_feed;
  restore_feed = false;
  prep = ARQ_PP_NONE;
}

static void fail(uint8_t err) {
  const bool quiet = printing() || !IsRunning();
  arq_mesh_fail(&sess, err);
  cleanup_begin(quiet);
}

// ---- Arranque --------------------------------------------------------------------------------------------------------------------
uint8_t arq_mesh_ctl_can_start() {
  ensure_init();
  if (!IsRunning()) return ARQ_MESH_E_STATE;
  if (printing()) return ARQ_MESH_E_PRINTING;
  if (g29_in_progress) return ARQ_MESH_E_G29_BUSY;
  if (arq_mesh_is_active(&sess) || prep != ARQ_PP_NONE) return ARQ_MESH_E_STATE;
  area_update();
  if (!arq_mesh_area_ok(&area)) return ARQ_MESH_E_LAYOUT;
  return ARQ_MESH_OK;
}

uint8_t arq_mesh_ctl_start(int16_t bed_temp) {
  const uint8_t e = arq_mesh_ctl_can_start();
  if (e) return e;
  if (bed_temp < 0 || bed_temp > arq_mesh_ctl_bed_max()) return ARQ_MESH_E_TEMP;
  if (!arq_mesh_begin(&sess)) return ARQ_MESH_E_STATE;
  prev_leveling = planner.leveling_active;
  prev_feed = feedrate_mm_s; restore_feed = true;
  #if HAS_HEATED_BED
    prev_bed = thermalManager.degTargetBed();
    bed_changed = false;
    if (bed_temp > 0) { thermalManager.setTargetBed(bed_temp); bed_changed = true; }
  #else
    prev_bed = 0; bed_changed = false;
  #endif
  bed_sel = bed_temp;
  move_sent = home_sent = lift_sent = end_lift_sent = false;
  t_mark = millis();
  prep = ARQ_PP_HEAT;
  return ARQ_MESH_OK;
}

// ---- Movimiento al punto en curso ------------------------------------------------------------------------------------------------
static bool send_move_to_point() {
  float x, y;
  arq_mesh_point_xy(&area, sess.cur, &x, &y);
  start_z_now = arq_mesh_start_z(sess.have_last, sess.last_z, ARQ_MESH_START_Z);
  float travel = ARQ_MESH_TRAVEL_Z;
  if (current_position.z > travel) travel = current_position.z;       // nunca se baja a la altura de desplazamiento
  char tz[10], xs[10], ys[10], sz[10], b[64];
  arq_mesh_fmt_z(tz, sizeof tz, travel); arq_mesh_fmt_z(xs, sizeof xs, x); arq_mesh_fmt_z(ys, sizeof ys, y); arq_mesh_fmt_z(sz, sizeof sz, start_z_now);
  const int n = snprintf(b, sizeof b, "G1 Z%s F600\nG1 X%s Y%s F%d\nG1 Z%s F300", tz, xs, ys, (int)(XY_PROBE_FEEDRATE), sz);
  if (n <= 0 || n >= (int)sizeof b) return false;                      // no cabe en el bufer de inyeccion de Marlin (64 B): no se envia nada
  return inj(b);
}

// ---- Bucle principal -------------------------------------------------------------------------------------------------------------
void arq_mesh_ctl_poll() {
  ensure_init();
  if (prep == ARQ_PP_END) { cleanup_poll(); return; }
  if (!arq_mesh_is_active(&sess)) return;

  // Condiciones que invalidan la sesion en cualquier momento
  if (printing())            { fail(ARQ_MESH_E_PRINTING); return; }
  if (!IsRunning())          { fail(ARQ_MESH_E_STATE); return; }
  if (g29_in_progress)       { fail(ARQ_MESH_E_G29_BUSY); return; }
  if (sess.st != ARQ_MESH_PREP && planner.leveling_active) { fail(ARQ_MESH_E_STATE); return; }   // p. ej. un G28 desde la consola la activo

  switch (sess.st) {
    case ARQ_MESH_PREP:
      switch (prep) {
        case ARQ_PP_HEAT: {
          #if HAS_HEATED_BED
            if (bed_sel == 0 || thermalManager.wholeDegBed() >= bed_sel - 2) { soak_end = millis() + SEC_TO_MS(bed_sel ? ARQ_MESH_SOAK_S : 0); prep = ARQ_PP_SOAK; }
            else if (millis() - t_mark > SEC_TO_MS(ARQ_MESH_HEAT_TIMEOUT_S)) fail(ARQ_MESH_E_TEMP);
          #else
            prep = ARQ_PP_SOAK; soak_end = millis();
          #endif
        } break;
        case ARQ_PP_SOAK:
          if ((int32_t)(millis() - soak_end) >= 0) { prep = ARQ_PP_HOME; home_sent = false; }
          break;
        case ARQ_PP_HOME:
          if (!home_sent) { if (inj(ARQ_HOME_CMD)) { home_sent = true; t_mark = millis(); } }
          else if (!busy()) {
            if (all_axes_homed()) {
              set_loose(true);                  // permite Z negativa durante la medicion (se restablece al terminar, cancelar o fallar)
              area_update();
              if (!arq_mesh_prep_done(&sess)) { fail(ARQ_MESH_E_STATE); break; }
              prep = ARQ_PP_NONE; move_sent = false;
            }
            else if (millis() - t_mark > 3000UL) fail(ARQ_MESH_E_NOT_HOMED);
          }
          break;
        default: break;
      }
      break;

    case ARQ_MESH_MOVING:
      if (!move_sent) { if (send_move_to_point()) move_sent = true; }
      else if (!busy()) {
        if (!all_axes_homed()) { fail(ARQ_MESH_E_NOT_HOMED); break; }
        if (fabsf(current_position.z - start_z_now) > 0.02f) { fail(ARQ_MESH_E_Z_SYNC); break; }
        arq_mesh_arrived(&sess, current_position.z);
        move_sent = false;
      }
      break;

    case ARQ_MESH_REVIEW:
      if (!lift_sent && !busy() && all_axes_homed() && current_position.z < ARQ_MESH_TRAVEL_Z) {
        char b[24], z[10]; arq_mesh_fmt_z(z, sizeof z, ARQ_MESH_TRAVEL_Z);
        snprintf(b, sizeof b, "G1 Z%s F600", z);
        if (inj(b)) lift_sent = true;
      }
      break;

    default: break;
  }
}

// ---- Acciones del usuario --------------------------------------------------------------------------------------------------------
bool arq_mesh_ctl_jog(int8_t dir) {
  ensure_init();
  if (sess.st != ARQ_MESH_ADJUST || busy()) return false;
  const float n = arq_mesh_step_z(sess.z_now, dir, arq_mesh_step_mm[sess.step_idx < ARQ_MESH_STEPS ? sess.step_idx : 1]);
  if (!(n == n) || n == sess.z_now) return false;
  char z[10], b[24]; arq_mesh_fmt_z(z, sizeof z, n);
  snprintf(b, sizeof b, "G1 Z%s F300", z);
  if (!inj(b)) return false;
  return arq_mesh_jog(&sess, dir);
}

uint8_t arq_mesh_ctl_confirm() {
  ensure_init();
  if (sess.st != ARQ_MESH_ADJUST || busy()) return ARQ_MESH_E_STATE;
  if (planner.leveling_active) { fail(ARQ_MESH_E_STATE); return ARQ_MESH_E_STATE; }
  const float z = current_position.z;                                // la Z REAL de Marlin (compensacion apagada: es la Z de la maquina)
  if (fabsf(z - sess.z_now) > ARQ_MESH_Z_TOL) { sess.z_now = z; return ARQ_MESH_E_Z_SYNC; }
  const uint8_t r = arq_mesh_confirm(&sess, z);
  if (r == ARQ_MESH_OK) { move_sent = false; lift_sent = false; }
  return r;
}

uint8_t arq_mesh_ctl_repeat(uint8_t p) {
  ensure_init();
  const uint8_t r = arq_mesh_repeat(&sess, p);
  if (r == ARQ_MESH_OK) { move_sent = false; lift_sent = false; }
  return r;
}

void arq_mesh_ctl_cancel() {
  ensure_init();
  if (!arq_mesh_is_active(&sess)) return;
  const bool quiet = printing() || !IsRunning();
  arq_mesh_cancel(&sess);
  cleanup_begin(quiet);
}

void arq_mesh_ctl_ack() {
  ensure_init();
  if (prep != ARQ_PP_NONE || arq_mesh_is_active(&sess)) return;
  const uint8_t step = sess.step_idx;
  arq_mesh_sess_reset(&sess);
  sess.step_idx = step;
}

// ---- Malla de Marlin -------------------------------------------------------------------------------------------------------------
static void stored_values(float out[ARQ_MESH_PTS]) {
  for (uint8_t iy = 0; iy < ARQ_MESH_N; ++iy) for (uint8_t ix = 0; ix < ARQ_MESH_N; ++ix) out[iy * ARQ_MESH_N + ix] = bedlevel.z_values[ix][iy];
}

static uint8_t stored_status(float *vals = nullptr) {
  float v[ARQ_MESH_PTS];
  stored_values(v);
  if (vals) memcpy(vals, v, sizeof v);
  area_update();
  return arq_mesh_check_stored(v, bedlevel.grid_start.x, bedlevel.grid_start.y, bedlevel.grid_spacing.x, bedlevel.grid_spacing.y, &area);
}

// ENTREGA ATOMICA de la malla a Marlin. Es el UNICO sitio que escribe en bedlevel. Todo se valida ANTES de la primera escritura;
// las escrituras son asignaciones simples. Si la comprobacion posterior falla se restaura la malla anterior.
uint8_t arq_mesh_ctl_commit() {
  ensure_init();
  if (sess.st != ARQ_MESH_REVIEW) return ARQ_MESH_E_STATE;
  if (printing()) return ARQ_MESH_E_PRINTING;
  if (g29_in_progress) return ARQ_MESH_E_G29_BUSY;
  float c[ARQ_MESH_PTS];
  uint8_t e = arq_mesh_corrections(&sess, c);
  if (e) return e;                                                   // incompleta / no finita / > 2 mm: la sesion sigue en REVIEW para repetir puntos
  area_update();
  if (!arq_mesh_area_ok(&area)) return ARQ_MESH_E_LAYOUT;

  planner.synchronize();
  // Copia de seguridad de la malla actual
  bed_mesh_t old_z; memcpy(old_z, bedlevel.z_values, sizeof old_z);
  const xy_pos_t old_sp = bedlevel.grid_spacing, old_st = bedlevel.grid_start;
  const bool was_on = planner.leveling_active;

  set_bed_leveling_enabled(false);
  xy_pos_t sp, st;
  sp.set(arq_mesh_spacing_x(&area), arq_mesh_spacing_y(&area));
  st.set(area.x_min, area.y_min);
  bedlevel.set_grid(sp, st);
  for (uint8_t iy = 0; iy < ARQ_MESH_N; ++iy) for (uint8_t ix = 0; ix < ARQ_MESH_N; ++ix) bedlevel.z_values[ix][iy] = c[iy * ARQ_MESH_N + ix];
  bedlevel.refresh_bed_level();

  e = stored_status();
  if (e == ARQ_MESH_OK) {
    set_bed_leveling_enabled(true);
    if (!planner.leveling_active) e = ARQ_MESH_E_APPLY;
  }
  if (e != ARQ_MESH_OK) {                                            // restaurar la malla anterior
    bedlevel.set_grid(old_sp, old_st);
    memcpy(bedlevel.z_values, old_z, sizeof old_z);
    bedlevel.refresh_bed_level();
    if (was_on && leveling_is_valid()) set_bed_leveling_enabled(true);
    return e;
  }
  prev_leveling = true;                                              // la compensacion ya esta activa: al terminar no hay que restaurar nada distinto
  arq_mesh_finish(&sess);
  cleanup_begin(false);
  return ARQ_MESH_OK;
}

uint8_t arq_mesh_ctl_save() {
  ensure_init();
  #if ENABLED(EEPROM_SETTINGS)
    if (printing()) return ARQ_MESH_E_PRINTING;
    if (arq_mesh_ctl_session_active() && sess.st != ARQ_MESH_DONE) return ARQ_MESH_E_STATE;
    const uint8_t e = stored_status();
    if (e) return e;                                                 // no se guarda una malla inexistente, incompleta o inconsistente
    if (!settings.save()) return ARQ_MESH_E_EEPROM;
    if (!settings.validate()) return ARQ_MESH_E_EEPROM;              // relectura y comprobacion de CRC
    return ARQ_MESH_OK;
  #else
    return ARQ_MESH_E_EEPROM;
  #endif
}

uint8_t arq_mesh_ctl_load() {
  ensure_init();
  #if ENABLED(EEPROM_SETTINGS)
    if (printing()) return ARQ_MESH_E_PRINTING;
    if (arq_mesh_ctl_session_active()) return ARQ_MESH_E_STATE;
    // settings.load() reinicia TODOS los ajustes si la EEPROM no es valida: se comprueba antes con validate() (solo lectura).
    if (!settings.validate()) return ARQ_MESH_E_EEPROM;
    if (!settings.load()) return ARQ_MESH_E_EEPROM;
    const uint8_t e = stored_status();
    if (e) return e;                                                 // malla ausente o inconsistente: no se activa
    set_bed_leveling_enabled(true);
    return planner.leveling_active ? ARQ_MESH_OK : ARQ_MESH_E_APPLY;
  #else
    return ARQ_MESH_E_EEPROM;
  #endif
}

uint8_t arq_mesh_ctl_set_leveling(bool on) {
  ensure_init();
  if (printing()) return ARQ_MESH_E_PRINTING;                        // cambiar la compensacion en plena impresion desplazaria la pieza
  if (arq_mesh_ctl_session_active()) return ARQ_MESH_E_STATE;
  if (on) { const uint8_t e = stored_status(); if (e) return e; }
  set_bed_leveling_enabled(on);
  return planner.leveling_active == on ? ARQ_MESH_OK : ARQ_MESH_E_APPLY;
}

// ---- Instantanea para la pantalla ------------------------------------------------------------------------------------------------
void arq_mesh_ctl_info(arq_mesh_info_t *o) {
  ensure_init();
  memset(o, 0, sizeof(*o));
  o->st = sess.st; o->err = sess.err; o->cur = sess.cur; o->step_idx = sess.step_idx; o->redo = sess.redo;
  o->prep = prep; o->count = arq_mesh_count(&sess); o->percent = arq_mesh_percent(&sess);
  o->z_now = sess.z_now;
  o->busy = busy();
  o->leveling_on = planner.leveling_active;
  #if HAS_HEATED_BED
    o->bed_cur = thermalManager.wholeDegBed();
    o->bed_set = bed_sel;
  #endif
  if (prep == ARQ_PP_SOAK) {
    const int32_t left = (int32_t)(soak_end - millis());
    o->soak_s = left > 0 ? (uint8_t)((left + 999) / 1000) : 0;
  }
  #if ENABLED(ENABLE_LEVELING_FADE_HEIGHT)
    o->fade_mm = planner.z_fade_height;
  #endif
  float stored[ARQ_MESH_PTS];
  o->mesh_err = stored_status(stored);
  o->mesh_ok = (o->mesh_err == ARQ_MESH_OK);

  if (arq_mesh_is_active(&sess)) {
    o->mask = sess.done;
    if (sess.st == ARQ_MESH_REVIEW) {
      float c[ARQ_MESH_PTS];
      const uint8_t e = arq_mesh_corrections(&sess, c);
      if (e == ARQ_MESH_OK) { memcpy(o->val, c, sizeof c); o->rel = true; }
      else { memcpy(o->val, sess.z, sizeof o->val); o->err = e; }       // p. ej. desviacion excesiva: se muestran las Z absolutas y el error
    }
    else memcpy(o->val, sess.z, sizeof o->val);
  }
  else if (o->mesh_ok) { memcpy(o->val, stored, sizeof stored); o->mask = 0x1FFFFFFUL; o->rel = true; }
}

#endif // HAS_TFT_LVGL_UI && ARQUIS_UI && AUTO_BED_LEVELING_BILINEAR && PROBE_MANUALLY
