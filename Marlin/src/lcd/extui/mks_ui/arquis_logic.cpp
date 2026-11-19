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

#include "arquis_logic.h"

#include <stdio.h>

arq_temp_state_t arq_temp_state(int16_t cur, int16_t target, int16_t tol) {
  if (target <= 0) return cur > ARQ_SAFE_TEMP ? ARQ_TEMP_COOLING : ARQ_TEMP_OFF;
  if (cur >= target - tol && cur <= target + tol) return ARQ_TEMP_READY;
  return cur < target ? ARQ_TEMP_HEATING : ARQ_TEMP_COOLING;
}

arq_pstate_t arq_printer_state(bool stopped, bool paused, bool printing, uint8_t n, const int16_t *cur, const int16_t *tgt, const int16_t *tol) {
  if (stopped) return ARQ_PS_ERROR;
  if (paused) return ARQ_PS_PAUSED;
  if (printing) return ARQ_PS_PRINTING;
  bool heating = false, ready = false, cooling = false;
  for (uint8_t i = 0; i < n; ++i) {
    switch (arq_temp_state(cur[i], tgt[i], tol[i])) {
      case ARQ_TEMP_HEATING: heating = true; break;
      case ARQ_TEMP_READY:   ready = true;   break;
      case ARQ_TEMP_COOLING: cooling = true; break;
      default: break;
    }
  }
  if (heating) return ARQ_PS_HEATING;
  if (ready)   return ARQ_PS_AT_TEMP;
  if (cooling) return ARQ_PS_COOLING;
  return ARQ_PS_IDLE;
}

arq_pstate_t arq_with_standby(arq_pstate_t s, bool card_ready) { return (s == ARQ_PS_IDLE && !card_ready) ? ARQ_PS_STANDBY : s; }

uint32_t arq_remaining_s(uint32_t elapsed_s, uint8_t percent) {
  if (percent < ARQ_PROGRESS_MIN || percent >= 100 || elapsed_s < ARQ_ELAPSED_MIN) return 0;
  return (uint32_t)((uint64_t)elapsed_s * (100 - percent) / percent);
}

size_t arq_fmt_hm(char *buf, size_t n, uint32_t s) {
  const int w = snprintf(buf, n, "%u:%02u", (unsigned)(s / 3600), (unsigned)((s % 3600) / 60));
  return w < 0 ? 0 : (size_t)w;
}

size_t arq_strlcpy(char *dst, const char *src, size_t n) {
  size_t i = 0;
  if (n) { for (; i + 1 < n && src[i]; ++i) dst[i] = src[i]; dst[i] = '\0'; }
  while (src[i]) ++i;
  return i;
}

bool arq_finish_detect(arq_finish_state_t *st, const arq_finish_in_t *in) {
  if (!in->working || in->once) { st->armed = false; return false; }
  if (in->sd_done)              { st->armed = false; return true; }
  if (in->preparing || in->job_active || in->file_open) { st->armed = false; return false; }
  if (!st->armed) { st->armed = true; st->since_ms = in->now_ms; return false; }
  if ((uint32_t)(in->now_ms - st->since_ms) >= ARQ_FINISH_GONE_MS) { st->armed = false; return true; }
  return false;
}

bool arq_finish_new_job(bool file_open, bool sd_done) { return file_open && !sd_done; }

