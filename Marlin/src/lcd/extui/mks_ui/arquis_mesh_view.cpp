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

#include "arquis_mesh_view.h"

#include <stdio.h>
#include <string.h>

#define CY0     (ARQ_HEADER_H + 8)
#define CY1     (ARQ_H - ARQ_MARGIN)
#define MAP_X   6
#define MAP_PAD 6
#define CELL    46
#define CGAP    4
#define MAP_SZ  (MAP_PAD * 2 + CELL * ARQ_MESH_N + CGAP * (ARQ_MESH_N - 1))   // 258
#define PX      (MAP_X + MAP_SZ + ARQ_GAP)                                       // 272
#define PW      (ARQ_W - ARQ_MARGIN - PX)                                        // 198
#define DEG     "\xC2\xB0"

#define PW_MAX 14      // objetos del panel derecho (maximo: fase de ajuste = 10)

static struct {
  arq_header_t hd;
  lv_obj_t *map, *pw[PW_MAX];
  uint8_t   npw;
  lv_event_cb_t cb;
  lv_obj_t *scr;
  int8_t    built;                        // fase construida (-1 = ninguna)
  bool      valid;
  arq_mesh_view_model_t M;                // copia del ultimo modelo
  // widgets con texto dinamico
  lv_obj_t *card, *toggle, *start, *save, *rep, *conf, *up, *dn, *step[ARQ_MESH_STEPS], *tmid, *tdec, *tinc, *cancel;
  arq_list_row_t rows[4];
  char t1[28], t2[24], t3[40], t4[16];
} V;

void arq_mesh_view_forget() { memset(&V, 0, sizeof(V)); V.built = -1; }

// ---- Mapa de 25 celdas (un solo objeto) ----------------------------------------------------------------------------------------------
static lv_style_t cell_st;
static bool cell_ready;

static void text_size(lv_point_t *sz, const char *t, const lv_style_t *st) {
  lv_txt_get_size(sz, t, st->text.font, st->text.letter_space, st->text.line_space, LV_COORD_MAX, LV_TXT_FLAG_NONE);
}
static void draw_text_c(const lv_area_t *mask, const lv_area_t &cell, const char *t, const lv_style_t *st) {   // centrado en la celda
  lv_point_t sz; text_size(&sz, t, st);
  const lv_coord_t x = cell.x1 + (lv_area_get_width(&cell) - sz.x) / 2, y = cell.y1 + (lv_area_get_height(&cell) - sz.y) / 2;
  lv_area_t a = { x, y, (lv_coord_t)(x + sz.x), (lv_coord_t)(y + sz.y) };
  lv_draw_label(&a, mask, st, LV_OPA_COVER, t, LV_TXT_FLAG_NONE, nullptr, nullptr, nullptr, LV_BIDI_DIR_LTR);
}

// Color de una desviacion: azul = mas baja que la referencia, ambar = mas alta, panel = a la altura de la referencia.
// (Mezcla propia en 8 bits: lv_color_mix() de LVGL oscurece un nivel incluso con mix = 0 y la referencia debe ser exactamente el color del panel.)
static lv_color_t dev_color(float t) {     // t en -1..+1
  const float a = t < 0 ? -t : t;
  const uint32_t k = (uint32_t)(a * 255.0f);
  const uint8_t r0 = 0x2D, g0 = 0x3B, b0 = 0x49;                           // ARQ_COL_PANEL_HI
  const uint8_t r1 = t < 0 ? 0x1F : 0xF5, g1 = t < 0 ? 0x86 : 0xA5, b1 = t < 0 ? 0xB5 : 0x24;   // ARQ_COL_BLUE_DK / ARQ_COL_WARN
  const uint8_t r = (uint8_t)((r1 * k + r0 * (255 - k)) / 255), g = (uint8_t)((g1 * k + g0 * (255 - k)) / 255), b = (uint8_t)((b1 * k + b0 * (255 - k)) / 255);
  #if LV_COLOR_DEPTH == 16 && !LV_COLOR_16_SWAP
    lv_color_t c; c.full = (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));     // RGB565 (LV_COLOR_MAKE con valores no constantes da avisos de estrechamiento)
    return c;
  #else
    return LV_COLOR_MAKE(r, g, b);
  #endif
}

