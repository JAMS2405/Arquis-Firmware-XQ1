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

#include "arquis_ctrl_view.h"

#include <stdio.h>
#include <string.h>

#define CY0   (ARQ_HEADER_H + 8)                 // inicio del contenido
#define CY1   (ARQ_H - ARQ_MARGIN)               // fin del contenido
#define DEG   "\xC2\xB0"

static const arq_tint_t temp_tint[] = { ARQ_TINT_MUTED, ARQ_TINT_WARN, ARQ_TINT_OK, ARQ_TINT_ACCENT };       // apagado, calentando, objetivo, enfriando
static const arq_text_t temp_text[] = { ARQ_TEXT_MUTED, ARQ_TEXT_WARN, ARQ_TEXT_OK, ARQ_TEXT_ACCENT };
static const char *const temp_word[] = { "Apagado", "Calentando", "Objetivo alcanzado", "Enfriando" };

static void style_btn(lv_obj_t *o, bool on) { arq_card_set_kind(o, on ? ARQ_BTN_PRIMARY : ARQ_BTN_SECONDARY); }

// ==== Temperatura ================================================================================================================
static struct { arq_header_t hd; lv_obj_t *hot, *bed, *tgt, *dec, *inc, *st[3], *p1, *p2, *off; bool built, valid; arq_temps_model_t prev; } T;
static char t_hv[12], t_ht[12], t_bv[12], t_bt[12], t_tv[12], t_ts[48], t_p1[24], t_p2[24];
static const char *t_n1, *t_n2;

void arq_temps_forget() { memset(&T, 0, sizeof(T)); }

void arq_temps_build(lv_obj_t *scr, lv_event_cb_t cb, bool has_hot, bool has_bed, const char *p1_name, const char *p2_name) {
  arq_temps_forget();
  t_n1 = p1_name; t_n2 = p2_name;
  arq_header_create(&T.hd, scr, "Temperatura", false, cb, ARQ_TID_BACK);
  const lv_coord_t lw = 196, rx = ARQ_MARGIN + lw + ARQ_GAP, rw = ARQ_W - ARQ_MARGIN - rx;
  if (has_hot) { T.hot = arq_card_create(scr, ARQ_MARGIN, CY0, lw, 82, ARQ_LAYOUT_PICK, ARQ_BTN_SECONDARY, cb, ARQ_TID_HOT); arq_card_set_icon(T.hot, &arq_icon_nozzle, ARQ_TINT_ACCENT); arq_card_set_title(T.hot, "Hotend", ARQ_TEXT_MUTED); arq_card_set_value(T.hot, t_hv, ARQ_TEXT_NUM_L); arq_card_set_value2(T.hot, t_ht); }
  if (has_bed) { T.bed = arq_card_create(scr, ARQ_MARGIN, CY0 + 90, lw, 82, ARQ_LAYOUT_PICK, ARQ_BTN_SECONDARY, cb, ARQ_TID_BED); arq_card_set_icon(T.bed, &arq_icon_bed, ARQ_TINT_ACCENT); arq_card_set_title(T.bed, "Cama", ARQ_TEXT_MUTED); arq_card_set_value(T.bed, t_bv, ARQ_TEXT_NUM_L); arq_card_set_value2(T.bed, t_bt); }
  static const char *const step_txt[3] = { "1" DEG, "5" DEG, "10" DEG };
  const lv_coord_t sw = (lw - 2 * ARQ_GAP) / 3, sy = CY0 + 180;
  for (uint8_t i = 0; i < 3; ++i) T.st[i] = arq_button_create(scr, ARQ_MARGIN + i * (sw + ARQ_GAP), sy, sw, CY1 - sy, nullptr, step_txt[i], ARQ_BTN_SECONDARY, cb, ARQ_TID_S1 + i);

  T.tgt = arq_card_create(scr, rx, CY0, rw, 100, ARQ_LAYOUT_BIG, ARQ_BTN_SECONDARY, nullptr, 0);
  arq_card_set_value(T.tgt, t_tv, ARQ_TEXT_NUM_L); arq_card_set_sub(T.tgt, t_ts, ARQ_TEXT_MUTED);
  const lv_coord_t bw = (rw - ARQ_GAP) / 2, by = CY0 + 108, bh = 64;
  T.dec = arq_button_create(scr, rx, by, bw, bh, &arq_icon_minus, nullptr, ARQ_BTN_SECONDARY, cb, ARQ_TID_DEC);
  T.inc = arq_button_create(scr, rx + bw + ARQ_GAP, by, rw - bw - ARQ_GAP, bh, &arq_icon_plus, nullptr, ARQ_BTN_SECONDARY, cb, ARQ_TID_INC);
  const lv_coord_t py = by + bh + ARQ_GAP, pw = (rw - 2 * ARQ_GAP) / 3;
  T.p1  = arq_nav_tile_create(scr, rx, py, pw, CY1 - py, &arq_icon_temp, ARQ_TINT_ACCENT, t_p1, ARQ_BTN_SECONDARY, cb, ARQ_TID_P1);
  T.p2  = arq_nav_tile_create(scr, rx + pw + ARQ_GAP, py, pw, CY1 - py, &arq_icon_temp, ARQ_TINT_ACCENT, t_p2, ARQ_BTN_SECONDARY, cb, ARQ_TID_P2);
  T.off = arq_nav_tile_create(scr, rx + 2 * (pw + ARQ_GAP), py, rw - 2 * (pw + ARQ_GAP), CY1 - py, &arq_icon_power, ARQ_TINT_ACCENT, "Apagar", ARQ_BTN_SECONDARY, cb, ARQ_TID_OFF);
  T.built = true;
}

