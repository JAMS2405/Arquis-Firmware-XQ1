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

#include "arquis_widgets.h"

#include <stdio.h>
#include <string.h>

typedef struct {
  const lv_img_dsc_t *icon;
  const char *title, *value, *value2, *sub;
  arq_fan_row_t *fans;
  arq_list_row_t *rows;
  lv_event_cb_t user_cb;
  uint8_t layout, kind, icon_tint, title_role, value_role, sub_role, dot_tint, bar_tint, flags, pct, nfans, nrows;
} arq_card_ext_t;

#define F_PRESSED  0x01
#define F_LOGO     0x02
#define F_DISABLED 0x04

static bool focus_ring_visible;   // el anillo de foco solo se ve cuando la perilla mueve el foco (con uso tactil no debe verse)

static inline arq_card_ext_t * ext(lv_obj_t *o) { return (arq_card_ext_t *)lv_obj_get_ext_attr(o); }

void arq_screen_apply(lv_obj_t *scr) {
  arq_theme_init();
  lv_obj_set_style(scr, arq_style_screen());
}

void arq_group_add_clickables(lv_group_t *g, lv_obj_t *scr) {
  for (lv_obj_t *c = lv_obj_get_child_back(scr, nullptr); c; c = lv_obj_get_child_back(scr, c))
    if (lv_obj_get_click(c) && ext(c) && ext(c)->user_cb) lv_group_add_obj(g, c);
  focus_ring_visible = false;   // el foco inicial no se dibuja hasta que la perilla se mueva
}

// ---- Dibujo ------------------------------------------------------------------------------------------------
static void text_size(lv_point_t *sz, const char *t, const lv_style_t *st) {
  lv_txt_get_size(sz, t, st->text.font, st->text.letter_space, st->text.line_space, LV_COORD_MAX, LV_TXT_FLAG_NONE);
}

static void draw_text(const lv_area_t *mask, lv_coord_t x, lv_coord_t y, const char *t, const lv_style_t *st) {
  lv_point_t sz; text_size(&sz, t, st);
  lv_area_t a = { x, y, (lv_coord_t)(x + sz.x), (lv_coord_t)(y + sz.y) };
  lv_draw_label(&a, mask, st, LV_OPA_COVER, t, LV_TXT_FLAG_NONE, nullptr, nullptr, nullptr, LV_BIDI_DIR_LTR);
}

// Texto de una linea que se recorta con "..." si no cabe en maxw (nombres de archivo largos).
static void draw_text_fit(const lv_area_t *mask, lv_coord_t x, lv_coord_t y, lv_coord_t maxw, const char *t, const lv_style_t *st) {
  lv_point_t sz; text_size(&sz, t, st);
  if (sz.x <= maxw) { draw_text(mask, x, y, t, st); return; }
  char tmp[48];
  size_t n = strlen(t); if (n > 43) n = 43;
  while (n > 1) {
    while (n > 1 && (t[n] & 0xC0) == 0x80) --n;                // no cortar a mitad de un caracter UTF-8
    memcpy(tmp, t, n); strcpy(tmp + n, "...");
    text_size(&sz, tmp, st);
    if (sz.x <= maxw) break;
    --n;
  }
  draw_text(mask, x, y, tmp, st);
}

static void draw_icon(const lv_area_t *mask, lv_coord_t x, lv_coord_t y, const lv_img_dsc_t *icon, const lv_style_t *st) {
  lv_area_t a = { x, y, (lv_coord_t)(x + icon->header.w - 1), (lv_coord_t)(y + icon->header.h - 1) };
  lv_draw_img(&a, mask, icon, st, LV_OPA_COVER);
}

static void draw_dot(const lv_area_t *mask, lv_coord_t cx, lv_coord_t cy, lv_coord_t r, arq_tint_t tint) {
  lv_area_t a = { (lv_coord_t)(cx - r), (lv_coord_t)(cy - r), (lv_coord_t)(cx + r - 1), (lv_coord_t)(cy + r - 1) };
  lv_draw_rect(&a, mask, arq_style_dot(tint), LV_OPA_COVER);
}

