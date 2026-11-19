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
#define GRID_X   280
#define GRID_Y   MAP_Y
#define GRID_W   196
#define GRID_H   224
#define ROW_H    30                                   // fila de la lista
#define NAV_Y    154                                  // fila de paginacion y zoom
#define NAV_H    28
#define INFO_Y   186                                  // dos lineas de informacion bajo la lista
#define ACT_Y    272
#define ACT_H    (ARQ_H - 6 - ACT_Y)
#define BTN_ARROW_W  38                               // [<] [2/6] [>]  [x1][x2][x4] = 38+44+38 + 4 + 3*24 = 196
#define PAGE_W       44
#define ZOOM_X       124
#define ZOOM_W       24

static arq_header_t    header;
static lv_obj_t       *o_map, *o_grid, *o_act;
static const arq_obj_model_t *M;                      // modelo del que llama (sin copia)
static uint32_t        last_sig;
static bool            valid;
static lv_style_t      st_box[5], st_ring, st_bed, st_row_sel, st_btn;   // cajas por estado ARQ_ST_*, anillo de seleccion, cama, fila elegida, boton pequeno
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
  mk_box(st_box[ARQ_ST_PENDING],     ARQ_COL_PANEL_HI, ARQ_COL_MUTED);     // pendiente: gris
  mk_box(st_box[ARQ_ST_PRINTING],    ARQ_COL_GREEN,    ARQ_COL_GREEN_DK);  // imprimiendo: verde
  mk_box(st_box[ARQ_ST_DONE],        ARQ_COL_BLUE_DK,  ARQ_COL_BLUE);      // completado: azul
  mk_box(st_box[ARQ_ST_CANCELED],    ARQ_COL_DANGER,   ARQ_COL_DANGER);    // cancelado: rojo
  mk_box(st_box[ARQ_ST_UNAVAILABLE], ARQ_COL_PANEL,    ARQ_COL_WARN);      // no disponible: borde ambar
  mk_box(st_bed, ARQ_COL_BG, ARQ_COL_LINE);
  st_bed.body.radius = 0;
  mk_box(st_row_sel, ARQ_COL_PANEL_HI, ARQ_COL_BLUE);
  mk_box(st_btn, ARQ_COL_PANEL_HI, ARQ_COL_LINE);
  lv_style_copy(&st_ring, &lv_style_plain);                                // seleccion: anillo azul sin relleno
  st_ring.body.opa = LV_OPA_TRANSP;
  st_ring.body.border.color = ARQ_COL_BLUE;
  st_ring.body.border.width = 3;
  st_ring.body.border.opa = LV_OPA_COVER;
  st_ring.body.radius = 4;
  st_ring.body.shadow.width = 0;
}

// ---- Texto y estado -------------------------------------------------------------------------------------------------------------
static const char *const STATE_WORD[5]  = { "Pendiente", "Imprimiendo", "Completado", "Cancelado", "No disponible" };
static const char        STATE_BADGE[5] = { 0, '>', '+', 'x', '?' };                // el estado nunca depende solo del color
static const arq_tint_t  STATE_TINT[5]  = { ARQ_TINT_MUTED, ARQ_TINT_OK, ARQ_TINT_ACCENT, ARQ_TINT_DANGER, ARQ_TINT_WARN };

static uint8_t count_of(const arq_obj_model_t *m) { return m->n > ARQ_OBJ_MAX ? ARQ_OBJ_MAX : m->n; }
static uint8_t state_of(const arq_obj_model_t *m, uint8_t i) { return m->state[i] <= ARQ_ST_UNAVAILABLE ? m->state[i] : (uint8_t)ARQ_ST_UNAVAILABLE; }
static uint8_t zoom_of(const arq_obj_model_t *m) { return (m->zoom == 2 || m->zoom == 4) ? m->zoom : 1; }
static uint8_t pages_of(const arq_obj_model_t *m) { const uint8_t n = count_of(m); return n ? (uint8_t)((n + ARQ_OBJ_ROWS - 1) / ARQ_OBJ_ROWS) : 1; }
static uint8_t page_of(const arq_obj_model_t *m) { const uint8_t p = pages_of(m); return m->page < p ? m->page : (uint8_t)(p - 1); }