void arq_temps_update(const arq_temps_model_t *m) {
  if (!T.built || (T.valid && !memcmp(&T.prev, m, sizeof(*m)))) return;
  T.prev = *m; T.valid = true;
  const arq_temp_state_t hs = arq_temp_state(m->hot_cur, m->hot_tgt, m->hot_tol), bs = arq_temp_state(m->bed_cur, m->bed_tgt, m->bed_tol);
  snprintf(t_hv, sizeof(t_hv), "%d" DEG, m->hot_cur);  if (m->hot_tgt > 0) snprintf(t_ht, sizeof(t_ht), "%d" DEG, m->hot_tgt); else t_ht[0] = 0;
  snprintf(t_bv, sizeof(t_bv), "%d" DEG, m->bed_cur);  if (m->bed_tgt > 0) snprintf(t_bt, sizeof(t_bt), "%d" DEG, m->bed_tgt); else t_bt[0] = 0;
  if (T.hot) { arq_card_set_dot(T.hot, temp_tint[hs]); arq_card_set_selected(T.hot, m->sel == 0); arq_card_refresh(T.hot); }
  if (T.bed) { arq_card_set_dot(T.bed, temp_tint[bs]); arq_card_set_selected(T.bed, m->sel == 1); arq_card_refresh(T.bed); }
  const bool bed = m->sel == 1;
  const int16_t cur = bed ? m->bed_cur : m->hot_cur, tgt = bed ? m->bed_tgt : m->hot_tgt;
  const arq_temp_state_t st = bed ? bs : hs;
  snprintf(t_tv, sizeof(t_tv), "%d" DEG, tgt);
  snprintf(t_ts, sizeof(t_ts), "%s \xC2\xB7 actual %d" DEG, temp_word[st], cur);
  arq_card_set_title(T.tgt, bed ? "Cama \xC2\xB7 objetivo" : "Hotend \xC2\xB7 objetivo", ARQ_TEXT_MUTED);
  arq_card_set_sub(T.tgt, t_ts, temp_text[st]); arq_card_set_dot(T.tgt, temp_tint[st]); arq_card_refresh(T.tgt);
  for (uint8_t i = 0; i < 3; ++i) { static const uint8_t v[3] = { 1, 5, 10 }; style_btn(T.st[i], m->step == v[i]); }
  snprintf(t_p1, sizeof(t_p1), "%s %d" DEG, t_n1, m->p1);
  snprintf(t_p2, sizeof(t_p2), "%s %d" DEG, t_n2, m->p2);
  arq_card_refresh(T.p1); arq_card_refresh(T.p2);
}

