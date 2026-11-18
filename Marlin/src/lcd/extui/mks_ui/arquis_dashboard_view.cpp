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

#include "arquis_dashboard_view.h"

#include <stdio.h>
#include <string.h>

// ---- Geometria (480x320): cabecera 44 | fila 1: tarjetas de dato 124 | fila 2: accesos 124 ----------------------------
#define Y1   (ARQ_HEADER_H + 10)
#define H1   124
#define W1   148
#define Y2   (Y1 + H1 + ARQ_GAP)
#define H2   (ARQ_H - Y2 - ARQ_MARGIN)
#define PRINT_W (2 * 85 + ARQ_GAP)          // la tarjeta de impresion ocupa el sitio de dos accesos

static lv_event_cb_t user_cb;
static arq_header_t  header;
static lv_obj_t     *chip, *fans_card, *nav[5], *print_card;
static arq_temp_tile_t hot, bed;
static arq_fan_row_t fan_rows[ARQ_FAN_ROWS_MAX];
static bool          built, shown_job, prev_valid;
static arq_snapshot_t prev;
static char          pct_txt[6], times_txt[36];
static char          file_txt[44];

static const char *const state_text[] = { "Lista", "Calentando", "Temp. alcanzada", "Enfriando", "Imprimiendo", "Pausada", "Error" };
// Colores del punto: verde = correcto, ambar = calentando/pausa, azul = enfriando (proceso pasivo), rojo = error
static const arq_tint_t state_tint[] = { ARQ_TINT_OK, ARQ_TINT_WARN, ARQ_TINT_OK, ARQ_TINT_ACCENT, ARQ_TINT_OK, ARQ_TINT_WARN, ARQ_TINT_DANGER };
static const char *const aux_name[] = { "Aux", "Aux 1", "Aux 2", "Aux 3", "Aux 4" };

static void build_row2(bool job) {
  for (uint8_t i = 0; i < 5; ++i) if (nav[i]) { lv_obj_del(nav[i]); nav[i] = nullptr; }
  if (print_card) { lv_obj_del(print_card); print_card = nullptr; }
  lv_obj_t *scr = lv_obj_get_parent(header.bar);

  struct Item { const lv_img_dsc_t *icon; const char *text; uint16_t id; };
  static const Item idle_items[5] = { { &arq_icon_print, "Imprimir", ARQ_ID_PRINT }, { &arq_icon_move, "Mover", ARQ_ID_MOVE },
                                       { &arq_icon_temp, "Temp.", ARQ_ID_TEMP }, { &arq_icon_tools, "Herram.", ARQ_ID_TOOLS },
                                       { &arq_icon_gear, "Ajustes", ARQ_ID_SETTINGS } };
  // Con trabajo activo: tarjeta de impresion (acceso a la supervision) + Mover, Temp. y Ajustes.
  // Herramientas (precalentar, nivelar, cambiar filamento...) se oculta mientras se imprime.
  static const Item job_items[3] = { { &arq_icon_move, "Mover", ARQ_ID_MOVE }, { &arq_icon_temp, "Temp.", ARQ_ID_TEMP }, { &arq_icon_gear, "Ajustes", ARQ_ID_SETTINGS } };

  if (!job) {
    const lv_coord_t w = (ARQ_W - 2 * ARQ_MARGIN - 4 * ARQ_GAP) / 5;
    for (uint8_t i = 0; i < 5; ++i)
      nav[i] = arq_nav_tile_create(scr, ARQ_MARGIN + i * (w + ARQ_GAP), Y2, w, H2, idle_items[i].icon, ARQ_TINT_ACCENT, idle_items[i].text, ARQ_BTN_SECONDARY, user_cb, idle_items[i].id);
  }
  else {
    print_card = arq_card_create(scr, ARQ_MARGIN, Y2, PRINT_W, H2, ARQ_LAYOUT_PRINT, ARQ_BTN_SECONDARY, user_cb, ARQ_ID_PRINT);
    arq_card_set_title(print_card, file_txt, ARQ_TEXT_BODY);
    arq_card_set_value(print_card, pct_txt, ARQ_TEXT_NUM_L);
    arq_card_set_value2(print_card, times_txt);
    const lv_coord_t x0 = ARQ_MARGIN + PRINT_W + ARQ_GAP, w = (ARQ_W - ARQ_MARGIN - x0 - 2 * ARQ_GAP) / 3;
    for (uint8_t i = 0; i < 3; ++i)
      nav[i] = arq_nav_tile_create(scr, x0 + i * (w + ARQ_GAP), Y2, w, H2, job_items[i].icon, ARQ_TINT_ACCENT, job_items[i].text, ARQ_BTN_SECONDARY, user_cb, job_items[i].id);
  }
  shown_job = job;
}

