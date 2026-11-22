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

#include "arquis_pid_logic.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static inline bool finite_f(float v) { return v == v && v - v == 0; }

arq_pid_limits_t arq_pid_limits(uint8_t heater, int16_t max_target) {
  arq_pid_limits_t l;
  if (heater == ARQ_PID_BED) { l.min = 40; l.def = 60; l.cyc_def = 6; }
  else                       { l.min = 150; l.def = 210; l.cyc_def = 8; }
  l.max = max_target;
  if (l.max < l.min) l.max = l.min;
  if (l.def > l.max) l.def = l.max;
  return l;
}

int16_t arq_pid_clamp_target(uint8_t heater, int16_t t, int16_t max_target) {
  const arq_pid_limits_t l = arq_pid_limits(heater, max_target);
  return t < l.min ? l.min : t > l.max ? l.max : t;
}

int8_t arq_pid_clamp_cycles(int c) { return (int8_t)(c < ARQ_PID_CYC_MIN ? ARQ_PID_CYC_MIN : c > ARQ_PID_CYC_MAX ? ARQ_PID_CYC_MAX : c); }

const char * arq_pid_err_text(uint8_t e) {
  static const char *const t[ARQ_PID_E_COUNT] = {
    "Correcto",
    "Operaci\xC3\xB3n no v\xC3\xA1lida ahora",
    "Par\xC3\xA1metros fuera de l\xC3\xADmites",
    "Hay una impresi\xC3\xB3n en curso",
    "La m\xC3\xA1quina est\xC3\xA1 ocupada",
    "Temperatura demasiado alta",
    "Tiempo agotado: el calentador no sigue la temperatura",
    "Cancelado",
    "Valores calculados no plausibles",
    "Marlin no inici\xC3\xB3 el ajuste",
    "Marlin est\xC3\xA1 detenido",
    "Error al guardar en la EEPROM"
  };
  return e < ARQ_PID_E_COUNT ? t[e] : t[ARQ_PID_E_STATE];
}

void arq_pid_sess_reset(arq_pid_sess_t *s) { memset(s, 0, sizeof(*s)); s->st = ARQ_PID_IDLE; }
bool arq_pid_is_running(const arq_pid_sess_t *s) { return s->st == ARQ_PID_RUNNING; }

bool arq_pid_result_ok(float kp, float ki, float kd) {
  return finite_f(kp) && finite_f(ki) && finite_f(kd) && kp > 0.0f && kp < 1000.0f && ki >= 0.0001f && ki < 100.0f && kd >= 0.0f && kd < 10000.0f;
}

uint8_t arq_pid_begin(arq_pid_sess_t *s, uint8_t heater, int16_t target, int cycles, int16_t max_target, const float old[3]) {
  if (s->st == ARQ_PID_RUNNING) return ARQ_PID_E_STATE;
  if (heater >= ARQ_PID_HEATERS) return ARQ_PID_E_PARAM;
  const arq_pid_limits_t l = arq_pid_limits(heater, max_target);
  if (target < l.min || target > l.max || cycles < ARQ_PID_CYC_MIN || cycles > ARQ_PID_CYC_MAX) return ARQ_PID_E_PARAM;
  arq_pid_sess_reset(s);
  s->heater = heater; s->target = target; s->cycles = (uint8_t)cycles;
  if (old) memcpy(s->old_pid, old, sizeof(s->old_pid));
  s->st = ARQ_PID_RUNNING;
  return ARQ_PID_OK;
}

void arq_pid_progress(arq_pid_sess_t *s, uint8_t cycle) { if (s->st == ARQ_PID_RUNNING) s->cycle = cycle > s->cycles ? s->cycles : cycle; }

void arq_pid_fail(arq_pid_sess_t *s, uint8_t err) {
  const uint8_t h = s->heater, c = s->cycles; const int16_t t = s->target;
  arq_pid_sess_reset(s);
  s->heater = h; s->cycles = c; s->target = t;
  s->st = ARQ_PID_FAILED; s->err = err ? err : (uint8_t)ARQ_PID_E_STATE;
}