static bool map_design(lv_obj_t *o, const lv_area_t *mask_in, lv_design_mode_t mode) {
  if (mode == LV_DESIGN_COVER_CHK) return false;
  if (mode != LV_DESIGN_DRAW_MAIN) return true;
  lv_area_t a; lv_obj_get_coords(o, &a);
  lv_area_t clip;
  if (!lv_area_intersect(&clip, mask_in, &a)) return true;
  const lv_area_t *mask = &clip;
  if (!cell_ready) { lv_style_copy(&cell_st, arq_style_panel()); cell_st.body.radius = 6; cell_st.body.border.width = 0; cell_ready = true; }
  lv_draw_rect(&a, mask, arq_style_panel(), LV_OPA_COVER);

  const arq_mesh_view_model_t &m = V.M;
  const bool active = m.phase == ARQ_MVP_MOVING || m.phase == ARQ_MVP_ADJUST;
  float limit = 0.10f;
  if (m.rel) { arq_mesh_stats_t st; arq_mesh_stats(m.val, m.mask, &st); const float lo = st.min < 0 ? -st.min : st.min, hi = st.max < 0 ? -st.max : st.max; limit = lo > hi ? lo : hi; if (limit < 0.10f) limit = 0.10f; }

  char txt[12];
  for (uint8_t p = 0; p < ARQ_MESH_PTS; ++p) {
    const uint8_t ix = arq_mesh_ix(p), iy = arq_mesh_iy(p);
    const lv_coord_t cx = a.x1 + MAP_PAD + ix * (CELL + CGAP), cy = a.y1 + MAP_PAD + (ARQ_MESH_N - 1 - iy) * (CELL + CGAP);   // Y crece hacia atras: delante abajo
    const lv_area_t c = { cx, cy, (lv_coord_t)(cx + CELL - 1), (lv_coord_t)(cy + CELL - 1) };
    const bool done = m.mask & (1UL << p), cur = active && p == m.cur;
    const lv_style_t *ts = arq_style_text(ARQ_TEXT_BODY);
    if (cur) {                                                // en curso: azul, con la Z actual
      cell_st.body.main_color = cell_st.body.grad_color = ARQ_COL_BLUE_DK;
      if (m.phase == ARQ_MVP_ADJUST) arq_mesh_fmt_z(txt, sizeof txt, m.z_now);          // la Z solo se muestra cuando se puede ajustar
      else snprintf(txt, sizeof txt, "%u", (unsigned)(arq_mesh_p_to_seq(p) + 1));       // mientras la boquilla se desplaza: numero de orden
    }
    else if (done) {
      if (m.rel) { cell_st.body.main_color = cell_st.body.grad_color = dev_color(arq_mesh_norm(m.val[p], limit)); arq_mesh_fmt_dev(txt, sizeof txt, m.val[p]); }
      else { cell_st.body.main_color = cell_st.body.grad_color = ARQ_COL_GREEN_DK; arq_mesh_fmt_z(txt, sizeof txt, m.val[p]); }   // medida (Z absoluta)
    }
    else {                                                    // pendiente: numero de orden de medicion
      cell_st.body.main_color = cell_st.body.grad_color = ARQ_COL_BG;
      snprintf(txt, sizeof txt, "%u", (unsigned)(arq_mesh_p_to_seq(p) + 1));
      ts = arq_style_text(ARQ_TEXT_MUTED);
    }
    lv_draw_rect(&c, mask, &cell_st, LV_OPA_COVER);
    draw_text_c(mask, c, txt, ts);
    if (m.sel == p) lv_draw_rect(&c, mask, arq_style_focus(), LV_OPA_COVER);          // seleccionada para repetir: anillo azul
  }
  return true;
}

