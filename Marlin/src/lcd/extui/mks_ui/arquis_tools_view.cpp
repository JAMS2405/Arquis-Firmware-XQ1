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

#include "arquis_tools_view.h"

#include <stdio.h>
#include <string.h>

#define CY0   (ARQ_HEADER_H + 8)
#define CY1   (ARQ_H - ARQ_MARGIN)
#define PCT   "%"          // solo para textos literales; en snprintf usar "%%"

// ==== Velocidad ==================================================================================================================
static struct { arq_header_t hd; lv_obj_t *feed, *flow, *big, *dec, *inc, *st[3]; bool built, valid; arq_speed_model_t prev; } V;
static char v_f[8], v_e[8], v_big[8];

void arq_speed_forget() { memset(&V, 0, sizeof(V)); }

void arq_speed_build(lv_obj_t *scr, lv_event_cb_t cb) {
  arq_speed_forget();
  arq_header_create(&V.hd, scr, "Velocidad", false, cb, ARQ_SID_BACK);
  const lv_coord_t lw = 196, rx = ARQ_MARGIN + lw + ARQ_GAP, rw = ARQ_W - ARQ_MARGIN - rx;
  V.feed = arq_card_create(scr, ARQ_MARGIN, CY0, lw, 82, ARQ_LAYOUT_PICK, ARQ_BTN_SECONDARY, cb, ARQ_SID_FEED);
  arq_card_set_icon(V.feed, &arq_icon_speed, ARQ_TINT_ACCENT); arq_card_set_title(V.feed, "Movimiento", ARQ_TEXT_MUTED); arq_card_set_value(V.feed, v_f, ARQ_TEXT_NUM_L);
  V.flow = arq_card_create(scr, ARQ_MARGIN, CY0 + 90, lw, 82, ARQ_LAYOUT_PICK, ARQ_BTN_SECONDARY, cb, ARQ_SID_FLOW);
  arq_card_set_icon(V.flow, &arq_icon_filament, ARQ_TINT_ACCENT); arq_card_set_title(V.flow, "Extrusi\xC3\xB3n", ARQ_TEXT_MUTED); arq_card_set_value(V.flow, v_e, ARQ_TEXT_NUM_L);
  static const char *const step_txt[3] = { "1" PCT, "5" PCT, "10" PCT };
  const lv_coord_t sw = (lw - 2 * ARQ_GAP) / 3, sy = CY0 + 180;
  for (uint8_t i = 0; i < 3; ++i) V.st[i] = arq_button_create(scr, ARQ_MARGIN + i * (sw + ARQ_GAP), sy, sw, CY1 - sy, nullptr, step_txt[i], ARQ_BTN_SECONDARY, cb, ARQ_SID_S1 + i);
  V.big = arq_card_create(scr, rx, CY0, rw, 110, ARQ_LAYOUT_BIG, ARQ_BTN_SECONDARY, nullptr, 0);
  arq_card_set_value(V.big, v_big, ARQ_TEXT_NUM_L);
  const lv_coord_t by = CY0 + 118, bw = (rw - ARQ_GAP) / 2;
  V.dec = arq_button_create(scr, rx, by, bw, CY1 - by, &arq_icon_minus, nullptr, ARQ_BTN_SECONDARY, cb, ARQ_SID_DEC);
  V.inc = arq_button_create(scr, rx + bw + ARQ_GAP, by, rw - bw - ARQ_GAP, CY1 - by, &arq_icon_plus, nullptr, ARQ_BTN_SECONDARY, cb, ARQ_SID_INC);
  V.built = true;
}

void arq_speed_update(const arq_speed_model_t *m) {
  if (!V.built || (V.valid && !memcmp(&V.prev, m, sizeof(*m)))) return;
  V.prev = *m; V.valid = true;
  snprintf(v_f, sizeof(v_f), "%d%%", m->feed); snprintf(v_e, sizeof(v_e), "%d%%", m->flow);
  snprintf(v_big, sizeof(v_big), "%d%%", m->sel ? m->flow : m->feed);
  arq_card_set_selected(V.feed, m->sel == 0); arq_card_set_selected(V.flow, m->sel == 1);
  arq_card_set_title(V.big, m->sel ? "Extrusi\xC3\xB3n" : "Movimiento", ARQ_TEXT_MUTED);
  for (uint8_t i = 0; i < 3; ++i) { static const uint8_t v[3] = { 1, 5, 10 }; arq_card_set_kind(V.st[i], m->step == v[i] ? ARQ_BTN_PRIMARY : ARQ_BTN_SECONDARY); }
  arq_card_refresh(V.feed); arq_card_refresh(V.flow); arq_card_refresh(V.big);
}

