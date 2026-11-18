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

#include "arquis_objects_view.h"

#include <stdio.h>
#include <string.h>

// ---- Geometria (480x320) ------------------------------------------------------------------------------------------------------
#define MAP_X    6
#define MAP_Y    (ARQ_HEADER_H + 4)
#define MAP_SZ   266
#define MAP_PAD  8
#define BOX_MIN  22                                   // tamano minimo de un rectangulo del mapa (tocable)
#define GRID_X   280
#define GRID_Y   MAP_Y
#define GRID_W   196
#define GRID_H   218
#define CELL_W   46
#define CELL_H   44
#define CELL_DX  50
#define CELL_DY  48
#define INFO_Y   196                                  // linea de estado, relativa al objeto de las fichas
#define ACT_Y    272
#define ACT_H    (ARQ_H - 6 - ACT_Y)

static arq_header_t    header;
static lv_obj_t       *o_map, *o_grid, *o_act;
static arq_obj_model_t M;
static bool            valid;
static lv_style_t      st_box[3], st_ring;            // [0 en espera, 1 imprimiendo, 2 cancelado]
static bool            styles_done;

static void mk_box(lv_style_t &s, lv_color_t fill, lv_color_t border) {
  lv_style_copy(&s, &lv_style_plain);
  s.body.main_color = s.body.grad_color = fill;
  s.body.border.color = border;
  s.body.border.width = 1;
  s.body.border.opa   = LV_OPA_COVER;
  s.body.radius       = 4;
  s.body.shadow.width = 0;
}

static void init_styles() {
  if (styles_done) return;
  styles_done = true;
  arq_theme_init();
  mk_box(st_box[0], ARQ_COL_PANEL_HI, ARQ_COL_MUTED);      // en espera: gris
  mk_box(st_box[1], ARQ_COL_GREEN,    ARQ_COL_GREEN_DK);   // imprimiendo: verde
  mk_box(st_box[2], ARQ_COL_DANGER,   ARQ_COL_DANGER);     // cancelado: rojo
  lv_style_copy(&st_ring, &lv_style_plain);                // seleccion: anillo azul sin relleno
  st_ring.body.opa = LV_OPA_TRANSP;
  st_ring.body.border.color = ARQ_COL_BLUE;
  st_ring.body.border.width = 3;
  st_ring.body.border.opa = LV_OPA_COVER;
  st_ring.body.radius = 4;
  st_ring.body.shadow.width = 0;
}

static uint8_t state_of(const arq_obj_model_t *m, int i) {
  if (m->canceled & (1UL << i)) return 2;
  return (i == m->active) ? 1 : 0;
}

// ---- Dibujo auxiliar -----------------------------------------------------------------------------------------------------------
static void draw_text(const lv_area_t *mask, lv_coord_t x, lv_coord_t y, const char *t, const lv_style_t *st) {
  lv_point_t sz;
  lv_txt_get_size(&sz, t, st->text.font, st->text.letter_space, st->text.line_space, LV_COORD_MAX, LV_TXT_FLAG_NONE);
  lv_area_t a = { x, y, (lv_coord_t)(x + sz.x), (lv_coord_t)(y + sz.y) };
  lv_draw_label(&a, mask, st, LV_OPA_COVER, t, LV_TXT_FLAG_NONE, nullptr, nullptr, nullptr, LV_BIDI_DIR_LTR);
}

static void draw_text_c(const lv_area_t *mask, const lv_area_t &box, const char *t, const lv_style_t *st) {   // centrado en un area
  lv_point_t sz;
  lv_txt_get_size(&sz, t, st->text.font, st->text.letter_space, st->text.line_space, LV_COORD_MAX, LV_TXT_FLAG_NONE);
  draw_text(mask, box.x1 + (lv_area_get_width(&box) - sz.x) / 2, box.y1 + (lv_area_get_height(&box) - sz.y) / 2, t, st);
}