int8_t arq_mesh_view_hit() {
  lv_indev_t *i = lv_indev_get_act();
  if (!i || !V.map) return -1;
  lv_point_t pt; lv_indev_get_point(i, &pt);
  lv_area_t a; lv_obj_get_coords(V.map, &a);
  const int x = pt.x - a.x1 - MAP_PAD + CGAP / 2, y = pt.y - a.y1 - MAP_PAD + CGAP / 2;
  if (x < 0 || y < 0) return -1;
  const int col = x / (CELL + CGAP), row = y / (CELL + CGAP);
  if (col >= ARQ_MESH_N || row >= ARQ_MESH_N) return -1;
  return (int8_t)((ARQ_MESH_N - 1 - row) * ARQ_MESH_N + col);
}

// ---- Panel derecho -------------------------------------------------------------------------------------------------------------------
static void panel_clear() {
  for (uint8_t i = 0; i < V.npw; ++i) if (V.pw[i]) lv_obj_del(V.pw[i]);
  memset(V.pw, 0, sizeof(V.pw)); V.npw = 0;
  V.card = V.toggle = V.start = V.save = V.rep = V.conf = V.up = V.dn = V.tmid = V.tdec = V.tinc = V.cancel = nullptr;
  for (uint8_t i = 0; i < ARQ_MESH_STEPS; ++i) V.step[i] = nullptr;
}
static lv_obj_t * keep(lv_obj_t *o) { if (o && V.npw < PW_MAX) V.pw[V.npw++] = o; return o; }
static lv_obj_t * btn(lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h, const lv_img_dsc_t *ic, const char *t, arq_btn_kind_t k, uint16_t id) {
  return keep(arq_button_create(V.scr, x, y, w, h, ic, t, k, V.cb, id));
}
static lv_obj_t * card(lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h, arq_layout_t l) {
  return keep(arq_card_create(V.scr, x, y, w, h, l, ARQ_BTN_SECONDARY, nullptr, 0));
}

static void build_idle() {
  V.card = card(PX, CY0, PW, 122, ARQ_LAYOUT_LIST);
  arq_card_set_rows(V.card, V.rows, 4);
  const lv_coord_t ty = CY0 + 128;
  V.tdec = btn(PX, ty, 46, 40, &arq_icon_minus, nullptr, ARQ_BTN_SECONDARY, ARQ_MV_TEMP_DEC);
  V.tmid = card(PX + 50, ty, PW - 100, 40, ARQ_LAYOUT_BUTTON);
  arq_card_set_title(V.tmid, V.t1, ARQ_TEXT_BODY);
  V.tinc = btn(PX + PW - 46, ty, 46, 40, &arq_icon_plus, nullptr, ARQ_BTN_SECONDARY, ARQ_MV_TEMP_INC);
  V.start = btn(PX, ty + 46, PW, 44, nullptr, "Iniciar medici\xC3\xB3n", ARQ_BTN_PRIMARY, ARQ_MV_START);
  const lv_coord_t by = ty + 46 + 48, bw = (PW - 8) / 3, bh = CY1 - by;
  V.toggle = btn(PX, by, bw, bh, nullptr, "Activar", ARQ_BTN_SECONDARY, ARQ_MV_TOGGLE);
  V.save = btn(PX + bw + 4, by, bw, bh, nullptr, "Guardar", ARQ_BTN_SECONDARY, ARQ_MV_SAVE);
  btn(PX + 2 * (bw + 4), by, PW - 2 * (bw + 4), bh, nullptr, "Cargar", ARQ_BTN_SECONDARY, ARQ_MV_LOAD);
}

static void build_prep() {                // PREP y MOVING
  V.card = card(PX, CY0, PW, 200, ARQ_LAYOUT_BIG);
  arq_card_set_title(V.card, V.t3, ARQ_TEXT_MUTED);
  arq_card_set_value(V.card, V.t1, ARQ_TEXT_NUM_L);
  arq_card_set_sub(V.card, V.t2, ARQ_TEXT_MUTED);
  V.cancel = btn(PX, CY1 - 48, PW, 48, nullptr, "Cancelar", ARQ_BTN_DANGER, ARQ_MV_CANCEL);
}