static void draw_bar(const lv_area_t *mask, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h, uint8_t pct, arq_tint_t tint) {
  lv_area_t a = { x, y, (lv_coord_t)(x + w - 1), (lv_coord_t)(y + h - 1) };
  lv_draw_rect(&a, mask, arq_style_bar_bg(), LV_OPA_COVER);
  if (!pct) return;
  a.x2 = x + (lv_coord_t)((int32_t)w * pct / 100) - 1;
  if (a.x2 < x + h - 1) a.x2 = x + h - 1;                       // ancho minimo: que el extremo redondeado se vea
  lv_draw_rect(&a, mask, tint == ARQ_TINT_AUTO ? arq_style_bar_indic() : arq_style_dot(tint), LV_OPA_COVER);
}

static void draw_stat(const lv_area_t *mask, const arq_card_ext_t *e, const lv_area_t &a, const lv_style_t *icon_st, bool pressed) {
  const lv_coord_t w = lv_area_get_width(&a), h = lv_area_get_height(&a), x0 = a.x1, y0 = a.y1;
  lv_point_t ts;
  lv_draw_rect(&a, mask, pressed ? arq_style_panel_pressed() : arq_style_panel(), LV_OPA_COVER);
  if (e->icon) draw_icon(mask, x0 + 10, y0 + 10, e->icon, icon_st);
  if (e->title) {
    const lv_style_t *st = arq_style_text((arq_text_t)e->title_role);
    text_size(&ts, e->title, st);
    draw_text(mask, x0 + 58, y0 + 10 + (ARQ_ICON_SIZE - ts.y) / 2, e->title, st);
  }
  if (e->value) {
    const lv_style_t *st = arq_style_text((arq_text_t)e->value_role);
    lv_point_t vs; text_size(&vs, e->value, st);
    draw_text(mask, x0 + 12, y0 + 54, e->value, st);
  }
  if (e->sub) {
    const lv_style_t *st = arq_style_text((arq_text_t)e->sub_role);
    text_size(&ts, e->sub, st);
    draw_text(mask, x0 + 12, y0 + h - ts.y - 8, e->sub, st);
    if (e->value2 && e->value2[0]) {                      // dato secundario (objetivo) a la derecha de la fila inferior
      const lv_style_t *s2 = arq_style_text(ARQ_TEXT_BODY);
      lv_point_t t2; text_size(&t2, e->value2, s2);
      draw_text(mask, x0 + w - 12 - t2.x, y0 + h - t2.y - 8, e->value2, s2);
    }
  }
  if (e->dot_tint != ARQ_TINT_AUTO) draw_dot(mask, x0 + w - 17, y0 + 30, 5, (arq_tint_t)e->dot_tint);   // punto de estado: arriba a la derecha
}

static void draw_fans(const lv_area_t *mask, const arq_card_ext_t *e, const lv_area_t &a, bool pressed) {
  const lv_coord_t w = lv_area_get_width(&a), x0 = a.x1, y0 = a.y1;
  lv_point_t ts;
  lv_draw_rect(&a, mask, pressed ? arq_style_panel_pressed() : arq_style_panel(), LV_OPA_COVER);
  if (e->title) draw_text(mask, x0 + 12, y0 + 9, e->title, arq_style_text((arq_text_t)e->title_role));
  for (uint8_t i = 0; i < e->nfans && i < ARQ_FAN_ROWS_MAX; ++i) {
    const arq_fan_row_t &r = e->fans[i];
    const lv_coord_t ry = y0 + 36 + i * 28;
    const bool on = r.pct > 0;
    if (r.icon) draw_icon(mask, x0 + 12, ry + 1, r.icon, arq_style_icon(on ? ARQ_TINT_OK : ARQ_TINT_MUTED));
    draw_text(mask, x0 + 38, ry, r.label, arq_style_text(ARQ_TEXT_BODY));
    const lv_style_t *ps = arq_style_text(on ? ARQ_TEXT_OK : ARQ_TEXT_MUTED);
    text_size(&ts, r.txt, ps);
    draw_text(mask, x0 + w - 12 - ts.x, ry, r.txt, ps);
    draw_bar(mask, x0 + 12, ry + 22, w - 24, 3, r.pct, ARQ_TINT_OK);
  }
}

