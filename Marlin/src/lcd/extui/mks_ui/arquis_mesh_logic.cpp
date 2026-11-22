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

#include "arquis_mesh_logic.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

const float arq_mesh_step_mm[ARQ_MESH_STEPS] = { 1.00f, 0.10f, 0.05f, 0.01f };

static inline bool finite_f(float v) { return v == v && v - v == 0; }     // ni NaN ni infinito (tambien con -ffast-math no se usa isfinite)

// ---- Orden y coordenadas ---------------------------------------------------------------------------------------------------------
uint8_t arq_mesh_seq_to_p(uint8_t seq) {
  if (seq >= ARQ_MESH_PTS) return 0xFF;
  const uint8_t row = seq / ARQ_MESH_N, col = seq % ARQ_MESH_N;
  return row * ARQ_MESH_N + ((row & 1) ? (ARQ_MESH_N - 1 - col) : col);
}

uint8_t arq_mesh_p_to_seq(uint8_t p) {
  if (p >= ARQ_MESH_PTS) return 0xFF;
  const uint8_t row = p / ARQ_MESH_N, col = p % ARQ_MESH_N;
  return row * ARQ_MESH_N + ((row & 1) ? (ARQ_MESH_N - 1 - col) : col);
}

bool arq_mesh_area_ok(const arq_mesh_area_t *a) {
  return a && finite_f(a->x_min) && finite_f(a->x_max) && finite_f(a->y_min) && finite_f(a->y_max)
           && a->x_max - a->x_min > 1.0f * (ARQ_MESH_N - 1) && a->y_max - a->y_min > 1.0f * (ARQ_MESH_N - 1);
}
float arq_mesh_spacing_x(const arq_mesh_area_t *a) { return (a->x_max - a->x_min) / (ARQ_MESH_N - 1); }
float arq_mesh_spacing_y(const arq_mesh_area_t *a) { return (a->y_max - a->y_min) / (ARQ_MESH_N - 1); }

void arq_mesh_point_xy(const arq_mesh_area_t *a, uint8_t p, float *x, float *y) {
  if (p >= ARQ_MESH_PTS) p = 0;
  if (x) *x = a->x_min + arq_mesh_ix(p) * arq_mesh_spacing_x(a);
  if (y) *y = a->y_min + arq_mesh_iy(p) * arq_mesh_spacing_y(a);
}

// ---- Movimiento de Z con proteccion ----------------------------------------------------------------------------------------------
float arq_mesh_step_z(float z, int8_t dir, float step_mm) {
  if (!finite_f(z) || !finite_f(step_mm) || step_mm <= 0 || dir == 0) return z;
  float n = z + (dir > 0 ? step_mm : -step_mm);
  if (dir < 0) {
    // Pasos gruesos (> 0,10 mm): no pueden acercar la boquilla por debajo del suelo (cerca del papel solo pasos finos).
    if (step_mm > 0.10f + 0.001f && n < ARQ_MESH_COARSE_FLOOR) n = (z > ARQ_MESH_COARSE_FLOOR) ? ARQ_MESH_COARSE_FLOOR : z;
  }
  if (n < ARQ_MESH_Z_MIN) n = ARQ_MESH_Z_MIN;
  if (n > ARQ_MESH_Z_MAX) n = ARQ_MESH_Z_MAX;
  // Redondeo a 0,01 mm (evita que se acumulen errores de coma flotante tras muchos pulsos).
  n = floorf(n * 100.0f + 0.5f) / 100.0f;
  return n;
}

float arq_mesh_start_z(bool have_last, float last_z, float start_z) {
  float z = start_z;
  if (have_last && finite_f(last_z)) { const float c = last_z + ARQ_MESH_NEXT_MARGIN; if (c < z) z = c; }
  if (z < ARQ_MESH_Z_MIN) z = ARQ_MESH_Z_MIN;
  if (z > ARQ_MESH_Z_MAX) z = ARQ_MESH_Z_MAX;
  return z;
}