static void build_adjust() {
  V.card = keep(arq_card_create(V.scr, PX, CY0, PW, 76, ARQ_LAYOUT_PICK, ARQ_BTN_SECONDARY, nullptr, 0));
  arq_card_set_icon(V.card, &arq_icon_level, ARQ_TINT_ACCENT);
  arq_card_set_title(V.card, V.t3, ARQ_TEXT_MUTED);
  arq_card_set_value(V.card, V.t1, ARQ_TEXT_NUM_L);
  arq_card_set_value2(V.card, "mm");
  static const char *const step_txt[ARQ_MESH_STEPS] = { "1.00", "0.10", "0.05", "0.01" };
  const lv_coord_t sy = CY0 + 80, sw = PW / ARQ_MESH_STEPS - 3;
  for (uint8_t i = 0; i < ARQ_MESH_STEPS; ++i) V.step[i] = btn(PX + i * (PW / ARQ_MESH_STEPS), sy, sw, 40, nullptr, step_txt[i], ARQ_BTN_SECONDARY, ARQ_MV_STEP0 + i);
  const lv_coord_t uy = sy + 44, uw = (PW - 4) / 2;
  V.up = btn(PX, uy, uw, 46, &arq_icon_up, nullptr, ARQ_BTN_SECONDARY, ARQ_MV_UP);
  V.dn = btn(PX + uw + 4, uy, PW - uw - 4, 46, &arq_icon_down, nullptr, ARQ_BTN_SECONDARY, ARQ_MV_DOWN);
  V.conf = btn(PX, uy + 50, PW, 40, &arq_icon_check, "Confirmar", ARQ_BTN_SUCCESS, ARQ_MV_CONFIRM);
  const lv_coord_t by = uy + 50 + 44;
  V.rep = btn(PX, by, uw, CY1 - by, nullptr, "Repetir", ARQ_BTN_SECONDARY, ARQ_MV_REPEAT);
  V.cancel = btn(PX + uw + 4, by, PW - uw - 4, CY1 - by, nullptr, "Cancelar", ARQ_BTN_DANGER, ARQ_MV_CANCEL);
}

static void build_review() {
  V.card = card(PX, CY0, PW, 122, V.M.rel ? ARQ_LAYOUT_LIST : ARQ_LAYOUT_DIALOG);
  if (V.M.rel) arq_card_set_rows(V.card, V.rows, 4);
  arq_card_set_title(V.card, V.t3, ARQ_TEXT_BODY);
  if (!V.M.rel) arq_card_set_sub(V.card, V.M.msg, ARQ_TEXT_MUTED);
  const lv_coord_t y = CY0 + 128;
  V.save = btn(PX, y, PW, 44, &arq_icon_check, "Guardar", ARQ_BTN_SUCCESS, ARQ_MV_SAVE);
  V.rep = btn(PX, y + 48, PW, 38, nullptr, "Repetir punto", ARQ_BTN_SECONDARY, ARQ_MV_REPEAT);
  V.cancel = btn(PX, y + 90, PW, CY1 - (y + 90), nullptr, "Cancelar", ARQ_BTN_DANGER, ARQ_MV_CANCEL);
}

static void build_result() {
  V.card = card(PX, CY0, PW, CY1 - CY0, ARQ_LAYOUT_DIALOG);
  arq_card_set_title(V.card, V.t3, ARQ_TEXT_BODY);
  arq_card_set_sub(V.card, V.M.msg, ARQ_TEXT_MUTED);
  btn(PX + 12, CY1 - 12 - 44, PW - 24, 44, nullptr, "Aceptar", ARQ_BTN_PRIMARY, ARQ_MV_OK);
}