// ==== Ventiladores ===============================================================================================================
static struct { arq_header_t hd; lv_obj_t *f[ARQ_FANCTL_MAX], *big, *dec, *inc, *off, *half, *full; bool built, valid; uint8_t n; const char *const *names; arq_fanctl_model_t prev; } F;
static char f_v[ARQ_FANCTL_MAX][8], f_big[8];

void arq_fanctl_forget() { memset(&F, 0, sizeof(F)); }

void arq_fanctl_build(lv_obj_t *scr, lv_event_cb_t cb, uint8_t n, const char *const *names) {
  arq_fanctl_forget();
  F.n = n > ARQ_FANCTL_MAX ? ARQ_FANCTL_MAX : n; F.names = names;
  arq_header_create(&F.hd, scr, "Ventiladores", false, cb, ARQ_FID_BACK);
  const lv_coord_t lw = 196, rx = ARQ_MARGIN + lw + ARQ_GAP, rw = ARQ_W - ARQ_MARGIN - rx;
  for (uint8_t i = 0; i < F.n; ++i) {
    F.f[i] = arq_card_create(scr, ARQ_MARGIN, CY0 + i * 90, lw, 82, ARQ_LAYOUT_PICK, ARQ_BTN_SECONDARY, F.n > 1 ? cb : nullptr, ARQ_FID_F0 + i);
    arq_card_set_icon(F.f[i], &arq_icon_fan, ARQ_TINT_ACCENT); arq_card_set_title(F.f[i], names[i], ARQ_TEXT_MUTED); arq_card_set_value(F.f[i], f_v[i], ARQ_TEXT_NUM_L);
  }
  F.big = arq_card_create(scr, rx, CY0, rw, 110, ARQ_LAYOUT_BIG, ARQ_BTN_SECONDARY, nullptr, 0);
  arq_card_set_value(F.big, f_big, ARQ_TEXT_NUM_L); arq_card_set_show_bar(F.big, true); arq_card_set_bar_tint(F.big, ARQ_TINT_OK);
  const lv_coord_t bw = (rw - ARQ_GAP) / 2, by = CY0 + 118, bh = 60;
  F.dec = arq_button_create(scr, rx, by, bw, bh, &arq_icon_minus, nullptr, ARQ_BTN_SECONDARY, cb, ARQ_FID_DEC);
  F.inc = arq_button_create(scr, rx + bw + ARQ_GAP, by, rw - bw - ARQ_GAP, bh, &arq_icon_plus, nullptr, ARQ_BTN_SECONDARY, cb, ARQ_FID_INC);
  const lv_coord_t py = by + bh + ARQ_GAP, pw = (rw - 2 * ARQ_GAP) / 3;
  F.off  = arq_nav_tile_create(scr, rx, py, pw, CY1 - py, &arq_icon_power, ARQ_TINT_ACCENT, "Apagar", ARQ_BTN_SECONDARY, cb, ARQ_FID_OFF);
  F.half = arq_nav_tile_create(scr, rx + pw + ARQ_GAP, py, pw, CY1 - py, &arq_icon_fan, ARQ_TINT_ACCENT, "50%", ARQ_BTN_SECONDARY, cb, ARQ_FID_HALF);
  F.full = arq_nav_tile_create(scr, rx + 2 * (pw + ARQ_GAP), py, rw - 2 * (pw + ARQ_GAP), CY1 - py, &arq_icon_fan, ARQ_TINT_ACCENT, "100%", ARQ_BTN_SECONDARY, cb, ARQ_FID_FULL);
  F.built = true;
}

void arq_fanctl_update(const arq_fanctl_model_t *m) {
  if (!F.built || (F.valid && !memcmp(&F.prev, m, sizeof(*m)))) return;
  F.prev = *m; F.valid = true;
  for (uint8_t i = 0; i < F.n; ++i) {
    snprintf(f_v[i], sizeof(f_v[i]), "%u%%", (unsigned)m->pct[i]);
    arq_card_set_dot(F.f[i], m->pct[i] ? ARQ_TINT_OK : ARQ_TINT_MUTED);
    arq_card_set_selected(F.f[i], F.n > 1 && m->sel == i);
    arq_card_refresh(F.f[i]);
  }
  const uint8_t s = m->sel < F.n ? m->sel : 0;
  snprintf(f_big, sizeof(f_big), "%u%%", (unsigned)m->pct[s]);
  arq_card_set_title(F.big, F.names[s], ARQ_TEXT_MUTED);
  arq_card_set_percent(F.big, m->pct[s]);
  arq_card_refresh(F.big);
}