uint8_t arq_pid_finish_result(arq_pid_sess_t *s, uint8_t r, float kp, float ki, float kd) {
  if (s->st != ARQ_PID_RUNNING) return ARQ_PID_E_STATE;
  switch (r) {
    case ARQ_PID_R_DONE:
      if (!arq_pid_result_ok(kp, ki, kd)) { arq_pid_fail(s, ARQ_PID_E_BAD_RESULT); return ARQ_PID_E_BAD_RESULT; }
      s->new_pid[0] = kp; s->new_pid[1] = ki; s->new_pid[2] = kd; s->cycle = s->cycles; s->st = ARQ_PID_RESULT;
      return ARQ_PID_OK;
    case ARQ_PID_R_TOO_HIGH: arq_pid_fail(s, ARQ_PID_E_TOO_HIGH); return ARQ_PID_E_TOO_HIGH;
    case ARQ_PID_R_TIMEOUT:  arq_pid_fail(s, ARQ_PID_E_TIMEOUT);  return ARQ_PID_E_TIMEOUT;
    case ARQ_PID_R_CANCEL:   arq_pid_fail(s, ARQ_PID_E_CANCEL);   return ARQ_PID_E_CANCEL;
    default:                 arq_pid_fail(s, ARQ_PID_E_NOT_RUN);  return ARQ_PID_E_NOT_RUN;      // NONE o RUNNING: la orden acabo sin resultado
  }
}

void arq_pid_discard(arq_pid_sess_t *s) {
  if (s->st == ARQ_PID_RUNNING) return;
  const uint8_t h = s->heater, c = s->cycles; const int16_t t = s->target;
  arq_pid_sess_reset(s);
  s->heater = h; s->cycles = c; s->target = t;
}

// ---- Formatos (sin printf de coma flotante: la libc reducida de la placa no lo trae siempre) -------------------------------------
size_t arq_pid_fmt_num(char *buf, size_t n, float v, uint8_t dec) {
  if (!n) return 0;
  if (!finite_f(v)) return (size_t)snprintf(buf, n, "--");
  if (dec > 4) dec = 4;
  long scale = 1; for (uint8_t i = 0; i < dec; ++i) scale *= 10;
  const double scaled = (double)v * scale;
  if (scaled > 2.0e9 || scaled < -2.0e9) return (size_t)snprintf(buf, n, "--");
  const long h = (long)(scaled + (scaled < 0 ? -0.5 : 0.5));
  const long a = h < 0 ? -h : h;
  if (!dec) return (size_t)snprintf(buf, n, "%s%ld", h < 0 ? "-" : "", a);
  return (size_t)snprintf(buf, n, "%s%ld.%0*ld", h < 0 ? "-" : "", a / scale, (int)dec, a % scale);
}

size_t arq_pid_fmt_time(char *buf, size_t n, uint32_t s) { return n ? (size_t)snprintf(buf, n, "%lu:%02lu", (unsigned long)(s / 60), (unsigned long)(s % 60)) : 0; }

size_t arq_pid_fmt_start(char *buf, size_t n, uint8_t heater, int16_t target, int cycles) {
  const int w = snprintf(buf, n, "M303 E%d S%d C%d", heater == ARQ_PID_BED ? -1 : 0, (int)target, cycles);
  return (w > 0 && (size_t)w < n) ? (size_t)w : 0;
}

size_t arq_pid_fmt_apply(char *buf, size_t n, uint8_t heater, float kp, float ki, float kd) {
  if (!arq_pid_result_ok(kp, ki, kd)) return 0;
  char a[16], b[16], c[16];
  arq_pid_fmt_num(a, sizeof a, kp, 4); arq_pid_fmt_num(b, sizeof b, ki, 4); arq_pid_fmt_num(c, sizeof c, kd, 4);
  const int w = snprintf(buf, n, "%s P%s I%s D%s", heater == ARQ_PID_BED ? "M304" : "M301", a, b, c);
  return (w > 0 && (size_t)w < n) ? (size_t)w : 0;
}