// ---- Textos ----------------------------------------------------------------------------------------------------------------------------
static void fill_rows(const arq_mesh_view_model_t &m) {
  arq_mesh_stats_t st; arq_mesh_stats(m.val, m.mask, &st);
  memset(V.rows, 0, sizeof(V.rows));
  static const char *const lab[4] = { "M\xC3\xA1ximo", "M\xC3\xADnimo", "Rango", "Desv. t\xC3\xADp." };
  for (uint8_t i = 0; i < 4; ++i) { V.rows[i].label = lab[i]; V.rows[i].dot = ARQ_TINT_AUTO; }
  if (!st.n || !m.rel) { for (uint8_t i = 0; i < 4; ++i) snprintf(V.rows[i].v1, sizeof(V.rows[i].v1), "--"); return; }
  arq_mesh_fmt_dev(V.rows[0].v1, sizeof(V.rows[0].v1), st.max); snprintf(V.rows[0].v2, sizeof(V.rows[0].v2), "P%u", (unsigned)(st.imax + 1));
  arq_mesh_fmt_dev(V.rows[1].v1, sizeof(V.rows[1].v1), st.min); snprintf(V.rows[1].v2, sizeof(V.rows[1].v2), "P%u", (unsigned)(st.imin + 1));
  arq_mesh_fmt_z(V.rows[2].v1, sizeof(V.rows[2].v1), st.range); snprintf(V.rows[2].v2, sizeof(V.rows[2].v2), "mm");
  arq_mesh_fmt_z(V.rows[3].v1, sizeof(V.rows[3].v1), st.sigma); snprintf(V.rows[3].v2, sizeof(V.rows[3].v2), "mm");
}

static void fill_texts(const arq_mesh_view_model_t &m) {
  switch (m.phase) {
    case ARQ_MVP_IDLE:
      fill_rows(m);
      snprintf(V.t1, sizeof(V.t1), "%d" DEG "C", (int)m.bed_set);
      snprintf(V.t3, sizeof(V.t3), "%s", m.mesh_ok ? (m.leveling_on ? "Malla activa" : "Malla inactiva") : "Sin malla v\xC3\xA1lida");
      break;
    case ARQ_MVP_PREP: {
      static const char *const ph[] = { "Preparando", "Calentando cama", "Estabilizando", "Homing", "Terminando" };
      snprintf(V.t3, sizeof(V.t3), "%s", ph[m.prep < 5 ? m.prep : 0]);
      if (m.prep == 1) { snprintf(V.t1, sizeof(V.t1), "%d/%d" DEG, (int)m.bed_cur, (int)m.bed_set); snprintf(V.t2, sizeof(V.t2), "Cama a temperatura"); }
      else if (m.prep == 2) { snprintf(V.t1, sizeof(V.t1), "%d:%02d", (int)(m.soak_s / 60), (int)(m.soak_s % 60)); snprintf(V.t2, sizeof(V.t2), "Dilataci\xC3\xB3n estable"); }
      else { snprintf(V.t1, sizeof(V.t1), "..."); snprintf(V.t2, sizeof(V.t2), "No toques la m\xC3\xA1quina"); }
    } break;
    case ARQ_MVP_MOVING:
      snprintf(V.t3, sizeof(V.t3), "Punto %u de %u", (unsigned)(m.count + 1 > ARQ_MESH_PTS ? ARQ_MESH_PTS : m.count + 1), (unsigned)ARQ_MESH_PTS);
      snprintf(V.t1, sizeof(V.t1), "..."); snprintf(V.t2, sizeof(V.t2), "Moviendo la boquilla");
      break;
    case ARQ_MVP_ADJUST:
      snprintf(V.t3, sizeof(V.t3), "Punto %u de %u", (unsigned)(m.count + 1 > ARQ_MESH_PTS ? ARQ_MESH_PTS : m.count + 1), (unsigned)ARQ_MESH_PTS);
      arq_mesh_fmt_z(V.t1, sizeof(V.t1), m.z_now);
      break;
    case ARQ_MVP_REVIEW:
      fill_rows(m);
      snprintf(V.t3, sizeof(V.t3), "%s", m.rel ? "25/25  Revisa" : "Revisa la medici\xC3\xB3n");
      break;
    default:
      snprintf(V.t3, sizeof(V.t3), "%s", m.msg_kind == ARQ_MV_MSG_OK ? "Listo" : m.msg_kind == ARQ_MV_MSG_ERR ? "Error" : "Aviso");
      break;
  }
}