// ==== Mover ======================================================================================================================
static struct { arq_header_t hd; lv_obj_t *dir[6], *list, *st[3], *chip; bool built, valid; arq_move_model_t prev; } M;
static arq_list_row_t m_rows[3];
static char m_chip[32];

void arq_move_forget() { memset(&M, 0, sizeof(M)); }

void arq_move_build(lv_obj_t *scr, lv_event_cb_t cb) {
  arq_move_forget();
  arq_header_create(&M.hd, scr, "Mover", false, cb, ARQ_MID_BACK);
  const lv_coord_t c = 68, g = 6, x1 = ARQ_MARGIN + c + g, x2 = x1 + c + g, y1 = CY0 + c + g, y2 = y1 + c + g;
  M.dir[0] = arq_nav_tile_create(scr, x1, CY0, c, c, &arq_icon_up,    ARQ_TINT_ACCENT, "Y+", ARQ_BTN_SECONDARY, cb, ARQ_MID_YP);
  M.dir[1] = arq_nav_tile_create(scr, ARQ_MARGIN, y1, c, c, &arq_icon_left,  ARQ_TINT_ACCENT, "X-", ARQ_BTN_SECONDARY, cb, ARQ_MID_XN);
  M.dir[2] = arq_nav_tile_create(scr, x2, y1, c, c, &arq_icon_right, ARQ_TINT_ACCENT, "X+", ARQ_BTN_SECONDARY, cb, ARQ_MID_XP);
  M.dir[3] = arq_nav_tile_create(scr, x1, y2, c, c, &arq_icon_down,  ARQ_TINT_ACCENT, "Y-", ARQ_BTN_SECONDARY, cb, ARQ_MID_YN);
  const lv_coord_t zx = x2 + c + ARQ_GAP, zw = 92, zh = (3 * c + 2 * g - g) / 2;
  M.dir[4] = arq_nav_tile_create(scr, zx, CY0, zw, zh, &arq_icon_up,   ARQ_TINT_ACCENT, "Z+", ARQ_BTN_SECONDARY, cb, ARQ_MID_ZP);
  M.dir[5] = arq_nav_tile_create(scr, zx, CY0 + zh + g, zw, zh, &arq_icon_down, ARQ_TINT_ACCENT, "Z-", ARQ_BTN_SECONDARY, cb, ARQ_MID_ZN);
  const lv_coord_t rx = zx + zw + ARQ_GAP, rw = ARQ_W - ARQ_MARGIN - rx;
  M.list = arq_card_create(scr, rx, CY0, rw, ARQ_LIST_PAD * 2 + 3 * ARQ_LIST_ROW_H - 4, ARQ_LAYOUT_LIST, ARQ_BTN_SECONDARY, nullptr, 0);
  m_rows[0].label = "X"; m_rows[1].label = "Y"; m_rows[2].label = "Z";
  arq_card_set_rows(M.list, m_rows, 3);
  static const char *const st_txt[3] = { "0,1 mm", "1 mm", "10 mm" };
  const lv_coord_t sy0 = CY0 + ARQ_LIST_PAD * 2 + 3 * ARQ_LIST_ROW_H - 4 + g, sh = (CY1 - sy0 - 2 * g) / 3;
  for (uint8_t i = 0; i < 3; ++i) M.st[i] = arq_button_create(scr, rx, sy0 + i * (sh + g), rw, sh, nullptr, st_txt[i], ARQ_BTN_SECONDARY, cb, ARQ_MID_S01 + i);
  M.chip = arq_chip_create(scr, ARQ_MARGIN, y2 + c + ARQ_GAP, zx + zw - ARQ_MARGIN, "", ARQ_TINT_MUTED);
  arq_card_set_title(M.chip, m_chip, ARQ_TEXT_BODY);
  M.built = true;
}

static void fmt_mm(char *b, size_t n, int32_t cmm) {
  const char *sg = cmm < 0 ? "-" : "";
  if (cmm < 0) cmm = -cmm;
  snprintf(b, n, "%s%ld.%ld mm", sg, (long)(cmm / 100), (long)((cmm % 100) / 10));
}

