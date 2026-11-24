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


#include "arquis_print_view.h"

#include <stdio.h>
#include <string.h>

// ---- Geometria ----------------------------------------------------------------------------------------------------------------
#define COL_X    210
#define COL_W    (ARQ_W - ARQ_MARGIN - COL_X)
#define JOB_Y    40
#define JOB_H    96
#define LIST_Y   (JOB_Y + JOB_H + ARQ_GAP)
#define LIST_H   (ARQ_LIST_PAD + ARQ_LIST_ROWS_MAX * ARQ_LIST_ROW_H_COMPACT)
#define BTN_Y    246
#define BTN_H    (ARQ_H - ARQ_MARGIN - BTN_Y)
#define HOME_W   100
#define CHIP_W   150

static arq_header_t   header;
static lv_obj_t      *chip, *home, *job, *list, *b_pause, *b_stop, *b_obj, *b_opt;
static bool           built, prev_valid;
static arq_pmode_t    prev_mode;
static arq_snapshot_t prev;
static char           file_txt[44], pct_txt[6], l1_txt[24], l2_txt[24];
static arq_list_row_t rows[ARQ_LIST_ROWS_MAX];

static const char *const state_text[] = { "Lista", "Calentando", "Temp. alcanzada", "Enfriando", "Imprimiendo", "Pausada", "Error", "Reposo" };
static const arq_tint_t state_tint[] = { ARQ_TINT_OK, ARQ_TINT_WARN, ARQ_TINT_OK, ARQ_TINT_ACCENT, ARQ_TINT_OK, ARQ_TINT_WARN, ARQ_TINT_DANGER, ARQ_TINT_MUTED };
static const arq_tint_t temp_dot[] = { ARQ_TINT_MUTED, ARQ_TINT_WARN, ARQ_TINT_OK, ARQ_TINT_ACCENT };   // apagado, calentando, objetivo, enfriando

void arq_print_forget() { built = prev_valid = false; chip = home = job = list = b_pause = b_stop = b_obj = b_opt = nullptr; header.bar = header.back = nullptr; }

void arq_print_build(lv_obj_t *scr, lv_event_cb_t cb, const char *machine_name, const arq_snapshot_t *first, arq_pmode_t mode, bool with_objects) {
  (void)first; (void)mode;
  arq_print_forget();
  header.bar = arq_card_create(scr, 0, 0, ARQ_W, ARQ_PRINT_HEADER_H, ARQ_LAYOUT_HEADER, ARQ_BTN_GHOST, nullptr, 0);
  arq_card_set_title(header.bar, machine_name, ARQ_TEXT_BODY);
  arq_card_set_logo(header.bar, true);
  chip = arq_chip_create(scr, ARQ_W - ARQ_MARGIN - HOME_W - ARQ_GAP - CHIP_W, 4, CHIP_W, state_text[0], state_tint[0]);
  home = arq_button_create(scr, ARQ_W - ARQ_MARGIN - HOME_W, 0, HOME_W, ARQ_PRINT_HEADER_H, nullptr, "Inicio", ARQ_BTN_SECONDARY, cb, ARQ_PID_HOME);

  job  = arq_card_create(scr, COL_X, JOB_Y, COL_W, JOB_H, ARQ_LAYOUT_JOB, ARQ_BTN_SECONDARY, nullptr, 0);
  arq_card_set_title(job, file_txt, ARQ_TEXT_BODY);
  arq_card_set_value(job, pct_txt, ARQ_TEXT_NUM_L);
  arq_card_set_value2(job, l1_txt);
  arq_card_set_sub(job, l2_txt, ARQ_TEXT_MUTED);
  list = arq_card_create(scr, COL_X, LIST_Y, COL_W, LIST_H, ARQ_LAYOUT_LIST, ARQ_BTN_SECONDARY, nullptr, 0);
  arq_card_set_compact(list, true);                                   // 5 filas compactas: caben hotend, cama, ventilador, velocidad y Z

  // Botones de accion (fila inferior, a todo el ancho, como los accesos del Dashboard: icono arriba y texto debajo)
  const uint8_t n = with_objects ? 4 : 3;
  const lv_coord_t w = (ARQ_W - 2 * ARQ_MARGIN - (n - 1) * ARQ_GAP) / n;
  lv_coord_t x = ARQ_MARGIN;
  b_pause = arq_nav_tile_create(scr, x, BTN_Y, w, BTN_H, &arq_icon_pause, ARQ_TINT_AUTO, "Pausar", ARQ_BTN_PRIMARY, cb, ARQ_PID_PAUSE); x += w + ARQ_GAP;
  b_stop  = arq_nav_tile_create(scr, x, BTN_Y, w, BTN_H, &arq_icon_close, ARQ_TINT_DANGER, "Cancelar", ARQ_BTN_SECONDARY, cb, ARQ_PID_STOP); x += w + ARQ_GAP;   // rojo = accion peligrosa
  b_obj = nullptr;
  if (with_objects) { b_obj = arq_nav_tile_create(scr, x, BTN_Y, w, BTN_H, &arq_icon_objects, ARQ_TINT_ACCENT, "Objetos", ARQ_BTN_SECONDARY, cb, ARQ_PID_OBJECTS); x += w + ARQ_GAP; }
  b_opt   = arq_nav_tile_create(scr, x, BTN_Y, w, BTN_H, &arq_icon_sliders, ARQ_TINT_ACCENT, "Opciones", ARQ_BTN_SECONDARY, cb, ARQ_PID_OPTIONS);
  built = true; prev_valid = false;
}