static void draw_text_cx(const lv_area_t *mask, lv_coord_t cx, lv_coord_t y, const char *t, const lv_style_t *st) {   // centrado en x
  lv_point_t sz;
  lv_txt_get_size(&sz, t, st->text.font, st->text.letter_space, st->text.line_space, LV_COORD_MAX, LV_TXT_FLAG_NONE);
  draw_text(mask, cx - sz.x / 2, y, t, st);
}

// Rectangulo de un objeto en el mapa, relativo al origen del mapa. false = posicion aun desconocida.
static bool box_rect(const arq_obj_model_t *m, int i, lv_coord_t *x, lv_coord_t *y, lv_coord_t *w, lv_coord_t *h) {
  if (i < 0 || i >= m->n || !(m->has_box & (1UL << i))) return false;
  const int32_t dim = (m->bed_x > m->bed_y ? m->bed_x : m->bed_y) > 0 ? (m->bed_x > m->bed_y ? m->bed_x : m->bed_y) : 1;
  const int32_t span = MAP_SZ - 2 * MAP_PAD;
  int32_t bw = ((int32_t)(m->x1[i] - m->x0[i]) * span) / dim, bh = ((int32_t)(m->y1[i] - m->y0[i]) * span) / dim;
  const int32_t px = MAP_PAD + ((int32_t)m->x0[i] * span) / dim, py = MAP_PAD + ((int32_t)(m->bed_y - m->y1[i]) * span) / dim;   // Y crece hacia atras
  int32_t ox = 0, oy = 0;
  if (bw < BOX_MIN) { ox = (BOX_MIN - bw) / 2; bw = BOX_MIN; }
  if (bh < BOX_MIN) { oy = (BOX_MIN - bh) / 2; bh = BOX_MIN; }
  *x = (lv_coord_t)(px - ox); *y = (lv_coord_t)(py - oy); *w = (lv_coord_t)bw; *h = (lv_coord_t)bh;
  return true;
}

static void label_of(char *b, size_t n, const arq_obj_model_t *m, int i) {
  if (m->canceled & (1UL << i)) snprintf(b, n, "X"); else snprintf(b, n, "%d", i + 1);
}

// true = hay que dibujar (solo en LV_DESIGN_DRAW_MAIN y si el area visible no esta vacia)
static bool prep(lv_obj_t *o, const lv_area_t *mask_in, lv_design_mode_t mode, lv_area_t *a, lv_area_t *clip) {
  if (mode != LV_DESIGN_DRAW_MAIN) return false;
  lv_obj_get_coords(o, a);
  return lv_area_intersect(clip, mask_in, a);
}