void arq_move_update(const arq_move_model_t *m) {
  if (!M.built || (M.valid && !memcmp(&M.prev, m, sizeof(*m)))) return;
  M.prev = *m; M.valid = true;
  for (uint8_t i = 0; i < 3; ++i) {
    fmt_mm(m_rows[i].v1, sizeof(m_rows[i].v1), m->pos_cmm[i]);
    m_rows[i].v2[0] = 0;
    m_rows[i].dot = (m->homed >> i) & 1 ? ARQ_TINT_OK : ARQ_TINT_WARN;
  }
  arq_card_set_rows(M.list, m_rows, 3);
  for (uint8_t i = 0; i < 3; ++i) style_btn(M.st[i], m->step == i);
  const bool all = (m->homed & 7) == 7;
  snprintf(m_chip, sizeof(m_chip), "%s", all ? "Ejes referenciados" : "Ejes sin referenciar");
  arq_card_set_dot(M.chip, all ? ARQ_TINT_OK : ARQ_TINT_WARN);
  arq_card_refresh(M.chip);
}

// ==== Extrusion ==================================================================================================================
static struct { arq_header_t hd; lv_obj_t *hot, *amount, *temp, *in, *out, *step, *speed; bool built, valid; arq_extrude_model_t prev; } E;
static char e_hv[12], e_ht[12], e_amt[14], e_sub[40], e_step[16], e_speed[20];

void arq_extrude_forget() { memset(&E, 0, sizeof(E)); }

void arq_extrude_build(lv_obj_t *scr, lv_event_cb_t cb, bool multi) {
  arq_extrude_forget();
  arq_header_create(&E.hd, scr, "Extrusi\xC3\xB3n", false, cb, ARQ_EID_BACK);
  const lv_coord_t lw = 224, rx = ARQ_MARGIN + lw + ARQ_GAP, rw = ARQ_W - ARQ_MARGIN - rx;
  E.hot = arq_card_create(scr, ARQ_MARGIN, CY0, lw, 82, ARQ_LAYOUT_PICK, ARQ_BTN_SECONDARY, multi ? cb : nullptr, ARQ_EID_TYPE);
  arq_card_set_icon(E.hot, &arq_icon_nozzle, ARQ_TINT_ACCENT); arq_card_set_value(E.hot, e_hv, ARQ_TEXT_NUM_L); arq_card_set_value2(E.hot, e_ht);
  E.amount = arq_card_create(scr, ARQ_MARGIN, CY0 + 90, lw, 90, ARQ_LAYOUT_BIG, ARQ_BTN_SECONDARY, nullptr, 0);
  arq_card_set_title(E.amount, "Extruido (mm)", ARQ_TEXT_MUTED); arq_card_set_value(E.amount, e_amt, ARQ_TEXT_NUM_L); arq_card_set_sub(E.amount, e_sub, ARQ_TEXT_MUTED);
  const lv_coord_t ty = CY0 + 188;
  E.temp = arq_button_create(scr, ARQ_MARGIN, ty, lw, CY1 - ty, &arq_icon_temp, "Temperatura", ARQ_BTN_SECONDARY, cb, ARQ_EID_TEMP);
  const lv_coord_t bh = 80;
  E.in  = arq_button_create(scr, rx, CY0, rw, bh, &arq_icon_down, "Extruir", ARQ_BTN_PRIMARY, cb, ARQ_EID_IN);
  E.out = arq_button_create(scr, rx, CY0 + bh + ARQ_GAP, rw, bh, &arq_icon_up, "Retraer", ARQ_BTN_SECONDARY, cb, ARQ_EID_OUT);
  const lv_coord_t sy = CY0 + 2 * (bh + ARQ_GAP), sw = (rw - ARQ_GAP) / 2;
  E.step  = arq_button_create(scr, rx, sy, sw, CY1 - sy, nullptr, e_step, ARQ_BTN_SECONDARY, cb, ARQ_EID_STEP);
  E.speed = arq_button_create(scr, rx + sw + ARQ_GAP, sy, rw - sw - ARQ_GAP, CY1 - sy, nullptr, e_speed, ARQ_BTN_SECONDARY, cb, ARQ_EID_SPEED);
  E.built = true;
}

