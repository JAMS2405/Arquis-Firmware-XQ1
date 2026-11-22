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

#include "arquis_data.h"
#include "draw_ui.h"       // gCfgItems / uiCfg: tamano del archivo y estado de impresion de la interfaz original

#include "../../../inc/MarlinConfig.h"
#include "../../../MarlinCore.h"
#include "../../../module/temperature.h"
#include "../../../module/printcounter.h"
#include "../../../module/motion.h"
#include "../../../module/planner.h"
#include "../../../sd/cardreader.h"

#include <string.h>

// Marlin solo define estas histeresis si el sensor existe; valor por defecto de Marlin si no.
#ifndef TEMP_HYSTERESIS
  #define TEMP_HYSTERESIS 3
#endif
#ifndef TEMP_BED_HYSTERESIS
  #define TEMP_BED_HYSTERESIS 3
#endif

extern uint32_t To_pre_view;

// Mismo calculo que la barra de la interfaz original (setProBarRate): descuenta la miniatura incrustada en el G-code, para que el
// porcentaje coincida con el relleno de la miniatura. Solo vale si el trabajo lo inicio esta interfaz (curFilesize es suyo);
// para cualquier otro trabajo (p. ej. desde un host) se usa el porcentaje de Marlin.
static uint8_t job_percent() {
  const uint8_t st = uiCfg.print_state;
  if ((st == WORKING || st == PAUSING || st == PAUSED || st == RESUMING || st == REPRINTED) && gCfgItems.curFilesize > 0) {
    int64_t idx = (int64_t)card.getIndex(), size = (int64_t)gCfgItems.curFilesize;
    if (gCfgItems.from_flash_pic) { const int64_t off = (int64_t)PREVIEW_SIZE + (int64_t)To_pre_view; idx -= off; size -= off; }
    if (size > 0) { const int64_t r = idx * 100 / size; return r < 0 ? 0 : r > 100 ? 100 : (uint8_t)r; }
  }
  return (uint8_t)card.percentDone();
}

bool arq_data_job_active() { return printingIsActive() || printingIsPaused(); }

// Velocidad real: posicion de X e Y segun los pasos YA EJECUTADOS por el stepper (planner.get_axis_position_mm, la misma que usa M114 D),
// derivada en el tiempo. Solo se mide durante una impresion; al terminar se pone a cero y la siguiente empieza con el maximo en cero.
static arq_speed_t speed;
void arq_data_speed_poll() {
  if (!arq_data_job_active()) { if (speed.have || speed.peak > 0) arq_speed_reset(&speed); return; }
  arq_speed_sample(&speed, (uint32_t)millis(), planner.get_axis_position_mm(X_AXIS), planner.get_axis_position_mm(Y_AXIS));
}

void arq_data_read(arq_snapshot_t *s) {
  memset(s, 0, sizeof(*s));                                  // incluye el relleno: permite comparar con memcmp

  // --- Temperaturas (thermalManager, igual que el resto de pantallas MKS) ---
  #if HAS_HOTEND
    s->hot = { true, (int16_t)thermalManager.wholeDegHotend(0), (int16_t)thermalManager.degTargetHotend(0), (int16_t)(TEMP_HYSTERESIS) };
  #endif
  #if HAS_HEATED_BED
    s->bed = { true, (int16_t)thermalManager.wholeDegBed(), (int16_t)thermalManager.degTargetBed(), (int16_t)(TEMP_BED_HYSTERESIS) };
  #endif

  // --- Ventiladores: porcentaje real de Marlin. Marlin no mide RPM en esta placa: no se muestra ninguna lectura de RPM ---
  #if HAS_FAN0
    s->fan[s->fan_n++] = { ARQ_FAN_LAYER, 0, (uint8_t)thermalManager.fanSpeedPercent(0) };
  #endif
  #if PIN_EXISTS(E0_AUTO_FAN)
    // Ventilador del disipador gobernado por Marlin (E0_AUTO_FAN_PIN): se enciende a EXTRUDER_AUTO_FAN_SPEED cuando
    // el hotend supera EXTRUDER_AUTO_FAN_TEMPERATURE (regla de Temperature::auto_fan_check). No hay lectura directa.
    if (s->fan_n < 3) s->fan[s->fan_n++] = { ARQ_FAN_HEATSINK, 0,
      (uint8_t)(thermalManager.wholeDegHotend(0) >= (EXTRUDER_AUTO_FAN_TEMPERATURE) ? ui8_to_percent(EXTRUDER_AUTO_FAN_SPEED) : 0) };
  #endif
  #if HAS_FAN1
    for (uint8_t f = 1; f < FAN_COUNT && s->fan_n < 3; ++f)
      s->fan[s->fan_n++] = { ARQ_FAN_AUX, f, (uint8_t)thermalManager.fanSpeedPercent(f) };
  #endif

  s->speed_mms = arq_speed_cur_mms(&speed);
  s->peak_mms  = arq_speed_peak_mms(&speed);
  s->z_cmm = (int32_t)(current_position.z * 100.0f + (current_position.z >= 0 ? 0.5f : -0.5f));

  // --- Estado de impresion (funciones reales de Marlin) ---
  const bool paused = printingIsPaused(), printing = !paused && printingIsActive();
  s->job = paused || printing;

  #if HAS_MEDIA
    if (s->job && card.isFileOpen()) {
      s->sd_job = true;
      arq_strlcpy(s->file, card.longest_filename(), sizeof(s->file));
      s->pct = job_percent();
    }
  #endif
  if (s->job) {
    s->elapsed_s = print_job_timer.duration(); // segundos (Stopwatch::duration devuelve millis_t en s)
    if (s->sd_job) s->remaining_s = arq_remaining_s(s->elapsed_s, s->pct);
  }

  // --- Estado global ---
  int16_t cur[2], tgt[2], tol[2]; uint8_t n = 0;
  if (s->hot.present) { cur[n] = s->hot.cur; tgt[n] = s->hot.tgt; tol[n] = s->hot.tol; ++n; }
  if (s->bed.present) { cur[n] = s->bed.cur; tgt[n] = s->bed.tgt; tol[n] = s->bed.tol; ++n; }
  bool card_ready = true;      // sin soporte de tarjeta no hay estado "Reposo"
  #if HAS_MEDIA
    card_ready = card.isMounted();
  #endif
  s->state = arq_with_standby(arq_printer_state(IsStopped() || marlin_state == MF_KILLED, paused, printing, n, cur, tgt, tol), card_ready);
}

#endif // HAS_TFT_LVGL_UI && ARQUIS_UI