static void draw_text(const lv_area_t *mask, lv_coord_t x, lv_coord_t y, const char *t, const lv_style_t *st) {
  lv_point_t sz;
  lv_txt_get_size(&sz, t, st->text.font, st->text.letter_space, st->text.line_space, LV_COORD_MAX, LV_TXT_FLAG_NONE);
  lv_area_t a = { x, y, (lv_coord_t)(x + sz.x), (lv_coord_t)(y + sz.y) };
  lv_draw_label(&a, mask, st, LV_OPA_COVER, t, LV_TXT_FLAG_NONE, nullptr, nullptr, nullptr, LV_BIDI_DIR_LTR);
}

static lv_coord_t text_w(const char *t, const lv_style_t *st) {
  lv_point_t sz;
  lv_txt_get_size(&sz, t, st->text.font, st->text.letter_space, st->text.line_space, LV_COORD_MAX, LV_TXT_FLAG_NONE);
  return sz.x;
}

static void draw_text_c(const lv_area_t *mask, const lv_area_t &box, const char *t, const lv_style_t *st) {   // centrado en un area
  lv_point_t sz;
  lv_txt_get_size(&sz, t, st->text.font, st->text.letter_space, st->text.line_space, LV_COORD_MAX, LV_TXT_FLAG_NONE);
  draw_text(mask, box.x1 + (lv_area_get_width(&box) - sz.x) / 2, box.y1 + (lv_area_get_height(&box) - sz.y) / 2, t, st);
}

static void draw_text_cx(const lv_area_t *mask, lv_coord_t cx, lv_coord_t y, const char *t, const lv_style_t *st) {   // centrado en x
  draw_text(mask, cx - text_w(t, st) / 2, y, t, st);
}

// Recorta con "..." hasta que quepa en max_w. cap = tamano del buffer de s. Los nombres del manifiesto pueden ser UTF-8: no se parte un caracter.
static void fit_text(char *s, size_t cap, lv_coord_t max_w, const lv_style_t *st) {
  if (text_w(s, st) <= max_w) return;
  char t[48];
  size_t n = strlen(s);
  const size_t lim = (cap < sizeof(t) ? cap : sizeof(t)) - 4;           // n + "..." + NUL debe caber en s y en t
  if (n > lim) n = lim;
  for (;;) {
    while (n > 0 && ((uint8_t)s[n] & 0xC0) == 0x80) --n;               // s[n] es continuacion UTF-8: retroceder al inicio del caracter
    memcpy(t, s, n); memcpy(t + n, "...", 4);
    if (n == 0 || text_w(t, st) <= max_w) { memcpy(s, t, n + 4); return; }
    --n;
  }
}

// ---- Geometria del mapa ---------------------------------------------------------------------------------------------------------
static bool obj_box(const arq_obj_model_t *m, uint8_t i, int32_t *cx, int32_t *cy, int32_t *w, int32_t *d, char *shape) {
  if (!m->src || !m->src->box || i >= count_of(m)) return false;
  return m->src->box(i, cx, cy, w, d, shape);
}

// Vista del mapa en coordenadas RELATIVAS al objeto LVGL del mapa. A x2/x4 se centra en el objeto seleccionado (si se conoce su posicion)
static void map_view(const arq_obj_model_t *m, ArqView *v) {
  v->x = 0; v->y = 0; v->w = MAP_SZ; v->h = MAP_SZ; v->zoom = zoom_of(m); v->has_center = false; v->cx = v->cy = 0;
  if (v->zoom > 1 && m->sel >= 0) {
    int32_t cx, cy, w, d; char sh;
    if (obj_box(m, (uint8_t)m->sel, &cx, &cy, &w, &d, &sh)) { v->has_center = true; v->cx = cx; v->cy = cy; }
  }
}

static bool marker_of(const arq_obj_model_t *m, const ArqView &v, uint8_t i, ArqMarker *mk) {
  int32_t cx, cy, w, d; char sh = 'R';
  if (!obj_box(m, i, &cx, &cy, &w, &d, &sh)) return false;
  arq_marker(cx, cy, w, d, sh, m->bed, v, mk);
  return true;
}