static void draw_print(const lv_area_t *mask, const arq_card_ext_t *e, const lv_area_t &a, const lv_style_t *btn) {
  const lv_coord_t w = lv_area_get_width(&a), h = lv_area_get_height(&a), x0 = a.x1, y0 = a.y1;
  lv_point_t ts;
  lv_draw_rect(&a, mask, btn, LV_OPA_COVER);
  if (e->title) draw_text_fit(mask, x0 + 12, y0 + 8, w - 24, e->title, arq_style_text(ARQ_TEXT_BODY));
  draw_bar(mask, x0 + 12, y0 + 34, w - 24, 8, e->pct, (arq_tint_t)e->bar_tint);
  lv_point_t vs = { 0, 0 };
  if (e->value) {
    const lv_style_t *st = arq_style_text(ARQ_TEXT_NUM_L);
    text_size(&vs, e->value, st);
    draw_text(mask, x0 + 12, y0 + 46, e->value, st);
  }
  if (e->sub) {
    const lv_style_t *st = arq_style_text((arq_text_t)e->sub_role);
    text_size(&ts, e->sub, st);
    draw_text_fit(mask, x0 + 12 + vs.x + 10, y0 + 46 + (vs.y - ts.y) / 2, w - 24 - vs.x - 10, e->sub, st);
  }
  if (e->value2) {
    const lv_style_t *st = arq_style_text(ARQ_TEXT_MUTED);
    text_size(&ts, e->value2, st);
    draw_text_fit(mask, x0 + 12, y0 + h - ts.y - 8, w - 24, e->value2, st);
  }
}

// Supervision de impresion: archivo, barra, porcentaje grande y dos lineas de tiempo a su derecha.
static void draw_job(const lv_area_t *mask, const arq_card_ext_t *e, const lv_area_t &a) {
  const lv_coord_t w = lv_area_get_width(&a), x0 = a.x1, y0 = a.y1;
  lv_draw_rect(&a, mask, arq_style_panel(), LV_OPA_COVER);
  if (e->title) draw_text_fit(mask, x0 + 12, y0 + 8, w - 24, e->title, arq_style_text(ARQ_TEXT_BODY));
  draw_bar(mask, x0 + 12, y0 + 34, w - 24, 8, e->pct, (arq_tint_t)e->bar_tint);
  if (e->value) draw_text(mask, x0 + 12, y0 + 47, e->value, arq_style_text(e->value_role >= ARQ_TEXT_NUM_L ? (arq_text_t)e->value_role : ARQ_TEXT_NUM_L));
  const lv_coord_t cx = x0 + 12 + 108, cw = w - 12 - 108 - 12;
  if (e->value2) draw_text_fit(mask, cx, y0 + 50, cw, e->value2, arq_style_text(ARQ_TEXT_BODY));
  if (e->sub)    draw_text_fit(mask, cx, y0 + 72, cw, e->sub, arq_style_text((arq_text_t)e->sub_role));
}

// Lista de datos: [punto] nombre ................ valor / objetivo
static void draw_list(const lv_area_t *mask, const arq_card_ext_t *e, const lv_area_t &a) {
  const lv_coord_t w = lv_area_get_width(&a), x0 = a.x1, y0 = a.y1;
  lv_point_t t1, t2;
  lv_draw_rect(&a, mask, arq_style_panel(), LV_OPA_COVER);
  for (uint8_t i = 0; i < e->nrows && i < ARQ_LIST_ROWS_MAX; ++i) {
    const arq_list_row_t &r = e->rows[i];
    const lv_coord_t ry = y0 + ARQ_LIST_PAD + i * ARQ_LIST_ROW_H;
    if (r.dot != ARQ_TINT_AUTO) draw_dot(mask, x0 + 17, ry + 9, 4, (arq_tint_t)r.dot);
    draw_text(mask, x0 + 32, ry, r.label, arq_style_text(ARQ_TEXT_MUTED));
    lv_coord_t rx = x0 + w - 14;
    if (r.v2[0]) { text_size(&t2, r.v2, arq_style_text(ARQ_TEXT_MUTED)); rx -= t2.x; draw_text(mask, rx, ry, r.v2, arq_style_text(ARQ_TEXT_MUTED)); rx -= 6; }
    text_size(&t1, r.v1, arq_style_text(ARQ_TEXT_BODY));
    draw_text(mask, rx - t1.x, ry, r.v1, arq_style_text(ARQ_TEXT_BODY));
  }
}