void arq_extrude_update(const arq_extrude_model_t *m) {
  if (!E.built || (E.valid && !memcmp(&E.prev, m, sizeof(*m)))) return;
  E.prev = *m; E.valid = true;
  const arq_temp_state_t st = arq_temp_state(m->cur, m->tgt, m->tol);
  snprintf(e_hv, sizeof(e_hv), "%d" DEG, m->cur); if (m->tgt > 0) snprintf(e_ht, sizeof(e_ht), "%d" DEG, m->tgt); else e_ht[0] = 0;
  arq_card_set_title(E.hot, m->ext_idx ? "Hotend 2" : "Hotend", ARQ_TEXT_MUTED);
  arq_card_set_dot(E.hot, temp_tint[st]); arq_card_refresh(E.hot);
  snprintf(e_amt, sizeof(e_amt), "%ld", (long)m->amount);
  if (m->hot_enough) snprintf(e_sub, sizeof(e_sub), "Listo para extruir"); else snprintf(e_sub, sizeof(e_sub), "Hotend fr\xC3\xADo (m\xC3\xADn. %d" DEG ")", m->min_temp);
  arq_card_set_sub(E.amount, e_sub, m->hot_enough ? ARQ_TEXT_OK : ARQ_TEXT_WARN); arq_card_set_dot(E.amount, m->hot_enough ? ARQ_TINT_OK : ARQ_TINT_WARN); arq_card_refresh(E.amount);
  arq_card_set_disabled(E.in, !m->hot_enough); arq_card_set_disabled(E.out, !m->hot_enough);
  snprintf(e_step, sizeof(e_step), "Paso %u mm", (unsigned)m->step_mm);
  static const char *const sp[3] = { "Lenta", "Normal", "R\xC3\xA1pida" };
  snprintf(e_speed, sizeof(e_speed), "Vel. %s", sp[m->speed > 2 ? 1 : m->speed]);
  arq_card_refresh(E.step); arq_card_refresh(E.speed);
}

// ==== Opciones ===================================================================================================================
static struct { arq_header_t hd; lv_obj_t *item[ARQ_OPT_MAX]; const lv_img_dsc_t *icon[ARQ_OPT_MAX]; uint8_t n; } O;

void arq_opts_forget() { memset(&O, 0, sizeof(O)); }

static void opt_apply(uint8_t i, const char *sub, uint8_t kind) {
  arq_card_set_kind(O.item[i], (arq_btn_kind_t)kind);
  arq_card_set_icon(O.item[i], O.icon[i], kind == ARQ_BTN_SUCCESS ? ARQ_TINT_AUTO : ARQ_TINT_ACCENT);   // sobre verde el icono toma el color del texto del boton
  arq_card_set_sub(O.item[i], sub, kind == ARQ_BTN_SUCCESS ? ARQ_TEXT_DARK : ARQ_TEXT_MUTED);
}

void arq_opts_build(lv_obj_t *scr, lv_event_cb_t cb, const char *title, const arq_opt_item_t *items, uint8_t n) {
  arq_opts_forget();
  O.n = n > ARQ_OPT_MAX ? ARQ_OPT_MAX : n;
  arq_header_create(&O.hd, scr, title, false, cb, ARQ_XID_BACK);
  const lv_coord_t w = (ARQ_W - 2 * ARQ_MARGIN - 3 * ARQ_GAP) / 4, h = (CY1 - CY0 - ARQ_GAP) / 2;
  for (uint8_t i = 0; i < O.n; ++i) {
    O.item[i] = arq_nav_tile_create(scr, ARQ_MARGIN + (i % 4) * (w + ARQ_GAP), CY0 + (i / 4) * (h + ARQ_GAP), w, h, items[i].icon, ARQ_TINT_ACCENT, items[i].title,
                                    (arq_btn_kind_t)items[i].kind, cb, ARQ_XID_ITEM0 + i);
    O.icon[i] = items[i].icon;
    opt_apply(i, items[i].sub, items[i].kind);
  }
}

void arq_opts_set(uint8_t i, const char *sub, uint8_t kind) {
  if (i < O.n) opt_apply(i, sub, kind);
}