uint32_t arq_objects_sign(const arq_obj_model_t *m) {
  uint32_t h = 2166136261UL;
  auto mix = [&h](uint32_t v) { h ^= v; h *= 16777619UL; };
  const uint8_t n = count_of(m);
  mix(n); mix(m->total); mix((uint8_t)m->active); mix((uint8_t)m->sel); mix(m->page); mix(m->zoom); mix(m->action); mix(m->source);
  mix((uint32_t)m->bed.min_x); mix((uint32_t)m->bed.min_y); mix((uint32_t)m->bed.w); mix((uint32_t)m->bed.d); mix((uint32_t)(uintptr_t)m->note);
  for (uint8_t i = 0; i < n; ++i) {
    mix(m->state[i]);
    int32_t cx, cy, w, d; char sh;
    if (obj_box(m, i, &cx, &cy, &w, &d, &sh)) { mix((uint32_t)cx); mix((uint32_t)cy); mix((uint32_t)w); mix((uint32_t)d); mix((uint8_t)sh); } else mix(0xFFFFFFFFUL);
  }
  return h;
}

// true = hay que dibujar (solo en LV_DESIGN_DRAW_MAIN y si el area visible no esta vacia)
static bool prep(lv_obj_t *o, const lv_area_t *mask_in, lv_design_mode_t mode, lv_area_t *a, lv_area_t *clip) {
  if (mode != LV_DESIGN_DRAW_MAIN) return false;
  lv_obj_get_coords(o, a);
  return lv_area_intersect(clip, mask_in, a);
}

static bool area_visible(const lv_area_t &r, const lv_area_t *mask) {
  return r.x1 <= mask->x2 && r.x2 >= mask->x1 && r.y1 <= mask->y2 && r.y2 >= mask->y1;
}

// ---- Mapa de la cama -------------------------------------------------------------------------------------------------------------
static void draw_marker(const lv_area_t *mask, const lv_area_t &origin, const ArqMarker &mk, uint8_t i, uint8_t state, bool selected) {
  const lv_area_t r = { (lv_coord_t)(origin.x1 + mk.x0), (lv_coord_t)(origin.y1 + mk.y0), (lv_coord_t)(origin.x1 + mk.x1), (lv_coord_t)(origin.y1 + mk.y1) };
  if (!area_visible(r, mask)) return;
  lv_draw_rect(&r, mask, &st_box[state], LV_OPA_COVER);
  if (selected) lv_draw_rect(&r, mask, &st_ring, LV_OPA_COVER);
  char t[8];
  const bool wide = (mk.x1 - mk.x0 + 1) >= 34;                     // cabe la insignia junto al numero
  if (!wide && (state == ARQ_ST_CANCELED || state == ARQ_ST_UNAVAILABLE)) snprintf(t, sizeof(t), "%c", STATE_BADGE[state]);     // marcador pequeno: la insignia sustituye al numero
  else if (wide && STATE_BADGE[state]) snprintf(t, sizeof(t), "%d%c", i + 1, STATE_BADGE[state]);
  else snprintf(t, sizeof(t), "%d", i + 1);
  draw_text_c(mask, r, t, arq_style_text(ARQ_TEXT_BODY));
}

