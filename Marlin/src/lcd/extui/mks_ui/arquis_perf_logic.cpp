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

#include "arquis_perf_logic.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static inline bool finite_f(float v) { return v == v && v - v == 0; }

#define ACC_UNIT "mm/s\xC2\xB2"     // mm/s2 (el superindice 2 esta en la fuente Latin-1 de la interfaz)

// Limites: Marlin acepta M204/M201 sin tope (el techo real de la maquina lo pone el usuario con la torre de ringing);
//   M900: K 0..10 (aqui 0..2, de sobra para accionamiento directo y Bowden)
//   M205 J: 0.01..0.3 (gcode/config/M200-M205.cpp)
//   M593 F: 0 (apagado) o por encima de la frecuencia minima del buffer compilado (stepper.h: shaping_min_freq; por debajo desborda)
arq_pf_param_t arq_pf_param(uint8_t id, float shaping_min) {
  arq_pf_param_t p; memset(&p, 0, sizeof p);
  switch (id) {
    case ARQ_PF_ACC_P:  p.label = "Acel. impresi\xC3\xB3n"; p.unit = ACC_UNIT; p.min = 200;  p.max = 20000; p.fine = 100;   p.coarse = 500;  p.dec = 0; break;
    case ARQ_PF_ACC_T:  p.label = "Acel. vac\xC3\xADo";      p.unit = ACC_UNIT; p.min = 200;  p.max = 20000; p.fine = 100;   p.coarse = 500;  p.dec = 0; break;
    case ARQ_PF_AMAX_X: p.label = "Acel. m\xC3\xA1x X";     p.unit = ACC_UNIT; p.min = 500;  p.max = 20000; p.fine = 250;   p.coarse = 1000; p.dec = 0; break;
    case ARQ_PF_AMAX_Y: p.label = "Acel. m\xC3\xA1x Y";     p.unit = ACC_UNIT; p.min = 500;  p.max = 20000; p.fine = 250;   p.coarse = 1000; p.dec = 0; break;
    case ARQ_PF_K:      p.label = "Linear Advance K";       p.unit = "mm";     p.min = 0.005f; p.max = 2.0f; p.fine = 0.005f; p.coarse = 0.05f; p.dec = 3; p.zero_off = true; break;
    case ARQ_PF_JD:     p.label = "Junction Deviation";     p.unit = "mm";     p.min = 0.01f;  p.max = 0.3f; p.fine = 0.005f; p.coarse = 0.02f; p.dec = 3; break;
    case ARQ_PF_FX:     p.label = "Shaping X";              p.unit = "Hz";     p.min = shaping_min; p.max = 200; p.fine = 0.5f; p.coarse = 5; p.dec = 1; p.zero_off = true; break;
    case ARQ_PF_FY:     p.label = "Shaping Y";              p.unit = "Hz";     p.min = shaping_min; p.max = 200; p.fine = 0.5f; p.coarse = 5; p.dec = 1; p.zero_off = true; break;
    default: break;
  }
  if (p.max < p.min) p.max = p.min;
  return p;
}

static float round_dec(float v, uint8_t dec) {
  float s = 1; for (uint8_t i = 0; i < dec; ++i) s *= 10;
  return floorf(v * s + 0.5f) / s;
}

float arq_pf_clamp(uint8_t id, float v, float shaping_min) {
  if (id >= ARQ_PF_COUNT) return 0;
  const arq_pf_param_t p = arq_pf_param(id, shaping_min);
  if (!finite_f(v)) return p.zero_off ? 0 : p.min;
  if (p.zero_off && round_dec(v, p.dec) <= 0) return 0;
  v = round_dec(v, p.dec);
  return v < p.min ? p.min : v > p.max ? p.max : v;
}

float arq_pf_step(uint8_t id, float v, int8_t dir, bool coarse, float shaping_min) {
  if (id >= ARQ_PF_COUNT || dir == 0) return arq_pf_clamp(id, v, shaping_min);
  const arq_pf_param_t p = arq_pf_param(id, shaping_min);
  v = arq_pf_clamp(id, v, shaping_min);
  const float st = coarse ? p.coarse : p.fine;
  if (p.zero_off) {
    if (dir > 0) return v <= 0 ? p.min : arq_pf_clamp(id, v + st, shaping_min);
    if (v <= 0) return 0;
    const float n = round_dec(v - st, p.dec);
    return n < p.min ? 0 : arq_pf_clamp(id, n, shaping_min);
  }
  return arq_pf_clamp(id, v + (dir > 0 ? st : -st), shaping_min);
}