// ==== Flujo de extrusion =========================================================================================================
static struct { arq_header_t hd; lv_obj_t *big, *pre[3], *st[3], *dec, *inc; bool built, valid; arq_flow_model_t prev; } F;
static char f_big[8];

void arq_flow_forget() { memset(&F, 0, sizeof(F)); }

void arq_flow_build(lv_obj_t *scr, lv_event_cb_t cb) {
  arq_flow_forget();
  arq_header_create(&F.hd, scr, "Flujo", false, cb, ARQ_WID_BACK);
  const lv_coord_t lw = 196, rx = ARQ_MARGIN + lw + ARQ_GAP, rw = ARQ_W - ARQ_MARGIN - rx;
  const lv_coord_t sw = (lw - 2 * ARQ_GAP) / 3, rh = (CY1 - CY0 - ARQ_GAP) / 2, sy = CY0 + rh + ARQ_GAP;
  static const char *const pre_txt[3] = { "90" PCT, "100" PCT, "110" PCT };
  static const char *const step_txt[3] = { "1" PCT, "5" PCT, "10" PCT };
  for (uint8_t i = 0; i < 3; ++i) {
    F.pre[i] = arq_button_create(scr, ARQ_MARGIN + i * (sw + ARQ_GAP), CY0, sw, rh, nullptr, pre_txt[i], ARQ_BTN_SECONDARY, cb, ARQ_WID_P0 + i);
    F.st[i]  = arq_button_create(scr, ARQ_MARGIN + i * (sw + ARQ_GAP), sy, sw, CY1 - sy, nullptr, step_txt[i], ARQ_BTN_SECONDARY, cb, ARQ_WID_S1 + i);
  }
  F.big = arq_card_create(scr, rx, CY0, rw, 110, ARQ_LAYOUT_BIG, ARQ_BTN_SECONDARY, nullptr, 0);
  arq_card_set_title(F.big, "Flujo", ARQ_TEXT_MUTED);
  arq_card_set_value(F.big, f_big, ARQ_TEXT_NUM_L);
  const lv_coord_t by = CY0 + 118, bw = (rw - ARQ_GAP) / 2;
  F.dec = arq_button_create(scr, rx, by, bw, CY1 - by, &arq_icon_minus, nullptr, ARQ_BTN_SECONDARY, cb, ARQ_WID_DEC);
  F.inc = arq_button_create(scr, rx + bw + ARQ_GAP, by, rw - bw - ARQ_GAP, CY1 - by, &arq_icon_plus, nullptr, ARQ_BTN_SECONDARY, cb, ARQ_WID_INC);
  F.built = true;
}

void arq_flow_update(const arq_flow_model_t *m) {
  if (!F.built || (F.valid && !memcmp(&F.prev, m, sizeof(*m)))) return;
  F.prev = *m; F.valid = true;
  snprintf(f_big, sizeof(f_big), "%d%%", m->flow);
  static const int16_t pv[3] = { ARQ_FLOW_PRESET_0, ARQ_FLOW_PRESET_1, ARQ_FLOW_PRESET_2 };
  static const uint8_t sv[3] = { 1, 5, 10 };
  for (uint8_t i = 0; i < 3; ++i) {
    arq_card_set_kind(F.pre[i], m->flow == pv[i] ? ARQ_BTN_PRIMARY : ARQ_BTN_SECONDARY);      // el preajuste activo se resalta
    arq_card_set_kind(F.st[i],  m->step == sv[i] ? ARQ_BTN_PRIMARY : ARQ_BTN_SECONDARY);
  }
  arq_card_refresh(F.big);
}