static bool map_design(lv_obj_t *o, const lv_area_t *mask_in, lv_design_mode_t mode) {
  if (mode == LV_DESIGN_COVER_CHK) return false;           // esquinas redondeadas: no cubre todo el area
  lv_area_t a, clip;
  if (!prep(o, mask_in, mode, &a, &clip)) return true;
  const lv_area_t *mask = &clip;
  lv_draw_rect(&a, mask, arq_style_panel(), LV_OPA_COVER);
  if (!valid || !M) return true;
  const uint8_t n = count_of(M);

  if (n == 0) {
    const lv_coord_t cx = a.x1 + MAP_SZ / 2, cy = a.y1 + MAP_SZ / 2 - 30;
    draw_text_cx(mask, cx, cy,      "Sin objetos",                     arq_style_text(ARQ_TEXT_BODY));
    draw_text_cx(mask, cx, cy + 26, "El G-code no define objetos",     arq_style_text(ARQ_TEXT_MUTED));
    draw_text_cx(mask, cx, cy + 46, "(etiquetas M486)",                arq_style_text(ARQ_TEXT_MUTED));
    return true;
  }

  ArqView v; map_view(M, &v);
  int32_t bx0, by1, bx1, by0;                                          // cama: esquina inferior izquierda y superior derecha en pantalla
  arq_to_px(M->bed.min_x, M->bed.min_y, M->bed, v, &bx0, &by1);
  arq_to_px(M->bed.min_x + M->bed.w, M->bed.min_y + M->bed.d, M->bed, v, &bx1, &by0);
  const lv_area_t bed = { (lv_coord_t)(a.x1 + bx0), (lv_coord_t)(a.y1 + by0), (lv_coord_t)(a.x1 + bx1), (lv_coord_t)(a.y1 + by1) };
  lv_draw_rect(&bed, mask, &st_bed, LV_OPA_COVER);
  if (bx0 >= -2 && by1 <= MAP_SZ + 2) draw_text(mask, bed.x1 + 4, bed.y2 - 20, "X0 Y0", arq_style_text(ARQ_TEXT_MUTED));   // origen del G-code en esta vista

  uint8_t found = 0;
  for (uint8_t i = 0; i < n; ++i) {
    ArqMarker mk;
    if (!marker_of(M, v, i, &mk)) continue;
    ++found;
    if (i != M->sel) draw_marker(mask, a, mk, i, state_of(M, i), false);
  }
  if (M->sel >= 0 && M->sel < n) {                                      // el seleccionado va encima de los demas
    ArqMarker mk;
    if (marker_of(M, v, (uint8_t)M->sel, &mk)) draw_marker(mask, a, mk, (uint8_t)M->sel, state_of(M, (uint8_t)M->sel), true);
  }
  if (!found) {
    const lv_coord_t cx = a.x1 + MAP_SZ / 2, cy = a.y1 + MAP_SZ / 2 - 20;
    draw_text_cx(mask, cx, cy,      "Las posiciones aparecen", arq_style_text(ARQ_TEXT_MUTED));
    draw_text_cx(mask, cx, cy + 20, "al imprimir cada objeto", arq_style_text(ARQ_TEXT_MUTED));
  }
  if (M->note) {                                                        // aviso fijo (p. ej. cancelacion bloqueada): arriba del mapa, sobre un fondo para que se lea
    const lv_coord_t w = text_w(M->note, arq_style_text(ARQ_TEXT_WARN)) + 12;
    const lv_area_t nb = { (lv_coord_t)(a.x1 + (MAP_SZ - w) / 2), (lv_coord_t)(a.y1 + 4), (lv_coord_t)(a.x1 + (MAP_SZ + w) / 2), (lv_coord_t)(a.y1 + 26) };
    lv_draw_rect(&nb, mask, &st_btn, LV_OPA_COVER);
    draw_text_c(mask, nb, M->note, arq_style_text(ARQ_TEXT_WARN));
  }
  return true;
}

// ---- Lista (virtualizada: solo se dibujan las filas de la pagina) + paginacion + zoom + informacion -----------------------------------
static void draw_btn(const lv_area_t *mask, const lv_area_t &r, const char *t, bool active, bool enabled) {
  lv_draw_rect(&r, mask, active ? &st_row_sel : &st_btn, LV_OPA_COVER);
  draw_text_c(mask, r, t, arq_style_text(enabled ? ARQ_TEXT_BODY : ARQ_TEXT_MUTED));
}