// ---- Textos de error -------------------------------------------------------------------------------------------------------------
const char * arq_mesh_err_text(uint8_t err) {
  static const char *const t[ARQ_MESH_E_COUNT] = {
    "Correcto",
    "Operaci\xC3\xB3n no v\xC3\xA1lida ahora",
    "Z fuera de l\xC3\xADmites",
    "Faltan puntos por medir",
    "Desviaci\xC3\xB3n excesiva (> 2 mm)",
    "Valor no num\xC3\xA9rico",
    "No hay malla en memoria",
    "La malla no es la de 5x5 esperada",
    "Punto no v\xC3\xA1lido o sin medir",
    "Hay una impresi\xC3\xB3n en curso",
    "Falta hacer Home",
    "Temperatura de cama fuera de l\xC3\xADmites",
    "G29 manual en curso (env\xC3\xAD" "a G29 A)",
    "Error de EEPROM",
    "Marlin no activ\xC3\xB3 la malla",
    "La Z real no coincide: revisa y confirma de nuevo"
  };
  return err < ARQ_MESH_E_COUNT ? t[err] : t[ARQ_MESH_E_STATE];
}

// ---- Sesion ----------------------------------------------------------------------------------------------------------------------
void arq_mesh_sess_reset(arq_mesh_sess_t *s) {
  memset(s, 0, sizeof(*s));
  s->st = ARQ_MESH_IDLE;
  s->step_idx = 1;                       // 0,10 mm por defecto
  for (uint8_t i = 0; i < ARQ_MESH_PTS; ++i) s->z[i] = 0.0f;
}

bool arq_mesh_is_active(const arq_mesh_sess_t *s) {
  return s->st == ARQ_MESH_PREP || s->st == ARQ_MESH_MOVING || s->st == ARQ_MESH_ADJUST || s->st == ARQ_MESH_REVIEW;
}

bool arq_mesh_begin(arq_mesh_sess_t *s) {
  if (arq_mesh_is_active(s)) return false;
  const uint8_t step = s->step_idx < ARQ_MESH_STEPS ? s->step_idx : 1;   // el paso elegido se conserva entre sesiones
  arq_mesh_sess_reset(s);
  s->step_idx = step;
  s->st = ARQ_MESH_PREP;
  return true;
}

uint8_t arq_mesh_count(const arq_mesh_sess_t *s) {
  uint8_t n = 0;
  for (uint8_t p = 0; p < ARQ_MESH_PTS; ++p) if (s->done & (1UL << p)) ++n;
  return n;
}
uint8_t arq_mesh_percent(const arq_mesh_sess_t *s) { return (uint8_t)(arq_mesh_count(s) * 100 / ARQ_MESH_PTS); }

int8_t arq_mesh_next_pending(const arq_mesh_sess_t *s) {
  for (uint8_t q = 0; q < ARQ_MESH_PTS; ++q) {
    const uint8_t p = arq_mesh_seq_to_p(q);
    if (!(s->done & (1UL << p))) return (int8_t)p;
  }
  return -1;
}

bool arq_mesh_prep_done(arq_mesh_sess_t *s) {
  if (s->st != ARQ_MESH_PREP) return false;
  const int8_t p = arq_mesh_next_pending(s);
  if (p < 0) return false;
  s->cur = (uint8_t)p;
  s->st = ARQ_MESH_MOVING;
  return true;
}

bool arq_mesh_arrived(arq_mesh_sess_t *s, float start_z) {
  if (s->st != ARQ_MESH_MOVING || !finite_f(start_z)) return false;
  s->z_now = start_z < ARQ_MESH_Z_MIN ? ARQ_MESH_Z_MIN : start_z > ARQ_MESH_Z_MAX ? ARQ_MESH_Z_MAX : start_z;
  s->st = ARQ_MESH_ADJUST;
  return true;
}

bool arq_mesh_select_step(arq_mesh_sess_t *s, uint8_t idx) {
  if (idx >= ARQ_MESH_STEPS) return false;
  s->step_idx = idx;
  return true;
}