// Numero con 'dec' decimales sin printf de coma flotante. -1 si no cabe o no es representable.
static int num_str(char *buf, size_t n, float v, uint8_t dec) {
  long scale = 1; for (uint8_t i = 0; i < dec; ++i) scale *= 10;
  const double sc = (double)v * scale;
  if (!n || sc > 2.0e9 || sc < 0) return -1;
  const long h = (long)(sc + 0.5);
  return dec ? snprintf(buf, n, "%ld.%0*ld", h / scale, (int)dec, h % scale) : snprintf(buf, n, "%ld", h);
}

size_t arq_pf_fmt_value(char *buf, size_t n, uint8_t id, float v) {
  if (!n) return 0;
  if (id >= ARQ_PF_COUNT || !finite_f(v)) return (size_t)snprintf(buf, n, "--");
  const arq_pf_param_t p = arq_pf_param(id, 0);
  if (p.zero_off && round_dec(v, p.dec) <= 0) return (size_t)snprintf(buf, n, "OFF");
  return num_str(buf, n, v, p.dec) < 0 ? (size_t)snprintf(buf, n, "--") : strlen(buf);
}

size_t arq_pf_fmt_cmd(char *buf, size_t n, uint8_t id, float v, float shaping_min) {
  if (!n || id >= ARQ_PF_COUNT || !finite_f(v)) return 0;
  const arq_pf_param_t p = arq_pf_param(id, shaping_min);
  const bool off = p.zero_off && v == 0;
  if (!off && (v < p.min - 1e-4f || v > p.max + 1e-4f)) return 0;      // nunca se envia un valor fuera de limites
  char num[24];
  if (num_str(num, sizeof num, v, p.dec) < 0) return 0;
  static const char *const head[ARQ_PF_COUNT] = { "M204 P", "M204 T", "M201 X", "M201 Y", "M900 K", "M205 J", "M593 X F", "M593 Y F" };
  const int w = snprintf(buf, n, "%s%s", head[id], num);
  return (w > 0 && (size_t)w < n) ? (size_t)w : 0;
}

float arq_pf_effective_acc(float acc, float amax_x, float amax_y) {
  float m = amax_x < amax_y ? amax_x : amax_y;
  return acc < m ? acc : m;
}

uint8_t arq_pf_hint(uint8_t id, float v, float amax_x, float amax_y, float ref_value) {
  switch (id) {
    case ARQ_PF_ACC_P: case ARQ_PF_ACC_T:
      if (v > arq_pf_effective_acc(v, amax_x, amax_y) + 0.5f) return ARQ_PF_HINT_CAPPED;       // Marlin la recorta a la maxima del eje
      if (ref_value > 0 && v > ref_value * 1.5f) return ARQ_PF_HINT_HIGH_ACC;
      return ARQ_PF_HINT_NONE;
    case ARQ_PF_AMAX_X: case ARQ_PF_AMAX_Y:
      return (ref_value > 0 && v > ref_value * 1.5f) ? ARQ_PF_HINT_HIGH_ACC : ARQ_PF_HINT_NONE;
    case ARQ_PF_K:  return v <= 0 ? ARQ_PF_HINT_K_ZERO : ARQ_PF_HINT_NONE;
    case ARQ_PF_FX: case ARQ_PF_FY: return v <= 0 ? ARQ_PF_HINT_NO_SHAPING : ARQ_PF_HINT_NONE;
    default: return ARQ_PF_HINT_NONE;
  }
}

bool arq_pf_equal(uint8_t id, float a, float b) {
  if (id >= ARQ_PF_COUNT) return false;
  const arq_pf_param_t p = arq_pf_param(id, 0);
  return round_dec(a, p.dec) == round_dec(b, p.dec);
}

const char * arq_pf_err_text(uint8_t e) {
  static const char *const t[ARQ_PF_E_COUNT] = {
    "Correcto",
    "No disponible en este firmware",
    "Valor fuera de l\xC3\xADmites",
    "Hay una impresi\xC3\xB3n en curso",
    "La m\xC3\xA1quina est\xC3\xA1 ocupada",
    "Marlin est\xC3\xA1 detenido",
    "Marlin no acept\xC3\xB3 el valor",
    "Error al guardar en la EEPROM"
  };
  return e < ARQ_PF_E_COUNT ? t[e] : t[ARQ_PF_E_PARAM];
}

const char * arq_pf_hint_text(uint8_t hint) {
  switch (hint) {
    case ARQ_PF_HINT_CAPPED:     return "Limitada por la acel. m\xC3\xA1x. de X e Y";
    case ARQ_PF_HINT_HIGH_ACC:   return "Mucho m\xC3\xA1s alta que la guardada: sube poco a poco";
    case ARQ_PF_HINT_NO_SHAPING: return "Input Shaping apagado en este eje";
    case ARQ_PF_HINT_K_ZERO:     return "Linear Advance apagado";
    default: return nullptr;
  }
}
