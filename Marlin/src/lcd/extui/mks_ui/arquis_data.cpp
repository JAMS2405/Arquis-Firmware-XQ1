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

#include "../../../inc/MarlinConfig.h"
#include "../../../MarlinCore.h"
#include "../../../module/temperature.h"
#include "../../../module/printcounter.h"
#include "../../../sd/cardreader.h"

#include <string.h>

// Marlin solo define estas histeresis si el sensor existe; valor por defecto de Marlin si no.
#ifndef TEMP_HYSTERESIS
  #define TEMP_HYSTERESIS 3
#endif
#ifndef TEMP_BED_HYSTERESIS
  #define TEMP_BED_HYSTERESIS 3
#endif

bool arq_data_job_active() { return printingIsActive() || printingIsPaused(); }

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

  // --- Estado de impresion (funciones reales de Marlin) ---
  const bool paused = printingIsPaused(), printing = !paused && printingIsActive();
  s->job = paused || printing;

  #if HAS_MEDIA
    if (s->job && card.isFileOpen()) {
      s->sd_job = true;
      arq_strlcpy(s->file, card.longest_filename(), sizeof(s->file));
      s->pct = card.percentDone();
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
  s->state = arq_printer_state(IsStopped() || marlin_state == MF_KILLED, paused, printing, n, cur, tgt, tol);
}

#endif // HAS_TFT_LVGL_UI && ARQUIS_UI