// ---- Mapa de la cama -------------------------------------------------------------------------------------------------------------
static bool map_design(lv_obj_t *o, const lv_area_t *mask_in, lv_design_mode_t mode) {
  if (mode == LV_DESIGN_COVER_CHK) return false;           // esquinas redondeadas: no cubre todo el area
  lv_area_t a, clip;
  if (!prep(o, mask_in, mode, &a, &clip)) return true;
  const lv_area_t *mask = &clip;
  lv_draw_rect(&a, mask, arq_style_panel(), LV_OPA_COVER);

  if (M.n == 0) {
    const lv_coord_t cx = a.x1 + MAP_SZ / 2, cy = a.y1 + MAP_SZ / 2 - 30;
    draw_text_cx(mask, cx, cy,      "Sin objetos",                     arq_style_text(ARQ_TEXT_BODY));
    draw_text_cx(mask, cx, cy + 26, "El G-code no define objetos",     arq_style_text(ARQ_TEXT_MUTED));
    draw_text_cx(mask, cx, cy + 46, "(etiquetas M486)",                arq_style_text(ARQ_TEXT_MUTED));
    return true;
  }

  char t[6];
  int8_t selfound = -1;
  for (int i = 0; i < M.n; ++i) {
    lv_coord_t x, y, w, h;
    if (!box_rect(&M, i, &x, &y, &w, &h)) continue;
    const lv_area_t r = { (lv_coord_t)(a.x1 + x), (lv_coord_t)(a.y1 + y), (lv_coord_t)(a.x1 + x + w - 1), (lv_coord_t)(a.y1 + y + h - 1) };
    lv_draw_rect(&r, mask, &st_box[state_of(&M, i)], LV_OPA_COVER);
    if (i == M.sel) selfound = (int8_t)i;
    else { label_of(t, sizeof(t), &M, i); draw_text_c(mask, r, t, arq_style_text(ARQ_TEXT_BODY)); }
  }
  if (selfound >= 0) {                                                  // el seleccionado va encima de los demas
    lv_coord_t x, y, w, h; box_rect(&M, selfound, &x, &y, &w, &h);
    const lv_area_t r = { (lv_coord_t)(a.x1 + x), (lv_coord_t)(a.y1 + y), (lv_coord_t)(a.x1 + x + w - 1), (lv_coord_t)(a.y1 + y + h - 1) };
    lv_draw_rect(&r, mask, &st_box[state_of(&M, selfound)], LV_OPA_COVER);
    lv_draw_rect(&r, mask, &st_ring, LV_OPA_COVER);
    label_of(t, sizeof(t), &M, selfound); draw_text_c(mask, r, t, arq_style_text(ARQ_TEXT_BODY));
  }
  if (!M.has_box) {
    const lv_coord_t cx = a.x1 + MAP_SZ / 2, cy = a.y1 + MAP_SZ / 2 - 20;
    draw_text_cx(mask, cx, cy,      "Las posiciones aparecen", arq_style_text(ARQ_TEXT_MUTED));
    draw_text_cx(mask, cx, cy + 20, "al imprimir cada objeto", arq_style_text(ARQ_TEXT_MUTED));
  }
  draw_text_cx(mask, a.x1 + MAP_SZ / 2, a.y2 - 22, "Frente", arq_style_text(ARQ_TEXT_MUTED));
  return true;
}

// ---- Fichas numeradas + linea de estado ---------------------------------------------------------------------------------------------
static bool grid_design(lv_obj_t *o, const lv_area_t *mask_in, lv_design_mode_t mode) {
  if (mode == LV_DESIGN_COVER_CHK) return false;           // esquinas redondeadas: no cubre todo el area
  lv_area_t a, clip;
  if (!prep(o, mask_in, mode, &a, &clip)) return true;
  const lv_area_t *mask = &clip;
  char t[24];

  for (int i = 0; i < M.n; ++i) {
    const lv_coord_t x = a.x1 + (i % 4) * CELL_DX, y = a.y1 + (i / 4) * CELL_DY;
    const lv_area_t r = { x, y, (lv_coord_t)(x + CELL_W - 1), (lv_coord_t)(y + CELL_H - 1) };
    lv_draw_rect(&r, mask, &st_box[state_of(&M, i)], LV_OPA_COVER);
    if (i == M.sel) lv_draw_rect(&r, mask, &st_ring, LV_OPA_COVER);
    label_of(t, sizeof(t), &M, i);
    draw_text_c(mask, r, t, arq_style_text(ARQ_TEXT_BODY));
  }

  // linea de estado: punto de color + texto
  const lv_coord_t ly = a.y1 + INFO_Y;
  if (M.sel >= 0 && M.sel < M.n) {
    const uint8_t s = state_of(&M, M.sel);
    static const char *const word[] = { "En espera", "Imprimiendo", "Cancelado" };
    static const arq_tint_t tint[] = { ARQ_TINT_MUTED, ARQ_TINT_OK, ARQ_TINT_DANGER };
    const lv_area_t d = { (lv_coord_t)(a.x1 + 2), (lv_coord_t)(ly + 6), (lv_coord_t)(a.x1 + 9), (lv_coord_t)(ly + 13) };
    lv_draw_rect(&d, mask, arq_style_dot(tint[s]), LV_OPA_COVER);
    snprintf(t, sizeof(t), "Objeto %d: %s", M.sel + 1, word[s]);
    draw_text(mask, a.x1 + 16, ly, t, arq_style_text(ARQ_TEXT_BODY));
  }
  else if (M.n == 0)
    draw_text(mask, a.x1 + 2, ly, "Sin objetos", arq_style_text(ARQ_TEXT_MUTED));
  else if (M.total > M.n) {
    snprintf(t, sizeof(t), "Se muestran %d de %d", M.n, M.total);
    draw_text(mask, a.x1 + 2, ly, t, arq_style_text(ARQ_TEXT_MUTED));
  }
  else
    draw_text(mask, a.x1 + 2, ly, "Toca un objeto", arq_style_text(ARQ_TEXT_MUTED));
  return true;
}