static bool card_design(lv_obj_t *o, const lv_area_t *mask_in, lv_design_mode_t mode) {
  if (mode == LV_DESIGN_COVER_CHK) return false;        // esquinas redondeadas: no cubre todo el area
  if (mode != LV_DESIGN_DRAW_MAIN) return true;

  const arq_card_ext_t *e = ext(o);
  lv_area_t a; lv_obj_get_coords(o, &a);
  lv_area_t clip;
  if (!lv_area_intersect(&clip, mask_in, &a)) return true;   // todo se recorta al area de la tarjeta
  const lv_area_t *mask = &clip;
  const lv_coord_t w = lv_area_get_width(&a), h = lv_area_get_height(&a), x0 = a.x1, y0 = a.y1;
  const arq_btn_kind_t kind = (arq_btn_kind_t)e->kind;
  const bool disabled = e->flags & F_DISABLED, pressed = !disabled && (e->flags & F_PRESSED);
  const lv_style_t *btn = arq_style_btn(kind, disabled ? LV_BTN_STATE_INA : pressed ? LV_BTN_STATE_PR : LV_BTN_STATE_REL);
  const lv_style_t *icon_st = disabled ? arq_style_icon(ARQ_TINT_MUTED) : e->icon_tint == ARQ_TINT_AUTO ? btn : arq_style_icon((arq_tint_t)e->icon_tint);
  lv_point_t ts = { 0, 0 };

  switch (e->layout) {
    case ARQ_LAYOUT_TILE: {
      lv_draw_rect(&a, mask, btn, LV_OPA_COVER);
      if (e->title) text_size(&ts, e->title, btn);
      const lv_coord_t gap = 4, total = (e->icon ? ARQ_ICON_SIZE + gap : 0) + ts.y, top = y0 + (h - total) / 2;
      if (e->icon) draw_icon(mask, x0 + (w - ARQ_ICON_SIZE) / 2, top, e->icon, icon_st);
      if (e->title) draw_text_fit(mask, x0 + (w - ts.x) / 2 < x0 + 2 ? x0 + 2 : x0 + (w - ts.x) / 2, top + (e->icon ? ARQ_ICON_SIZE + gap : 0), w - 4, e->title, btn);
    } break;

    case ARQ_LAYOUT_BUTTON: {
      lv_draw_rect(&a, mask, btn, LV_OPA_COVER);
      if (e->title) text_size(&ts, e->title, btn);
      const lv_coord_t gap = 8, total = (e->icon ? ARQ_ICON_SIZE : 0) + (e->icon && e->title ? gap : 0) + ts.x, left = x0 + (w - total) / 2;
      if (e->icon) draw_icon(mask, left, y0 + (h - ARQ_ICON_SIZE) / 2, e->icon, icon_st);
      if (e->title) draw_text(mask, left + (e->icon ? ARQ_ICON_SIZE + gap : 0), y0 + (h - ts.y) / 2, e->title, btn);
    } break;

    case ARQ_LAYOUT_STAT: draw_stat(mask, e, a, icon_st, pressed); break;
    case ARQ_LAYOUT_FANS: draw_fans(mask, e, a, pressed); break;
    case ARQ_LAYOUT_PRINT: draw_print(mask, e, a, btn); break;
    case ARQ_LAYOUT_JOB: draw_job(mask, e, a); break;
    case ARQ_LAYOUT_LIST: draw_list(mask, e, a); break;

    case ARQ_LAYOUT_CHIP: {
      lv_draw_rect(&a, mask, arq_style_chip(), LV_OPA_COVER);
      if (e->dot_tint != ARQ_TINT_AUTO) draw_dot(mask, x0 + 16, y0 + h / 2, 4, (arq_tint_t)e->dot_tint);
      if (e->title) {
        const lv_style_t *st = arq_style_text((arq_text_t)e->title_role);
        text_size(&ts, e->title, st);
        draw_text(mask, x0 + 28, y0 + (h - ts.y) / 2, e->title, st);
      }
    } break;

    case ARQ_LAYOUT_HEADER: {
      lv_draw_rect(&a, mask, arq_style_header(), LV_OPA_COVER);
      lv_coord_t tx = e->icon ? ARQ_HEADER_H + 8 : ARQ_MARGIN + 2;
      if (e->flags & F_LOGO) {
        const lv_coord_t ly = y0 + (h - 32) / 2;
        draw_icon(mask, x0 + ARQ_MARGIN, ly, &arq_logo32_blue, arq_style_icon(ARQ_TINT_ACCENT));
        draw_icon(mask, x0 + ARQ_MARGIN, ly, &arq_logo32_green, arq_style_icon(ARQ_TINT_OK));
        tx = ARQ_MARGIN + 32 + 10;
      }
      if (e->title) {
        const lv_style_t *st = arq_style_text((arq_text_t)e->title_role);
        text_size(&ts, e->title, st);
        draw_text(mask, x0 + tx, y0 + (h - ts.y) / 2, e->title, st);
      }
    } break;

    case ARQ_LAYOUT_BAR: draw_bar(mask, x0, y0, w, h, e->pct, (arq_tint_t)e->bar_tint); break;

    case ARQ_LAYOUT_DIALOG: {
      lv_draw_rect(&a, mask, arq_style_panel(), LV_OPA_COVER);
      if (e->title) draw_text(mask, x0 + 16, y0 + 14, e->title, arq_style_text(ARQ_TEXT_BODY));
      if (e->sub) {
        lv_area_t m = { (lv_coord_t)(x0 + 16), (lv_coord_t)(y0 + 44), (lv_coord_t)(x0 + w - 17), (lv_coord_t)(y0 + h - 70) };
        lv_draw_label(&m, mask, arq_style_text(ARQ_TEXT_MUTED), LV_OPA_COVER, e->sub, LV_TXT_FLAG_NONE, nullptr, nullptr, nullptr, LV_BIDI_DIR_LTR);
      }
    } break;
  }
  // Foco de la perilla (encoder): anillo azul sobre las tarjetas clicables
  lv_group_t *grp = (lv_group_t *)lv_obj_get_group(o);
  if (focus_ring_visible && grp && e->user_cb && lv_group_get_focused(grp) == o) lv_draw_rect(&a, mask, arq_style_focus(), LV_OPA_COVER);
  return true;
}

