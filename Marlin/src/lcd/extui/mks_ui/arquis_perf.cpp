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

#include "arquis_perf.h"
#include "arquis_pid.h"
#include "arquis_mesh_ui.h"
#include "draw_ui.h"

#include "../../../inc/MarlinConfig.h"
#include "../../../MarlinCore.h"
#include "../../../module/planner.h"
#include "../../../module/stepper.h"
#include "../../../gcode/queue.h"
#include "../../../gcode/gcode.h"
#if ENABLED(EEPROM_SETTINGS)
  #include "../../../module/settings.h"
#endif
#if ARQ_HAS_MESH
  #include "arquis_mesh.h"
#endif

#include <string.h>

static bool    inited, coarse;
static uint8_t sel = ARQ_PF_ACC_P;
static float   ref[ARQ_PF_COUNT];

static float shaping_min() {
  #if HAS_ZV_SHAPING
    return (float)shaping_min_freq;          // stepper.h: tamano del buffer de Input Shaping compilado; por debajo de esta frecuencia se desborda
  #else
    return 0;
  #endif
}

static bool available(uint8_t id) {
  switch (id) {
    case ARQ_PF_K:  return ENABLED(LIN_ADVANCE);
    case ARQ_PF_JD: return DISABLED(CLASSIC_JERK);
    case ARQ_PF_FX: return ENABLED(INPUT_SHAPING_X);
    case ARQ_PF_FY: return ENABLED(INPUT_SHAPING_Y);
    default:        return id < ARQ_PF_COUNT;
  }
}

// Lo que Marlin usa ahora para cada valor
static float read(uint8_t id) {
  switch (id) {
    case ARQ_PF_ACC_P:  return planner.settings.acceleration;
    case ARQ_PF_ACC_T:  return planner.settings.travel_acceleration;
    case ARQ_PF_AMAX_X: return (float)planner.settings.max_acceleration_mm_per_s2[X_AXIS];
    case ARQ_PF_AMAX_Y: return (float)planner.settings.max_acceleration_mm_per_s2[Y_AXIS];
    #if ENABLED(LIN_ADVANCE)
      case ARQ_PF_K:    return planner.extruder_advance_K[0];
    #endif
    #if DISABLED(CLASSIC_JERK)
      case ARQ_PF_JD:   return planner.junction_deviation_mm;
    #endif
    #if ENABLED(INPUT_SHAPING_X)
      case ARQ_PF_FX:   return stepper.get_shaping_frequency(X_AXIS);
    #endif
    #if ENABLED(INPUT_SHAPING_Y)
      case ARQ_PF_FY:   return stepper.get_shaping_frequency(Y_AXIS);
    #endif
    default: return 0;
  }
}

static void ensure_init() {
  if (inited) return;
  for (uint8_t i = 0; i < ARQ_PF_COUNT; ++i) ref[i] = read(i);
  inited = true;
}

static bool printing() {
  return printJobOngoing() || printingIsActive() || printingIsPaused()
      || uiCfg.print_state == WORKING || uiCfg.print_state == PAUSING || uiCfg.print_state == PAUSED
      || uiCfg.print_state == RESUMING || uiCfg.print_state == REPRINTING || uiCfg.print_state == REPRINTED;
}

static uint8_t can_edit() {
  if (!IsRunning()) return ARQ_PF_E_STOPPED;
  if (printing()) return ARQ_PF_E_PRINTING;
  if (queue.has_commands_queued() || planner.has_blocks_queued()) return ARQ_PF_E_BUSY;
  #if ARQ_HAS_PID
    if (arq_pid_ctl_busy()) return ARQ_PF_E_BUSY;                 // M303 en curso
  #endif
  #if ARQ_HAS_MESH
    if (arq_mesh_ctl_session_active()) return ARQ_PF_E_BUSY;      // midiendo la malla
  #endif
  return ARQ_PF_OK;
}

// Envia la orden a Marlin (que valida) y comprueba que tomo el valor
static uint8_t apply(uint8_t id, float v) {
  char c[32];
  if (!arq_pf_fmt_cmd(c, sizeof c, id, v, shaping_min())) return ARQ_PF_E_PARAM;
  gcode.process_subcommands_now(c);
  return arq_pf_equal(id, read(id), v) ? ARQ_PF_OK : ARQ_PF_E_REJECT;
}

void arq_perf_ctl_select(uint8_t id) { ensure_init(); if (id < ARQ_PF_COUNT && available(id)) sel = id; }
void arq_perf_ctl_toggle_step() { coarse = !coarse; }

uint8_t arq_perf_ctl_delta(int8_t dir) {
  ensure_init();
  if (!available(sel)) return ARQ_PF_E_UNAVAILABLE;
  const uint8_t w = can_edit();
  if (w) return w;
  const float cur = read(sel), nv = arq_pf_step(sel, cur, dir, coarse, shaping_min());
  if (arq_pf_equal(sel, cur, nv)) return ARQ_PF_OK;               // ya esta en el limite
  return apply(sel, nv);
}

uint8_t arq_perf_ctl_save() {
  ensure_init();
  const uint8_t w = can_edit();
  if (w) return w;
  #if ENABLED(EEPROM_SETTINGS)
    if (!settings.save() || !settings.validate()) return ARQ_PF_E_SAVE;
    for (uint8_t i = 0; i < ARQ_PF_COUNT; ++i) ref[i] = read(i);
    return ARQ_PF_OK;
  #else
    return ARQ_PF_E_SAVE;
  #endif
}

uint8_t arq_perf_ctl_restore() {
  ensure_init();
  const uint8_t w = can_edit();
  if (w) return w;
  uint8_t r = ARQ_PF_OK;
  for (uint8_t i = 0; i < ARQ_PF_COUNT; ++i) {
    if (!available(i) || arq_pf_equal(i, read(i), ref[i])) continue;
    const uint8_t e = apply(i, ref[i]);
    if (e) r = e;
  }
  return r;
}

void arq_perf_ctl_info(arq_perf_info_t *o) {
  ensure_init();
  memset(o, 0, sizeof(*o));
  o->shaping_min = shaping_min(); o->sel = sel; o->coarse = coarse; o->why = can_edit();
  for (uint8_t i = 0; i < ARQ_PF_COUNT; ++i) {
    o->has[i] = available(i); o->val[i] = read(i); o->ref[i] = ref[i];
    if (o->has[i] && !arq_pf_equal(i, o->val[i], ref[i])) o->dirty = true;
  }
}

#endif // HAS_TFT_LVGL_UI && ARQUIS_UI