static bool grid_design(lv_obj_t *o, const lv_area_t *mask_in, lv_design_mode_t mode) {
  if (mode == LV_DESIGN_COVER_CHK) return false;           // esquinas redondeadas: no cubre todo el area
  lv_area_t a, clip;
  if (!prep(o, mask_in, mode, &a, &clip)) return true;
  const lv_area_t *mask = &clip;
  if (!valid || !M) return true;
  const uint8_t n = count_of(M), pages = pages_of(M), page = page_of(M);
  char t[40];

  // filas: [punto de color] [numero] [nombre] [insignia]
  for (uint8_t r = 0; r < ARQ_OBJ_ROWS; ++r) {
    const uint8_t i = (uint8_t)(page * ARQ_OBJ_ROWS + r);
    if (i >= n) break;
    const lv_coord_t y = a.y1 + r * ROW_H;
    const lv_area_t row = { a.x1, y, (lv_coord_t)(a.x1 + GRID_W - 1), (lv_coord_t)(y + ROW_H - 2) };
    const uint8_t st = state_of(M, i);
    lv_draw_rect(&row, mask, i == M->sel ? &st_row_sel : &st_btn, LV_OPA_COVER);
    const lv_area_t dot = { (lv_coord_t)(a.x1 + 6), (lv_coord_t)(y + 9), (lv_coord_t)(a.x1 + 13), (lv_coord_t)(y + 16) };
    lv_draw_rect(&dot, mask, arq_style_dot(STATE_TINT[st]), LV_OPA_COVER);
    snprintf(t, sizeof(t), "%d", i + 1);
    draw_text(mask, a.x1 + 18, y + 4, t, arq_style_text(ARQ_TEXT_BODY));
    if (M->src && M->src->label) M->src->label(i, t, 24); else snprintf(t, sizeof(t), "Objeto %d", i + 1);
    fit_text(t, 24, 112, arq_style_text(ARQ_TEXT_BODY));
    draw_text(mask, a.x1 + 44, y + 4, t, arq_style_text(ARQ_TEXT_BODY));
    if (STATE_BADGE[st]) { char b[2] = { STATE_BADGE[st], 0 }; draw_text(mask, a.x1 + GRID_W - 16, y + 4, b, arq_style_text(ARQ_TEXT_BODY)); }
  }

  // paginacion [<] [p/P] [>] y zoom [x1] [x2] [x4]
  const lv_coord_t ny = a.y1 + NAV_Y;
  const lv_area_t prev = { a.x1, ny, (lv_coord_t)(a.x1 + BTN_ARROW_W - 1), (lv_coord_t)(ny + NAV_H - 1) };
  const lv_area_t next = { (lv_coord_t)(a.x1 + BTN_ARROW_W + PAGE_W), ny, (lv_coord_t)(a.x1 + 2 * BTN_ARROW_W + PAGE_W - 1), (lv_coord_t)(ny + NAV_H - 1) };
  const lv_area_t pg   = { (lv_coord_t)(a.x1 + BTN_ARROW_W), ny, (lv_coord_t)(a.x1 + BTN_ARROW_W + PAGE_W - 1), (lv_coord_t)(ny + NAV_H - 1) };
  draw_btn(mask, prev, "<", false, page > 0);
  draw_btn(mask, next, ">", false, page + 1 < pages);
  snprintf(t, sizeof(t), "%d/%d", page + 1, pages);
  draw_text_c(mask, pg, t, arq_style_text(ARQ_TEXT_MUTED));
  static const char *const zl[3] = { "x1", "x2", "x4" };
  for (uint8_t k = 0; k < 3; ++k) {
    const lv_area_t z = { (lv_coord_t)(a.x1 + ZOOM_X + k * ZOOM_W), ny, (lv_coord_t)(a.x1 + ZOOM_X + (k + 1) * ZOOM_W - 2), (lv_coord_t)(ny + NAV_H - 1) };
    draw_btn(mask, z, zl[k], zoom_of(M) == (1 << k), true);
  }

  // informacion: objeto elegido (nombre y estado, con texto e insignia) o resumen; debajo, la nota (p. ej. cancelacion bloqueada)
  const lv_coord_t ly = a.y1 + INFO_Y;
  if (M->sel >= 0 && M->sel < n) {
    const uint8_t st = state_of(M, (uint8_t)M->sel);
    char nm[24];
    if (M->src && M->src->label) M->src->label((uint8_t)M->sel, nm, sizeof(nm)); else snprintf(nm, sizeof(nm), "Objeto %d", M->sel + 1);
    snprintf(t, sizeof(t), "%d %s", M->sel + 1, nm);
    fit_text(t, sizeof(t), GRID_W, arq_style_text(ARQ_TEXT_BODY));
    draw_text(mask, a.x1 + 2, ly, t, arq_style_text(ARQ_TEXT_BODY));
    if (STATE_BADGE[st]) snprintf(t, sizeof(t), "%s  %c", STATE_WORD[st], STATE_BADGE[st]); else snprintf(t, sizeof(t), "%s", STATE_WORD[st]);
    draw_text(mask, a.x1 + 2, ly + 17, t, arq_style_text(st == ARQ_ST_CANCELED ? ARQ_TEXT_WARN : ARQ_TEXT_MUTED));
  }
  else if (n == 0) draw_text(mask, a.x1 + 2, ly, "Sin objetos", arq_style_text(ARQ_TEXT_MUTED));
  else {
    if (M->total > n) snprintf(t, sizeof(t), "Se muestran %d de %d", n, M->total); else snprintf(t, sizeof(t), "Toca un objeto (%d)", n);
    draw_text(mask, a.x1 + 2, ly, t, arq_style_text(ARQ_TEXT_MUTED));
  }
  return true;
}