// Reenvia los eventos al callback de la pantalla; marca el estado "pulsado" para el aspecto del boton.
static void card_event(lv_obj_t *o, lv_event_t ev) {
  arq_card_ext_t *e = ext(o);
  if (ev == LV_EVENT_FOCUSED) focus_ring_visible = true;
  if (ev == LV_EVENT_FOCUSED || ev == LV_EVENT_DEFOCUSED) lv_obj_invalidate(o);
  if (ev == LV_EVENT_PRESSED || ev == LV_EVENT_RELEASED || ev == LV_EVENT_PRESS_LOST) {
    const bool p = (ev == LV_EVENT_PRESSED);
    if (p != bool(e->flags & F_PRESSED)) { e->flags = p ? (e->flags | F_PRESSED) : (e->flags & ~F_PRESSED); lv_obj_invalidate(o); }
  }
  if (e->user_cb) e->user_cb(o, ev);
}

lv_obj_t * arq_card_create(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                           arq_layout_t layout, arq_btn_kind_t kind, lv_event_cb_t cb, uint16_t id) {
  arq_theme_init();
  lv_obj_t *o = lv_obj_create(parent, nullptr);
  if (!o) return nullptr;                                  // heap de LVGL agotado: sin objeto, sin caida
  arq_card_ext_t *e = (arq_card_ext_t *)lv_obj_allocate_ext_attr(o, sizeof(arq_card_ext_t));
  if (!e) { lv_obj_del(o); return nullptr; }
  *e = arq_card_ext_t();
  e->layout = layout; e->kind = kind; e->icon_tint = ARQ_TINT_AUTO; e->dot_tint = ARQ_TINT_AUTO; e->bar_tint = ARQ_TINT_AUTO;
  e->title_role = e->value_role = e->sub_role = ARQ_TEXT_BODY;
  e->user_cb = cb;
  lv_obj_set_pos(o, x, y);
  lv_obj_set_size(o, w, h);
  lv_obj_set_design_cb(o, card_design);
  lv_obj_set_click(o, cb != nullptr);
  if (cb) lv_obj_set_event_cb_mks(o, card_event, id, "", 0);
  return o;
}