// ---- API ---------------------------------------------------------------------------------------------------------------------------
void arq_objects_forget() { o_map = o_grid = o_act = nullptr; header.bar = header.back = nullptr; valid = false; memset(&M, 0, sizeof(M)); M.active = M.sel = -1; }

static lv_obj_t * custom_obj(lv_obj_t *scr, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h, lv_design_cb_t d, lv_event_cb_t cb, uint16_t id) {
  lv_obj_t *o = lv_obj_create(scr, nullptr);
  if (!o) return nullptr;                                  // heap agotado: sin objeto, sin caida
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_set_design_cb(o, d);
  lv_obj_set_click(o, true);
  lv_obj_set_event_cb_mks(o, cb, id, "", 0);
  return o;
}

void arq_objects_build(lv_obj_t *scr, lv_event_cb_t cb) {
  init_styles();
  arq_objects_forget();
  arq_header_create(&header, scr, "Cancelar objeto", false, cb, ARQ_OID_BACK);
  o_map  = custom_obj(scr, MAP_X, MAP_Y, MAP_SZ, MAP_SZ, map_design, cb, ARQ_OID_MAP);
  o_grid = custom_obj(scr, GRID_X, GRID_Y, GRID_W, GRID_H, grid_design, cb, ARQ_OID_GRID);
  o_act  = arq_button_create(scr, GRID_X, ACT_Y, GRID_W, ACT_H, nullptr, "Cancelar objeto", ARQ_BTN_DANGER, cb, ARQ_OID_ACTION);
  arq_card_set_disabled(o_act, true);
}

void arq_objects_update(const arq_obj_model_t *m) {
  arq_obj_model_t t = *m;
  if (t.n > ARQ_OBJ_MAX) t.n = ARQ_OBJ_MAX;
  if (t.sel >= (int8_t)t.n) t.sel = -1;
  if (valid && !memcmp(&M, &t, sizeof(M))) return;     // nada cambio: no se toca LVGL
  M = t;
  valid = true;
  if (o_map)  lv_obj_invalidate(o_map);
  if (o_grid) lv_obj_invalidate(o_grid);
  if (!o_act) return;
  const bool can = M.sel >= 0 && M.sel < M.n;
  const bool restore = can && (M.canceled & (1UL << M.sel));
  arq_card_set_disabled(o_act, !can);
  arq_card_set_kind(o_act, restore ? ARQ_BTN_SUCCESS : ARQ_BTN_DANGER);
  arq_card_set_title(o_act, restore ? "Restaurar objeto" : "Cancelar objeto", ARQ_TEXT_BODY);
}

int8_t arq_objects_pick(lv_obj_t *o, const lv_point_t *p) {
  if (!o || !p) return -1;
  lv_area_t a; lv_obj_get_coords(o, &a);
  const lv_coord_t rx = p->x - a.x1, ry = p->y - a.y1;
  if (o == o_map) {
    for (int i = M.n - 1; i >= 0; --i) {                    // el ultimo dibujado esta encima
      lv_coord_t x, y, w, h;
      if (box_rect(&M, i, &x, &y, &w, &h) && rx >= x && rx < x + w && ry >= y && ry < y + h) return (int8_t)i;
    }
  }
  else if (o == o_grid) {
    if (rx < 0 || ry < 0 || ry >= 4 * CELL_DY) return -1;
    const int col = rx / CELL_DX, row = ry / CELL_DY;
    if (col > 3 || rx % CELL_DX >= CELL_W || ry % CELL_DY >= CELL_H) return -1;
    const int i = row * 4 + col;
    if (i < M.n) return (int8_t)i;
  }
  return -1;
}