void arq_dash_build(lv_obj_t *scr, lv_event_cb_t cb, const char *static_title, const arq_snapshot_t *first) {
  arq_dash_forget();
  user_cb = cb;
  arq_header_create(&header, scr, static_title, true, nullptr, 0);
  chip = arq_chip_create(scr, ARQ_W - ARQ_MARGIN - 160, 8, 160, state_text[0], state_tint[0]);

  lv_coord_t x = ARQ_MARGIN;
  if (first->hot.present) { arq_temp_tile_create(&hot, scr, x, Y1, W1, H1, &arq_icon_nozzle, "Hotend", user_cb, ARQ_ID_HOT); x += W1 + ARQ_GAP; }
  if (first->bed.present) { arq_temp_tile_create(&bed, scr, x, Y1, W1, H1, &arq_icon_bed, "Cama", user_cb, ARQ_ID_BED); x += W1 + ARQ_GAP; }
  fans_card = nullptr;
  if (first->fan_n) {
    fans_card = arq_card_create(scr, x, Y1, W1, H1, ARQ_LAYOUT_FANS, ARQ_BTN_SECONDARY, user_cb, ARQ_ID_FANS);
    arq_card_set_title(fans_card, "Ventiladores", ARQ_TEXT_BODY);
  }
  memset(nav, 0, sizeof(nav)); print_card = nullptr;
  built = true; prev_valid = false;
  build_row2(first->job);
}

void arq_dash_forget() { built = prev_valid = false; chip = fans_card = print_card = nullptr; memset(nav, 0, sizeof(nav)); header.bar = header.back = nullptr; hot.card = bed.card = nullptr; }

void arq_dash_update(const arq_snapshot_t *s) {
  if (!built || (prev_valid && arq_snapshot_equal(&prev, s))) return;
  if (s->job != shown_job) build_row2(s->job);

  arq_card_set_title(chip, state_text[s->state], ARQ_TEXT_BODY);
  arq_card_set_dot(chip, state_tint[s->state]);
  if (hot.card) arq_temp_tile_update(&hot, s->hot.cur, s->hot.tgt, s->hot.tol);
  if (bed.card) arq_temp_tile_update(&bed, s->bed.cur, s->bed.tgt, s->bed.tol);

  if (fans_card) {
    for (uint8_t i = 0; i < s->fan_n && i < ARQ_FAN_ROWS_MAX; ++i) {
      arq_fan_row_t &r = fan_rows[i];
      const arq_fan_t &f = s->fan[i];
      r.icon = f.kind == ARQ_FAN_LAYER ? &arq_icon_s_fan : f.kind == ARQ_FAN_HEATSINK ? &arq_icon_s_heatsink : &arq_icon_s_aux;
      r.label = f.kind == ARQ_FAN_LAYER ? "Capa" : f.kind == ARQ_FAN_HEATSINK ? "Disipador" : aux_name[f.index < 5 ? f.index : 0];
      r.pct = f.pct;
      snprintf(r.txt, sizeof(r.txt), "%u%%", (unsigned)f.pct);
    }
    arq_card_set_fans(fans_card, fan_rows, s->fan_n);
  }

  if (print_card) {
    // Solo datos reales: sin archivo de SD no hay nombre ni porcentaje; sin avance suficiente no hay tiempo restante
    if (s->sd_job) { arq_strlcpy(file_txt, s->file, sizeof(file_txt)); snprintf(pct_txt, sizeof(pct_txt), "%u%%", (unsigned)s->pct); }
    else           { arq_strlcpy(file_txt, "Impresion en curso", sizeof(file_txt)); arq_strlcpy(pct_txt, "--%", sizeof(pct_txt)); }
    char e[12], r[12];
    arq_fmt_hm(e, sizeof(e), s->elapsed_s);
    if (s->remaining_s) arq_fmt_hm(r, sizeof(r), s->remaining_s); else arq_strlcpy(r, "--:--", sizeof(r));
    snprintf(times_txt, sizeof(times_txt), "%s \xC2\xB7 resta %s", e, r);
    arq_card_set_sub(print_card, s->state == ARQ_PS_PAUSED ? "Pausada" : "", s->state == ARQ_PS_PAUSED ? ARQ_TEXT_WARN : ARQ_TEXT_OK);   // en curso: el estado ya lo dice la cabecera
    arq_card_set_percent(print_card, s->sd_job ? s->pct : 0);
    arq_card_set_bar_tint(print_card, s->state == ARQ_PS_PAUSED ? ARQ_TINT_WARN : ARQ_TINT_OK);
    arq_card_refresh(print_card);
  }
  prev = *s; prev_valid = true;
}