template<typename T> static void upd(lv_obj_t *c, T &field, T v) { if (field != v) { field = v; lv_obj_invalidate(c); } }
#define NN(c) if (!(c)) return;   // los componentes pueden ser nullptr si el heap se agoto

void arq_card_set_icon(lv_obj_t *c, const lv_img_dsc_t *icon, arq_tint_t tint) { NN(c) arq_card_ext_t *e = ext(c); upd(c, e->icon, icon); upd<uint8_t>(c, e->icon_tint, tint); }
void arq_card_set_title(lv_obj_t *c, const char *t, arq_text_t r) { NN(c) arq_card_ext_t *e = ext(c); upd(c, e->title, t); upd<uint8_t>(c, e->title_role, r); }
void arq_card_set_value(lv_obj_t *c, const char *t, arq_text_t r) { NN(c) arq_card_ext_t *e = ext(c); upd(c, e->value, t); upd<uint8_t>(c, e->value_role, r); }
void arq_card_set_value2(lv_obj_t *c, const char *t)              { NN(c) upd(c, ext(c)->value2, t); }
void arq_card_set_sub(lv_obj_t *c, const char *t, arq_text_t r)   { NN(c) arq_card_ext_t *e = ext(c); upd(c, e->sub, t); upd<uint8_t>(c, e->sub_role, r); }
void arq_card_set_dot(lv_obj_t *c, arq_tint_t t)                  { NN(c) upd<uint8_t>(c, ext(c)->dot_tint, t); }
void arq_card_set_percent(lv_obj_t *c, uint8_t p)                 { NN(c) upd<uint8_t>(c, ext(c)->pct, p > 100 ? 100 : p); }
void arq_card_set_bar_tint(lv_obj_t *c, arq_tint_t t)             { NN(c) upd<uint8_t>(c, ext(c)->bar_tint, t); }
void arq_card_set_logo(lv_obj_t *c, bool s)                       { NN(c) arq_card_ext_t *e = ext(c); const uint8_t f = s ? (e->flags | F_LOGO) : (e->flags & ~F_LOGO); upd<uint8_t>(c, e->flags, f); }
void arq_card_set_fans(lv_obj_t *c, arq_fan_row_t *rows, uint8_t n) { NN(c) arq_card_ext_t *e = ext(c); e->fans = rows; upd<uint8_t>(c, e->nfans, n); lv_obj_invalidate(c); }
void arq_card_set_rows(lv_obj_t *c, arq_list_row_t *rows, uint8_t n) { NN(c) arq_card_ext_t *e = ext(c); e->rows = rows; e->nrows = n; lv_obj_invalidate(c); }
void arq_card_refresh(lv_obj_t *c)                                { NN(c) lv_obj_invalidate(c); }
void arq_card_set_disabled(lv_obj_t *c, bool d) {
  NN(c)
  arq_card_ext_t *e = ext(c);
  const uint8_t f = d ? (e->flags | F_DISABLED) : (e->flags & ~F_DISABLED);
  if (f != e->flags) { e->flags = f; lv_obj_set_click(c, !d && e->user_cb); lv_obj_invalidate(c); }
}

// ---- Atajos ------------------------------------------------------------------------------------------------
lv_obj_t * arq_nav_tile_create(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h, const lv_img_dsc_t *icon,
                               arq_tint_t icon_tint, const char *text, arq_btn_kind_t kind, lv_event_cb_t cb, uint16_t id) {
  lv_obj_t *o = arq_card_create(parent, x, y, w, h, ARQ_LAYOUT_TILE, kind, cb, id);
  arq_card_set_icon(o, icon, icon_tint);
  arq_card_set_title(o, text, ARQ_TEXT_BODY);
  return o;
}