bool arq_mesh_jog(arq_mesh_sess_t *s, int8_t dir) {
  if (s->st != ARQ_MESH_ADJUST) return false;
  const float n = arq_mesh_step_z(s->z_now, dir, arq_mesh_step_mm[s->step_idx < ARQ_MESH_STEPS ? s->step_idx : 1]);
  if (n == s->z_now) return false;
  s->z_now = n;
  return true;
}

uint8_t arq_mesh_confirm(arq_mesh_sess_t *s, float z_abs) {
  if (s->st != ARQ_MESH_ADJUST) return ARQ_MESH_E_STATE;
  if (s->cur >= ARQ_MESH_PTS) return ARQ_MESH_E_BAD_POINT;
  if (!finite_f(z_abs)) return ARQ_MESH_E_NAN;
  if (z_abs < ARQ_MESH_Z_MIN - 0.005f || z_abs > ARQ_MESH_Z_MAX + 0.005f) return ARQ_MESH_E_Z_RANGE;
  s->z[s->cur] = z_abs;
  s->done |= 1UL << s->cur;
  s->last_z = z_abs;
  s->have_last = true;
  s->redo = 0;
  const int8_t nx = arq_mesh_next_pending(s);
  if (nx < 0) { s->st = ARQ_MESH_REVIEW; }
  else { s->cur = (uint8_t)nx; s->st = ARQ_MESH_MOVING; }
  return ARQ_MESH_OK;
}

uint8_t arq_mesh_repeat(arq_mesh_sess_t *s, uint8_t p) {
  if (s->st != ARQ_MESH_ADJUST && s->st != ARQ_MESH_REVIEW) return ARQ_MESH_E_STATE;
  if (p >= ARQ_MESH_PTS || !(s->done & (1UL << p))) return ARQ_MESH_E_BAD_POINT;
  s->done &= ~(1UL << p);            // vuelve a estar PENDIENTE: no cuenta hasta que el usuario lo confirme de nuevo
  s->z[p] = 0.0f;
  s->cur = p;
  s->redo = 1;
  s->st = ARQ_MESH_MOVING;
  return ARQ_MESH_OK;
}

void arq_mesh_cancel(arq_mesh_sess_t *s) {
  if (!arq_mesh_is_active(s)) return;
  const uint8_t step = s->step_idx;
  arq_mesh_sess_reset(s);
  s->step_idx = step;
  s->st = ARQ_MESH_ABORTED;
}

void arq_mesh_fail(arq_mesh_sess_t *s, uint8_t err) {
  const uint8_t step = s->step_idx;
  arq_mesh_sess_reset(s);
  s->step_idx = step;
  s->st = ARQ_MESH_ERROR;
  s->err = err ? err : (uint8_t)ARQ_MESH_E_STATE;
}

bool arq_mesh_finish(arq_mesh_sess_t *s) {
  if (s->st != ARQ_MESH_REVIEW) return false;
  s->st = ARQ_MESH_DONE;
  return true;
}

// ---- Calculo y validacion --------------------------------------------------------------------------------------------------------
uint8_t arq_mesh_corrections(const arq_mesh_sess_t *s, float out[ARQ_MESH_PTS]) {
  for (uint8_t p = 0; p < ARQ_MESH_PTS; ++p) {
    if (!(s->done & (1UL << p))) return ARQ_MESH_E_INCOMPLETE;
    if (!finite_f(s->z[p])) return ARQ_MESH_E_NAN;
  }
  const float ref = s->z[ARQ_MESH_CENTER];
  float tmp[ARQ_MESH_PTS];
  for (uint8_t p = 0; p < ARQ_MESH_PTS; ++p) {
    tmp[p] = s->z[p] - ref;
    if (fabsf(tmp[p]) > ARQ_MESH_MAX_DEV) return ARQ_MESH_E_RANGE;
  }
  tmp[ARQ_MESH_CENTER] = 0.0f;
  memcpy(out, tmp, sizeof(tmp));
  return ARQ_MESH_OK;
}