static void apply_dynamic(const arq_mesh_view_model_t &m) {
  switch (m.phase) {
    case ARQ_MVP_IDLE:
      arq_card_set_title(V.card, V.t3, m.mesh_ok ? (m.leveling_on ? ARQ_TEXT_OK : ARQ_TEXT_WARN) : ARQ_TEXT_MUTED);
      arq_card_set_title(V.toggle, m.leveling_on ? "Desact." : "Activar", ARQ_TEXT_BODY);
      arq_card_set_disabled(V.toggle, !m.mesh_ok);
      arq_card_set_disabled(V.save, !m.mesh_ok);
      arq_card_set_disabled(V.start, false);
      arq_card_refresh(V.card); arq_card_refresh(V.tmid);
      break;
    case ARQ_MVP_PREP: case ARQ_MVP_MOVING:
      arq_card_refresh(V.card);
      break;
    case ARQ_MVP_ADJUST:
      for (uint8_t i = 0; i < ARQ_MESH_STEPS; ++i) arq_card_set_kind(V.step[i], i == m.step_idx ? ARQ_BTN_PRIMARY : ARQ_BTN_SECONDARY);
      arq_card_set_disabled(V.up, m.busy); arq_card_set_disabled(V.dn, m.busy);
      arq_card_set_disabled(V.conf, m.busy);
      arq_card_set_disabled(V.rep, !m.can_repeat || m.busy);
      arq_card_refresh(V.card);
      break;
    case ARQ_MVP_REVIEW:
      arq_card_set_disabled(V.save, !m.rel || m.busy);
      arq_card_set_disabled(V.rep, m.busy);
      arq_card_refresh(V.card);
      break;
    default: arq_card_refresh(V.card); break;
  }
}

// ---- API -------------------------------------------------------------------------------------------------------------------------------
void arq_mesh_view_build(lv_obj_t *scr, lv_event_cb_t cb) {
  arq_mesh_view_forget();
  V.scr = scr; V.cb = cb;
  arq_header_create(&V.hd, scr, "NIVELACI\xC3\x93N DE CAMA", false, cb, ARQ_MV_BACK);
  V.map = arq_card_create(scr, MAP_X, CY0, MAP_SZ, MAP_SZ, ARQ_LAYOUT_LIST, ARQ_BTN_SECONDARY, cb, ARQ_MV_MAP);   // la tarjeta aporta el evento tactil; su dibujo se sustituye
  if (V.map) lv_obj_set_design_cb(V.map, map_design);
}

void arq_mesh_view_update(const arq_mesh_view_model_t *m) {
  if (!V.scr || !V.map) return;
  const bool rebuild = (V.built != (int8_t)m->phase) || (m->phase == ARQ_MVP_REVIEW && V.valid && V.M.rel != m->rel);
  const bool changed = rebuild || !V.valid || memcmp(&V.M, m, sizeof(*m)) != 0;
  if (!changed) return;
  memcpy(&V.M, m, sizeof(*m)); V.valid = true;      // (memcpy: asi los bytes de relleno coinciden en la siguiente comparacion)
  fill_texts(V.M);
  if (rebuild) {
    panel_clear();
    switch (m->phase) {
      case ARQ_MVP_IDLE:   build_idle(); break;
      case ARQ_MVP_PREP:
      case ARQ_MVP_MOVING: build_prep(); break;
      case ARQ_MVP_ADJUST: build_adjust(); break;
      case ARQ_MVP_REVIEW: build_review(); break;
      default:             build_result(); break;
    }
    V.built = (int8_t)m->phase;
  }
  apply_dynamic(V.M);
  lv_obj_invalidate(V.map);
}