lv_obj_t * arq_button_create(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h, const lv_img_dsc_t *icon,
                             const char *text, arq_btn_kind_t kind, lv_event_cb_t cb, uint16_t id) {
  lv_obj_t *o = arq_card_create(parent, x, y, w, h, ARQ_LAYOUT_BUTTON, kind, cb, id);
  arq_card_set_icon(o, icon, ARQ_TINT_AUTO);
  arq_card_set_title(o, text, ARQ_TEXT_BODY);
  return o;
}

lv_obj_t * arq_chip_create(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, const char *text, arq_tint_t dot_tint) {
  lv_obj_t *o = arq_card_create(parent, x, y, w, 28, ARQ_LAYOUT_CHIP, ARQ_BTN_SECONDARY, nullptr, 0);
  arq_card_set_title(o, text, ARQ_TEXT_BODY);
  arq_card_set_dot(o, dot_tint);
  return o;
}

lv_obj_t * arq_progress_create(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h) {
  return arq_card_create(parent, x, y, w, h, ARQ_LAYOUT_BAR, ARQ_BTN_PRIMARY, nullptr, 0);
}

void arq_header_create(arq_header_t *hd, lv_obj_t *parent, const char *static_title, bool logo, lv_event_cb_t back_cb, uint16_t back_id) {
  hd->bar = arq_card_create(parent, 0, 0, ARQ_W, ARQ_HEADER_H, ARQ_LAYOUT_HEADER, ARQ_BTN_GHOST, nullptr, 0);
  arq_card_set_title(hd->bar, static_title, ARQ_TEXT_BODY);
  hd->back = nullptr;
  if (back_cb) {
    arq_card_set_icon(hd->bar, &arq_icon_back, ARQ_TINT_TEXT);      // (la flecha la dibuja el boton; el icono solo reserva espacio)
    hd->back = arq_card_create(parent, 0, 0, ARQ_HEADER_H + 8, ARQ_HEADER_H, ARQ_LAYOUT_BUTTON, ARQ_BTN_GHOST, back_cb, back_id);
    arq_card_set_icon(hd->back, &arq_icon_back, ARQ_TINT_TEXT);
  }
  else if (logo) arq_card_set_logo(hd->bar, true);
}

// ---- Tarjeta de temperatura ---------------------------------------------------------------------------------
void arq_temp_tile_create(arq_temp_tile_t *t, lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                          const lv_img_dsc_t *icon, const char *static_name, lv_event_cb_t cb, uint16_t id) {
  t->card = arq_card_create(parent, x, y, w, h, ARQ_LAYOUT_STAT, ARQ_BTN_SECONDARY, cb, id);
  t->cur = t->tgt = -32768; t->state = 255; t->vbuf[0] = t->tbuf[0] = '\0';
  arq_card_set_icon(t->card, icon, ARQ_TINT_ACCENT);
  arq_card_set_title(t->card, static_name, ARQ_TEXT_BODY);
  arq_card_set_value(t->card, t->vbuf, ARQ_TEXT_NUM_L);
  arq_card_set_value2(t->card, t->tbuf);
}

void arq_temp_tile_update(arq_temp_tile_t *t, int16_t cur, int16_t target, int16_t tol) {
  if (!t->card) return;
  const arq_temp_state_t st = arq_temp_state(cur, target, tol);
  if (cur == t->cur && target == t->tgt && st == t->state) return;     // nada cambio: no se toca LVGL
  t->cur = cur; t->tgt = target; t->state = st;
  snprintf(t->vbuf, sizeof(t->vbuf), "%d" "\xC2\xB0", cur);
  if (target > 0) snprintf(t->tbuf, sizeof(t->tbuf), "%d" "\xC2\xB0", target); else t->tbuf[0] = '\0';
  static const char *const word[] = { "Apagado", "Calentando", "Objetivo", "Enfriando" };
  static const arq_tint_t dot_tint[] = { ARQ_TINT_MUTED, ARQ_TINT_WARN, ARQ_TINT_OK, ARQ_TINT_ACCENT };
  static const arq_text_t val_role[] = { ARQ_TEXT_NUM_L, ARQ_TEXT_NUM_L_WARN, ARQ_TEXT_NUM_L_OK, ARQ_TEXT_NUM_L };
  arq_card_set_sub(t->card, word[st], ARQ_TEXT_MUTED);
  arq_card_set_dot(t->card, dot_tint[st]);
  arq_card_set_value(t->card, t->vbuf, val_role[st]);
  arq_card_refresh(t->card);                                            // el contenido de los buffers cambio
}