void arq_mesh_stats(const float v[ARQ_MESH_PTS], uint32_t mask, arq_mesh_stats_t *o) {
  memset(o, 0, sizeof(*o));
  double sum = 0;
  bool first = true;
  for (uint8_t p = 0; p < ARQ_MESH_PTS; ++p) {
    if (!(mask & (1UL << p)) || !finite_f(v[p])) continue;
    if (first || v[p] < o->min) { o->min = v[p]; o->imin = p; }
    if (first || v[p] > o->max) { o->max = v[p]; o->imax = p; }
    first = false;
    sum += v[p];
    ++o->n;
  }
  if (!o->n) return;
  o->mean = (float)(sum / o->n);
  o->range = o->max - o->min;
  double acc = 0;
  for (uint8_t p = 0; p < ARQ_MESH_PTS; ++p) {
    if (!(mask & (1UL << p)) || !finite_f(v[p])) continue;
    const double d = v[p] - o->mean;
    acc += d * d;
  }
  o->sigma = (float)sqrt(acc / o->n);
}

uint8_t arq_mesh_check_stored(const float z[ARQ_MESH_PTS], float start_x, float start_y, float sp_x, float sp_y, const arq_mesh_area_t *expected) {
  if (!finite_f(sp_x) || !finite_f(sp_y) || sp_x <= 0.0f || sp_y <= 0.0f) return ARQ_MESH_E_NO_MESH;     // rejilla sin definir
  if (!arq_mesh_area_ok(expected)) return ARQ_MESH_E_LAYOUT;
  if (!finite_f(start_x) || !finite_f(start_y)
      || fabsf(start_x - expected->x_min) > ARQ_MESH_LAYOUT_TOL || fabsf(start_y - expected->y_min) > ARQ_MESH_LAYOUT_TOL
      || fabsf(sp_x - arq_mesh_spacing_x(expected)) > ARQ_MESH_LAYOUT_TOL || fabsf(sp_y - arq_mesh_spacing_y(expected)) > ARQ_MESH_LAYOUT_TOL)
    return ARQ_MESH_E_LAYOUT;
  for (uint8_t p = 0; p < ARQ_MESH_PTS; ++p) {
    if (!finite_f(z[p])) return ARQ_MESH_E_NAN;
    if (fabsf(z[p]) > ARQ_MESH_MAX_DEV) return ARQ_MESH_E_RANGE;
  }
  return ARQ_MESH_OK;
}

float arq_mesh_norm(float v, float limit) {
  if (!finite_f(v) || !finite_f(limit) || limit <= 0.0f) return 0.0f;
  float t = v / limit;
  return t < -1.0f ? -1.0f : t > 1.0f ? 1.0f : t;
}

// ---- Textos ----------------------------------------------------------------------------------------------------------------------
static int32_t to_hundredths(float v) {
  if (v > 99999.0f) v = 99999.0f;
  if (v < -99999.0f) v = -99999.0f;
  return (int32_t)(v * 100.0f + (v < 0 ? -0.5f : 0.5f));
}

size_t arq_mesh_fmt_dev(char *buf, size_t n, float v) {
  if (!n) return 0;
  if (!finite_f(v)) return (size_t)snprintf(buf, n, "--");
  const int32_t h = to_hundredths(v);
  if (!h) return (size_t)snprintf(buf, n, "0.00");
  const int32_t a = h < 0 ? -h : h;
  return (size_t)snprintf(buf, n, "%c%ld.%02ld", h < 0 ? '-' : '+', (long)(a / 100), (long)(a % 100));
}

size_t arq_mesh_fmt_z(char *buf, size_t n, float v) {
  if (!n) return 0;
  if (!finite_f(v)) return (size_t)snprintf(buf, n, "--");
  const int32_t h = to_hundredths(v);
  const int32_t a = h < 0 ? -h : h;
  return (size_t)snprintf(buf, n, "%s%ld.%02ld", h < 0 ? "-" : "", (long)(a / 100), (long)(a % 100));
}