// ==== Nivelar cama ===============================================================================================================
#define LV_MAP_X   6
#define LV_MAP_SZ  (CY1 - CY0)
#define LV_BTN     54
static struct { arq_header_t hd; lv_obj_t *map, *pt[5], *info; } L;
static char l_lbl[5][4];

void arq_level_forget() { memset(&L, 0, sizeof(L)); }

void arq_level_build(lv_obj_t *scr, lv_event_cb_t cb, const arq_level_model_t *m) {
  arq_level_forget();
  arq_header_create(&L.hd, scr, "Nivelar cama", false, cb, ARQ_LID_BACK);
  const lv_coord_t mx = LV_MAP_X, my = CY0, sz = LV_MAP_SZ;
  L.map = arq_card_create(scr, mx, my, sz, sz, ARQ_LAYOUT_LIST, ARQ_BTN_SECONDARY, nullptr, 0);   // fondo del esquema de la cama (panel sin filas)
  const int32_t dim = (m->bed_x > m->bed_y ? m->bed_x : m->bed_y) > 0 ? (m->bed_x > m->bed_y ? m->bed_x : m->bed_y) : 1;
  const int32_t span = sz - LV_BTN - 8;                    // recorrido del centro del boton dentro del esquema
  for (uint8_t i = 0; i < 5; ++i) {
    int32_t px = (int32_t)m->x[i] * span / dim, py = (int32_t)(m->bed_y - m->y[i]) * span / dim;   // Y crece hacia atras
    px = px < 0 ? 0 : px > span ? span : px;
    py = py < 0 ? 0 : py > span ? span : py;
    snprintf(l_lbl[i], sizeof(l_lbl[i]), "%u", (unsigned)(i + 1));
    L.pt[i] = arq_button_create(scr, mx + 4 + px, my + 4 + py, LV_BTN, LV_BTN, nullptr, l_lbl[i], ARQ_BTN_SECONDARY, cb, ARQ_LID_P1 + i);
  }
  const lv_coord_t ix = mx + sz + ARQ_GAP, iw = ARQ_W - ARQ_MARGIN - ix;
  L.info = arq_card_create(scr, ix, CY0, iw, sz, ARQ_LAYOUT_DIALOG, ARQ_BTN_SECONDARY, nullptr, 0);
  arq_card_set_title(L.info, "C\xC3\xB3mo usarlo", ARQ_TEXT_BODY);
  arq_card_set_sub(L.info, "Toca un punto: el cabezal sube a Z10, se mueve al punto y baja a Z0. Ajusta el tornillo hasta que una hoja de papel roce.", ARQ_TEXT_MUTED);
}

// ==== Sistema y diagnostico ======================================================================================================
static struct { arq_header_t hd; lv_obj_t *left, *right; } I;

void arq_info_forget() { memset(&I, 0, sizeof(I)); }

void arq_info_build(lv_obj_t *scr, lv_event_cb_t cb, const char *header_title, arq_info_list_t *l, arq_info_list_t *r) {
  arq_info_forget();
  arq_header_create(&I.hd, scr, header_title, false, cb, ARQ_IID_BACK);
  const lv_coord_t lw = 236, rx = ARQ_MARGIN + lw + ARQ_GAP, rw = ARQ_W - ARQ_MARGIN - rx;
  const uint8_t n = l->n > r->n ? l->n : r->n;
  const lv_coord_t h = 26 + ARQ_LIST_PAD * 2 + n * ARQ_LIST_ROW_H - 4;
  I.left  = arq_card_create(scr, ARQ_MARGIN, CY0, lw, h, ARQ_LAYOUT_LIST, ARQ_BTN_SECONDARY, nullptr, 0);
  I.right = arq_card_create(scr, rx, CY0, rw, h, ARQ_LAYOUT_LIST, ARQ_BTN_SECONDARY, nullptr, 0);
  arq_card_set_title(I.left, l->title, ARQ_TEXT_MUTED);  arq_card_set_rows(I.left, l->rows, l->n);
  arq_card_set_title(I.right, r->title, ARQ_TEXT_MUTED); arq_card_set_rows(I.right, r->rows, r->n);
}

void arq_info_refresh() { if (I.left) arq_card_refresh(I.left); if (I.right) arq_card_refresh(I.right); }