static void set_row(arq_list_row_t &r, const char *label, const char *v1, const char *v2, arq_tint_t dot) {
  r.label = label; arq_strlcpy(r.v1, v1, sizeof(r.v1)); arq_strlcpy(r.v2, v2, sizeof(r.v2)); r.dot = dot;
}

static void temp_row(arq_list_row_t &r, const char *label, const arq_heater_t &h) {
  char v1[10], v2[10] = "";
  snprintf(v1, sizeof(v1), "%d\xC2\xB0", (int)h.cur);
  if (h.tgt > 0) snprintf(v2, sizeof(v2), "/ %d\xC2\xB0", (int)h.tgt);
  set_row(r, label, v1, v2, temp_dot[arq_temp_state(h.cur, h.tgt, h.tol)]);
}

void arq_print_update(const arq_snapshot_t *s, arq_pmode_t mode) {
  if (!built || (prev_valid && prev_mode == mode && arq_snapshot_equal(&prev, s))) return;

  // ---- Estado (cabecera) ----
  const char *st = state_text[s->state]; arq_tint_t st_tint = state_tint[s->state];
  if (s->state != ARQ_PS_ERROR) {
    switch (mode) {
      case ARQ_PM_PREPARING: st = "Preparando";  st_tint = ARQ_TINT_WARN; break;
      case ARQ_PM_PAUSING:   st = "Pausando";    st_tint = ARQ_TINT_WARN; break;
      case ARQ_PM_RESUMING:  st = "Reanudando";  st_tint = ARQ_TINT_WARN; break;
      case ARQ_PM_RECOVER:   st = "Interrumpida"; st_tint = ARQ_TINT_WARN; break;
      default: break;
    }
  }
  arq_card_set_title(chip, st, ARQ_TEXT_BODY);
  arq_card_set_dot(chip, st_tint);

  // ---- Avance ----
  const bool warn = mode == ARQ_PM_PAUSING || mode == ARQ_PM_PAUSED || mode == ARQ_PM_RECOVER || s->state == ARQ_PS_PAUSED;
  if (s->job && s->sd_job) {
    arq_strlcpy(file_txt, s->file, sizeof(file_txt));
    snprintf(pct_txt, sizeof(pct_txt), "%u%%", (unsigned)s->pct);
    char e[12], r[12];
    arq_fmt_hm(e, sizeof(e), s->elapsed_s);
    if (s->remaining_s) arq_fmt_hm(r, sizeof(r), s->remaining_s); else arq_strlcpy(r, "--:--", sizeof(r));
    snprintf(l1_txt, sizeof(l1_txt), "Tiempo  %s", e);
    snprintf(l2_txt, sizeof(l2_txt), "Resta  %s", r);
  }
  else {
    arq_strlcpy(file_txt, mode == ARQ_PM_PREPARING ? "Preparando la impresi\xC3\xB3n" : mode == ARQ_PM_RECOVER ? "Recuperar impresi\xC3\xB3n" : s->job ? "Impresi\xC3\xB3n en curso" : "Sin impresi\xC3\xB3n en curso", sizeof(file_txt));
    arq_strlcpy(pct_txt, "--%", sizeof(pct_txt));
    if (s->job) { char e[12]; arq_fmt_hm(e, sizeof(e), s->elapsed_s); snprintf(l1_txt, sizeof(l1_txt), "Tiempo  %s", e); }
    else l1_txt[0] = '\0';
    l2_txt[0] = '\0';
  }
  arq_card_set_percent(job, (s->job && s->sd_job) ? s->pct : 0);
  arq_card_set_bar_tint(job, warn ? ARQ_TINT_WARN : ARQ_TINT_OK);
  arq_card_set_value(job, pct_txt, warn ? ARQ_TEXT_NUM_L_WARN : ARQ_TEXT_NUM_L);
  arq_card_refresh(job);

  // ---- Lista de datos reales ----
  uint8_t n = 0;
  if (s->hot.present) temp_row(rows[n++], "Hotend", s->hot);
  if (s->bed.present) temp_row(rows[n++], "Cama", s->bed);
  if (s->fan_n && n < ARQ_LIST_ROWS_MAX) {
    char v[10]; snprintf(v, sizeof(v), "%u%%", (unsigned)s->fan[0].pct);
    set_row(rows[n++], "Ventilador", v, "", s->fan[0].pct ? ARQ_TINT_OK : ARQ_TINT_MUTED);
  }
  if (s->job && n < ARQ_LIST_ROWS_MAX) {                             // velocidad REAL del cabezal (XY) y maxima alcanzada en esta impresion
    // m[16]: cubre todo el rango de uint16_t ("max 65535" con a acentuada = 10 B + nulo). La logica (speed_u16) ya limita a 0..9999;
    // el tope de 9999 tambien aqui garantiza que el texto cabe completo en arq_list_row_t::v2[10] (9 B + nulo) aunque esa logica cambie.
    char v[16], m[16];
    snprintf(v, sizeof(v), "%u mm/s", (unsigned)s->speed_mms);
    snprintf(m, sizeof(m), "m\xC3\xA1x %u", (unsigned)(s->peak_mms > 9999 ? 9999 : s->peak_mms));
    set_row(rows[n++], "Veloc.", v, m, s->speed_mms ? ARQ_TINT_ACCENT : ARQ_TINT_MUTED);
  }
  if (n < ARQ_LIST_ROWS_MAX) {
    char v[16]; int32_t z = s->z_cmm < 0 ? -s->z_cmm : s->z_cmm;
    if (z > 99999) z = 99999;                                        // limite de la pantalla (999,99 mm): nunca desborda el texto
    snprintf(v, sizeof(v), "%s%ld.%02ld mm", s->z_cmm < 0 ? "-" : "", (long)(z / 100), (long)(z % 100));
    set_row(rows[n++], "Altura Z", v, "", ARQ_TINT_AUTO);
  }
  arq_card_set_rows(list, rows, n);

  // ---- Controles ----
  const bool prep = mode == ARQ_PM_PREPARING;
  const bool can_pause = mode == ARQ_PM_RUNNING, can_resume = mode == ARQ_PM_PAUSED || mode == ARQ_PM_RECOVER;
  const bool resume_look = can_resume || mode == ARQ_PM_RESUMING;
  arq_card_set_icon(b_pause, resume_look ? &arq_icon_play : &arq_icon_pause, ARQ_TINT_AUTO);
  arq_card_set_title(b_pause, mode == ARQ_PM_PAUSING ? "Pausando" : mode == ARQ_PM_RESUMING ? "Reanudando" : resume_look ? "Reanudar" : "Pausar", ARQ_TEXT_BODY);
  arq_card_set_disabled(b_pause, !(can_pause || can_resume));
  arq_card_set_disabled(b_stop, prep);
  arq_card_set_disabled(b_obj, prep);
  arq_card_set_disabled(b_opt, prep);
  arq_card_set_disabled(home, prep);

  prev = *s; prev_mode = mode; prev_valid = true;
}