// ---- Dialogo de confirmacion -------------------------------------------------------------------------------
static lv_obj_t *dlg_scrim, *dlg_panel, *dlg_ok, *dlg_cancel;

static bool scrim_design(lv_obj_t *o, const lv_area_t *mask, lv_design_mode_t mode) {
  if (mode == LV_DESIGN_COVER_CHK) return false;
  if (mode == LV_DESIGN_DRAW_MAIN) { lv_area_t a; lv_obj_get_coords(o, &a); lv_draw_rect(&a, mask, arq_style_scrim(), LV_OPA_COVER); }
  return true;
}

lv_obj_t * arq_dialog_open(const char *title, const char *msg, const char *ok_text, const char *cancel_text, bool danger,
                           lv_event_cb_t cb, uint16_t ok_id, uint16_t cancel_id) {
  arq_theme_init();
  if (dlg_scrim) arq_dialog_close();
  lv_obj_t *top = lv_layer_top();
  dlg_scrim = lv_obj_create(top, nullptr);
  if (!dlg_scrim) return nullptr;
  lv_obj_set_size(dlg_scrim, ARQ_W, ARQ_H);
  lv_obj_set_pos(dlg_scrim, 0, 0);
  lv_obj_set_design_cb(dlg_scrim, scrim_design);
  lv_obj_set_click(dlg_scrim, true);                                  // absorbe toques fuera del dialogo
  const lv_coord_t pw = 380, ph = 176, px = (ARQ_W - pw) / 2, py = (ARQ_H - ph) / 2;
  dlg_panel = arq_card_create(top, px, py, pw, ph, ARQ_LAYOUT_DIALOG, ARQ_BTN_SECONDARY, nullptr, 0);
  arq_card_set_title(dlg_panel, title, ARQ_TEXT_BODY);
  arq_card_set_sub(dlg_panel, msg, ARQ_TEXT_MUTED);
  const lv_coord_t bw = (pw - 48) / 2, by = py + ph - 16 - 48;
  dlg_cancel = nullptr;
  if (cancel_text) {
    dlg_cancel = arq_button_create(top, px + 16, by, bw, 48, nullptr, cancel_text, ARQ_BTN_SECONDARY, cb, cancel_id);
    dlg_ok = arq_button_create(top, px + 32 + bw, by, bw, 48, nullptr, ok_text, danger ? ARQ_BTN_DANGER : ARQ_BTN_PRIMARY, cb, ok_id);
  }
  else   // informativo: un solo boton ancho
    dlg_ok = arq_button_create(top, px + 16, by, pw - 32, 48, nullptr, ok_text, danger ? ARQ_BTN_DANGER : ARQ_BTN_PRIMARY, cb, ok_id);
  if (!dlg_panel || !dlg_ok || (cancel_text && !dlg_cancel)) { arq_dialog_close(); return nullptr; }   // sin memoria: no queda un dialogo a medias
  return dlg_panel;
}

void arq_dialog_close() {
  if (dlg_ok)     lv_obj_del(dlg_ok);
  if (dlg_cancel) lv_obj_del(dlg_cancel);
  if (dlg_panel)  lv_obj_del(dlg_panel);
  if (dlg_scrim)  lv_obj_del(dlg_scrim);
  dlg_ok = dlg_cancel = dlg_panel = dlg_scrim = nullptr;
}

bool arq_dialog_is_open() { return dlg_scrim != nullptr; }