// ---- API ---------------------------------------------------------------------------------------------------------------------------
void arq_objects_forget() { o_map = o_grid = o_act = nullptr; header.bar = header.back = nullptr; valid = false; M = nullptr; last_sig = 0; }

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
  if (!m) return;
  const uint32_t sig = arq_objects_sign(m);
  if (valid && M == m && sig == last_sig) return;       // nada cambio: no se toca LVGL
  M = m; last_sig = sig; valid = true;
  if (o_map)  lv_obj_invalidate(o_map);
  if (o_grid) lv_obj_invalidate(o_grid);
  if (!o_act) return;
  const uint8_t a = M->action;
  arq_card_set_disabled(o_act, !(a == ARQ_ACT_CANCEL || a == ARQ_ACT_RESTORE));
  arq_card_set_kind(o_act, a == ARQ_ACT_RESTORE ? ARQ_BTN_SUCCESS : (a == ARQ_ACT_LOCKED ? ARQ_BTN_SECONDARY : ARQ_BTN_DANGER));
  arq_card_set_title(o_act, a == ARQ_ACT_RESTORE ? "Restaurar objeto" : (a == ARQ_ACT_LOCKED ? "Cancelaci\xC3\xB3n bloqueada" : "Cancelar objeto"), ARQ_TEXT_BODY);
}

int8_t arq_objects_pick(lv_obj_t *o, const lv_point_t *p) {
  if (!o || !p || !valid || !M) return ARQ_PICK_NONE;
  lv_area_t a; lv_obj_get_coords(o, &a);
  const lv_coord_t rx = p->x - a.x1, ry = p->y - a.y1;
  const uint8_t n = count_of(M);
  if (o == o_map) {
    ArqView v; map_view(M, &v);
    int16_t best_id = -1; ArqMarker best = {0, 0, 0, 0, 0, 0};
    for (uint8_t i = 0; i < n; ++i) {
      ArqMarker mk;
      if (marker_of(M, v, i, &mk) && arq_marker_hit(mk, rx, ry) && arq_marker_beats(mk, i, best, best_id)) { best = mk; best_id = i; }   // gana el de menor area
    }
    return (int8_t)best_id;
  }
  if (o == o_grid) {
    if (rx < 0 || rx >= GRID_W || ry < 0) return ARQ_PICK_NONE;
    if (ry < ARQ_OBJ_ROWS * ROW_H) {
      const uint8_t i = (uint8_t)(page_of(M) * ARQ_OBJ_ROWS + ry / ROW_H);
      return (ry % ROW_H < ROW_H - 1 && i < n) ? (int8_t)i : (int8_t)ARQ_PICK_NONE;
    }
    if (ry >= NAV_Y && ry < NAV_Y + NAV_H) {
      if (rx < BTN_ARROW_W) return page_of(M) > 0 ? ARQ_PICK_PAGE_PREV : ARQ_PICK_NONE;
      if (rx >= BTN_ARROW_W + PAGE_W && rx < 2 * BTN_ARROW_W + PAGE_W) return page_of(M) + 1 < pages_of(M) ? ARQ_PICK_PAGE_NEXT : ARQ_PICK_NONE;
      if (rx >= ZOOM_X && rx < ZOOM_X + 3 * ZOOM_W - 2) {
        const int k = (rx - ZOOM_X) / ZOOM_W;
        return k == 0 ? ARQ_PICK_ZOOM1 : (k == 1 ? ARQ_PICK_ZOOM2 : ARQ_PICK_ZOOM4);
      }
    }
  }
  return ARQ_PICK_NONE;
}
