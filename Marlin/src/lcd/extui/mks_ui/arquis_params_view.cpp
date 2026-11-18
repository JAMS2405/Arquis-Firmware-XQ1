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


#include "arquis_params_view.h"

#include <string.h>

// ---- Geometria ---------------------------------------------------------------------------------------------
#define LX      ARQ_MARGIN
#define LY      (ARQ_HEADER_H + 8)
#define LW      (ARQ_W - 2 * ARQ_MARGIN)
#define ROW_H   54
#define ROW_DY  62
#define VBW     104                 // caja de valor
#define VBH     34
#define PADX    16

static arq_header_t  hd;
static lv_obj_t     *o_list, *o_page;
static arq_pl_row_t  rows[ARQ_PL_ROWS];
static uint8_t       nrows, last_row;
static const char   *last_name;
static lv_event_cb_t user_cb;

// ---- Texto ------------------------------------------------------------------------------------------------
static lv_coord_t text_h(const lv_style_t *st) {
  lv_point_t sz;
  lv_txt_get_size(&sz, "A", st->text.font, st->text.letter_space, st->text.line_space, LV_COORD_MAX, LV_TXT_FLAG_NONE);
  return sz.y;
}

static void draw_fit(const lv_area_t *mask, lv_coord_t x, lv_coord_t y, lv_coord_t maxw, const char *t, const lv_style_t *st) {
  lv_point_t sz;
  lv_txt_get_size(&sz, t, st->text.font, st->text.letter_space, st->text.line_space, LV_COORD_MAX, LV_TXT_FLAG_NONE);
  char tmp[56];
  if (sz.x > maxw) {                                         // texto largo: se recorta con "..."
    size_t n = strlen(t); if (n > 50) n = 50;
    while (n > 1) {
      while (n > 1 && (t[n] & 0xC0) == 0x80) --n;           // no cortar a mitad de un caracter UTF-8
      memcpy(tmp, t, n); strcpy(tmp + n, "...");
      lv_txt_get_size(&sz, tmp, st->text.font, st->text.letter_space, st->text.line_space, LV_COORD_MAX, LV_TXT_FLAG_NONE);
      if (sz.x <= maxw) break;
      --n;
    }
    t = tmp;
  }
  lv_area_t a = { x, y, (lv_coord_t)(x + sz.x), (lv_coord_t)(y + sz.y) };
  lv_draw_label(&a, mask, st, LV_OPA_COVER, t, LV_TXT_FLAG_NONE, nullptr, nullptr, nullptr, LV_BIDI_DIR_LTR);
}

static void draw_cx(const lv_area_t *mask, const lv_area_t *box, const char *t, const lv_style_t *st) {
  lv_point_t sz;
  lv_txt_get_size(&sz, t, st->text.font, st->text.letter_space, st->text.line_space, LV_COORD_MAX, LV_TXT_FLAG_NONE);
  const lv_coord_t cx = (box->x1 + box->x2) / 2, cy = (box->y1 + box->y2) / 2;
  lv_area_t a = { (lv_coord_t)(cx - sz.x / 2), (lv_coord_t)(cy - sz.y / 2), (lv_coord_t)(cx + sz.x / 2), (lv_coord_t)(cy + sz.y / 2) };
  lv_draw_label(&a, mask, st, LV_OPA_COVER, t, LV_TXT_FLAG_NONE, nullptr, nullptr, nullptr, LV_BIDI_DIR_LTR);
}

// ---- Lista de parametros -----------------------------------------------------------------------------------
static lv_area_t box1(const lv_area_t *r) { lv_area_t b = { (lv_coord_t)(r->x2 - 12 - VBW), (lv_coord_t)(r->y1 + (ROW_H - VBH) / 2), (lv_coord_t)(r->x2 - 12), (lv_coord_t)(r->y1 + (ROW_H + VBH) / 2 - 1) }; return b; }
static lv_area_t box2(const lv_area_t *r) { lv_area_t b = box1(r); b.x1 -= VBW + 8; b.x2 -= VBW + 8; return b; }

static void draw_value_box(const lv_area_t *mask, const lv_area_t *b, const char *t, const lv_style_t *tst) {
  lv_draw_rect(b, mask, arq_style_btn(ARQ_BTN_SECONDARY, LV_BTN_STATE_REL), LV_OPA_COVER);
  draw_cx(mask, b, t, tst);
}

static bool list_design(lv_obj_t *o, const lv_area_t *mask_in, lv_design_mode_t mode) {
  if (mode == LV_DESIGN_COVER_CHK) return false;
  if (mode != LV_DESIGN_DRAW_MAIN) return true;
  lv_area_t a, clip;
  lv_obj_get_coords(o, &a);
  if (!lv_area_intersect(&clip, mask_in, &a)) return true;
  const lv_area_t *mask = &clip;
  const lv_style_t *body = arq_style_text(ARQ_TEXT_BODY);
  const lv_coord_t th = text_h(body);

  for (uint8_t i = 0; i < nrows && i < ARQ_PL_ROWS; ++i) {
    const arq_pl_row_t &r = rows[i];
    const lv_area_t ra = { a.x1, (lv_coord_t)(a.y1 + i * ROW_DY), (lv_coord_t)(a.x1 + LW - 1), (lv_coord_t)(a.y1 + i * ROW_DY + ROW_H - 1) };
    lv_draw_rect(&ra, mask, arq_style_panel(), LV_OPA_COVER);
    lv_coord_t right = ra.x2 - PADX;                        // limite derecho del nombre
    switch (r.kind) {
      case ARQ_PL_EDIT2: {
        const lv_area_t b1 = box1(&ra), b2 = box2(&ra);
        draw_value_box(mask, &b2, r.v2, arq_style_text(ARQ_TEXT_ACCENT));
        draw_value_box(mask, &b1, r.v1, arq_style_text(ARQ_TEXT_ACCENT));
        right = b2.x1 - 10;
      } break;
      case ARQ_PL_EDIT1: {
        const lv_area_t b1 = box1(&ra);
        draw_value_box(mask, &b1, r.v1, arq_style_text(ARQ_TEXT_ACCENT));
        right = b1.x1 - 10;
      } break;
      case ARQ_PL_ONOFF: {                                  // verde = activo, gris = inactivo
        const lv_area_t b1 = box1(&ra);
        lv_draw_rect(&b1, mask, arq_style_btn(r.on ? ARQ_BTN_SUCCESS : ARQ_BTN_SECONDARY, LV_BTN_STATE_REL), LV_OPA_COVER);
        draw_cx(mask, &b1, r.on ? "Activado" : "Desactivado", arq_style_text(r.on ? ARQ_TEXT_DARK : ARQ_TEXT_MUTED));
        right = b1.x1 - 10;
      } break;
      default:                                              // NAV: chevron
        lv_area_t ic = { (lv_coord_t)(ra.x2 - PADX - ARQ_ICON_SMALL + 1), (lv_coord_t)(ra.y1 + (ROW_H - ARQ_ICON_SMALL) / 2),
                         (lv_coord_t)(ra.x2 - PADX), (lv_coord_t)(ra.y1 + (ROW_H + ARQ_ICON_SMALL) / 2 - 1) };
        lv_draw_img(&ic, mask, &arq_icon_s_chev, arq_style_icon(ARQ_TINT_MUTED), LV_OPA_COVER);
        right = ic.x1 - 10;
    }
    draw_fit(mask, ra.x1 + PADX, ra.y1 + (ROW_H - th) / 2, right - (ra.x1 + PADX), r.name ? r.name : "", body);
  }
  return true;
}

static int pick_at(lv_obj_t *o, const lv_point_t *p, uint8_t *row_out) {
  if (!o || !p || o != o_list) return -1;
  lv_area_t a; lv_obj_get_coords(o, &a);
  const lv_coord_t ry = p->y - a.y1;
  if (p->x < a.x1 || p->x > a.x2 || ry < 0) return -1;
  const int i = ry / ROW_DY;
  if (i >= nrows || ry % ROW_DY >= ROW_H) return -1;
  const arq_pl_row_t &r = rows[i];
  int id = r.id1;
  if (r.kind == ARQ_PL_EDIT2) {
    const lv_area_t ra = { a.x1, (lv_coord_t)(a.y1 + i * ROW_DY), (lv_coord_t)(a.x1 + LW - 1), (lv_coord_t)(a.y1 + i * ROW_DY + ROW_H - 1) };
    const lv_area_t b1 = box1(&ra), b2 = box2(&ra);
    if (p->x >= b1.x1 - 4 && p->x <= b1.x2 + 4)      id = r.id1;
    else if (p->x >= b2.x1 - 4 && p->x <= b2.x2 + 4) id = r.id2;
    else id = -1;
  }
  if (id < 0) return -1;
  if (row_out) *row_out = (uint8_t)i;
  return id;
}

int arq_pl_pick(lv_obj_t *o, const lv_point_t *p, uint8_t *row_out) { return pick_at(o, p, row_out); }

static void list_event(lv_obj_t *o, lv_event_t ev) {
  if (ev != LV_EVENT_RELEASED || !user_cb) return;
  lv_point_t p;
  lv_indev_t *in = lv_indev_get_act();
  if (!in) return;
  lv_indev_get_point(in, &p);
  uint8_t row = 0;
  const int id = pick_at(o, &p, &row);
  if (id < 0) return;
  last_row = row;
  last_name = rows[row].name;
  o->mks_obj_id = (decltype(o->mks_obj_id))id;
  user_cb(o, LV_EVENT_RELEASED);                             // el manejador original puede liberar la pantalla: no se usa 'o' despues
}

void arq_pl_forget() {
  o_list = o_page = nullptr; hd.bar = hd.back = nullptr;
  nrows = last_row = 0; user_cb = nullptr;
  memset(rows, 0, sizeof(rows));
}

lv_obj_t * arq_pl_begin(lv_obj_t *scr, const char *title) {
  arq_theme_init();
  arq_pl_forget();
  last_name = nullptr;
  arq_screen_apply(scr);
  arq_header_create(&hd, scr, title ? title : "", false, nullptr, 0);
  return hd.bar;
}

void arq_pl_back(lv_obj_t *scr, lv_event_cb_t cb, uint16_t id) { arq_header_add_back(&hd, scr, cb, id); }

void arq_pl_page(lv_obj_t *scr, const char *text, lv_event_cb_t cb, uint16_t id) {
  if (o_page) return;
  o_page = arq_button_create(scr, ARQ_W - ARQ_MARGIN - 110, 4, 110, 36, nullptr, text, ARQ_BTN_SECONDARY, cb, id);
}

void arq_pl_row(lv_obj_t *scr, uint8_t row, const arq_pl_row_t *r, lv_event_cb_t cb) {
  if (row >= ARQ_PL_ROWS || !r) return;
  if (!o_list) {
    o_list = lv_obj_create(scr, nullptr);
    if (!o_list) return;
    lv_obj_set_pos(o_list, LX, LY);
    lv_obj_set_size(o_list, LW, ROW_DY * ARQ_PL_ROWS - (ROW_DY - ROW_H));
    lv_obj_set_design_cb(o_list, list_design);
    lv_obj_set_click(o_list, true);
    lv_obj_set_event_cb_mks(o_list, list_event, 0, "", 0);
  }
  user_cb = cb;
  rows[row] = *r;
  if (row + 1 > nrows) nrows = row + 1;
  lv_obj_invalidate(o_list);
}

void arq_pl_set_on(bool on) { if (last_row < ARQ_PL_ROWS) { rows[last_row].on = on; if (o_list) lv_obj_invalidate(o_list); } }
const char *arq_pl_last_name() { return last_name; }

// ---- Teclado numerico --------------------------------------------------------------------------------------
#define KY      106
#define KH      (ARQ_H - ARQ_MARGIN - KY)           // 204
#define KGAP    6
#define KCW     ((LW - 3 * ARQ_GAP) / 4)            // ancho de tecla
#define KRH     ((KH - 3 * KGAP) / 4)               // alto de tecla
#define DY      52
#define DH      46

static arq_header_t  nk_hd;
static lv_obj_t     *o_keys, *o_disp;
static char          disp_txt[16];
static bool          ok_enabled;
static int8_t        pressed_key;
static lv_event_cb_t nk_cb;

// Disposicion 4x4 (fila, columna) -> id. Digitos en el mismo orden que el teclado original (1-2-3 arriba).
static const uint8_t kmap[4][4] = {
  { ARQ_NK_1,     2, 3, ARQ_NK_BKSP   },
  { 4,            5, 6, ARQ_NK_CLEAR  },
  { 7,            8, 9, ARQ_NK_CANCEL },
  { ARQ_NK_MINUS, ARQ_NK_0, ARQ_NK_POINT, ARQ_NK_OK }
};

static lv_area_t key_area(const lv_area_t *a, int r, int c) {
  lv_area_t k = { (lv_coord_t)(a->x1 + c * (KCW + ARQ_GAP)), (lv_coord_t)(a->y1 + r * (KRH + KGAP)), 0, 0 };
  k.x2 = k.x1 + KCW - 1; k.y2 = k.y1 + KRH - 1;
  return k;
}

static bool keys_design(lv_obj_t *o, const lv_area_t *mask_in, lv_design_mode_t mode) {
  if (mode == LV_DESIGN_COVER_CHK) return false;
  if (mode != LV_DESIGN_DRAW_MAIN) return true;
  lv_area_t a, clip;
  lv_obj_get_coords(o, &a);
  if (!lv_area_intersect(&clip, mask_in, &a)) return true;
  const lv_area_t *mask = &clip;
  for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) {
    const int id = kmap[r][c];
    const lv_area_t k = key_area(&a, r, c);
    const bool pr = (id == pressed_key);
    if (id == ARQ_NK_OK) {
      lv_draw_rect(&k, mask, arq_style_btn(ok_enabled ? ARQ_BTN_PRIMARY : ARQ_BTN_SECONDARY, ok_enabled ? (pr ? LV_BTN_STATE_PR : LV_BTN_STATE_REL) : LV_BTN_STATE_INA), LV_OPA_COVER);
      draw_cx(mask, &k, "Aceptar", arq_style_text(ok_enabled ? ARQ_TEXT_DARK : ARQ_TEXT_MUTED));
      continue;
    }
    const bool word = (id == ARQ_NK_CLEAR || id == ARQ_NK_CANCEL);
    lv_draw_rect(&k, mask, arq_style_btn(ARQ_BTN_SECONDARY, pr ? LV_BTN_STATE_PR : LV_BTN_STATE_REL), LV_OPA_COVER);
    if (id == ARQ_NK_BKSP) {
      lv_area_t ic = { (lv_coord_t)((k.x1 + k.x2) / 2 - ARQ_ICON_SIZE / 2), (lv_coord_t)((k.y1 + k.y2) / 2 - ARQ_ICON_SIZE / 2), 0, 0 };
      ic.x2 = ic.x1 + ARQ_ICON_SIZE - 1; ic.y2 = ic.y1 + ARQ_ICON_SIZE - 1;
      lv_draw_img(&ic, mask, &arq_icon_left, arq_style_icon(ARQ_TINT_TEXT), LV_OPA_COVER);
      continue;
    }
    char t[2] = { '\0', '\0' };
    const char *label = t;
    if (id >= ARQ_NK_1 && id <= ARQ_NK_9) t[0] = (char)('0' + id);
    else if (id == ARQ_NK_0)      t[0] = '0';
    else if (id == ARQ_NK_POINT)  t[0] = '.';
    else if (id == ARQ_NK_MINUS)  t[0] = '-';
    else if (id == ARQ_NK_CLEAR)  label = "Borrar";
    else if (id == ARQ_NK_CANCEL) label = "Cancelar";
    draw_cx(mask, &k, label, arq_style_text(word ? ARQ_TEXT_MUTED : ARQ_TEXT_NUM_L));
  }
  return true;
}

static bool disp_design(lv_obj_t *o, const lv_area_t *mask_in, lv_design_mode_t mode) {
  if (mode == LV_DESIGN_COVER_CHK) return false;
  if (mode != LV_DESIGN_DRAW_MAIN) return true;
  lv_area_t a, clip;
  lv_obj_get_coords(o, &a);
  if (!lv_area_intersect(&clip, mask_in, &a)) return true;
  lv_draw_rect(&a, &clip, arq_style_btn(ARQ_BTN_SECONDARY, LV_BTN_STATE_PR), LV_OPA_COVER);   // borde azul = campo activo
  const lv_style_t *st = arq_style_text(ARQ_TEXT_NUM_L);
  lv_point_t sz;
  const char *t = disp_txt[0] ? disp_txt : "0";
  lv_txt_get_size(&sz, t, st->text.font, st->text.letter_space, st->text.line_space, LV_COORD_MAX, LV_TXT_FLAG_NONE);
  lv_area_t ta = { (lv_coord_t)(a.x2 - 16 - sz.x), (lv_coord_t)((a.y1 + a.y2) / 2 - sz.y / 2), (lv_coord_t)(a.x2 - 16), (lv_coord_t)((a.y1 + a.y2) / 2 + sz.y / 2) };
  lv_draw_label(&ta, &clip, disp_txt[0] ? st : arq_style_text(ARQ_TEXT_MUTED), LV_OPA_COVER, t, LV_TXT_FLAG_NONE, nullptr, nullptr, nullptr, LV_BIDI_DIR_LTR);
  return true;
}

static int key_at(lv_obj_t *o, const lv_point_t *p) {
  if (!o || !p || o != o_keys) return -1;
  lv_area_t a; lv_obj_get_coords(o, &a);
  for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) {
    const lv_area_t k = key_area(&a, r, c);
    if (p->x >= k.x1 && p->x <= k.x2 && p->y >= k.y1 && p->y <= k.y2) return kmap[r][c];
  }
  return -1;
}

int arq_numpad_pick(lv_obj_t *o, const lv_point_t *p) { return key_at(o, p); }

static void keys_event(lv_obj_t *o, lv_event_t ev) {
  lv_point_t p;
  lv_indev_t *in = lv_indev_get_act();
  if (!in) return;
  lv_indev_get_point(in, &p);
  if (ev == LV_EVENT_PRESSED) {
    const int k = key_at(o, &p);
    if (k != pressed_key) { pressed_key = (int8_t)k; lv_obj_invalidate(o); }
  }
  else if (ev == LV_EVENT_RELEASED || ev == LV_EVENT_PRESS_LOST) {
    const int k = (ev == LV_EVENT_RELEASED) ? key_at(o, &p) : -1;
    if (pressed_key != -1) { pressed_key = -1; lv_obj_invalidate(o); }
    if (k < 0 || !nk_cb) return;
    if (k == ARQ_NK_OK && !ok_enabled) return;              // "Aceptar" deshabilitado (valor no valido)
    o->mks_obj_id = (decltype(o->mks_obj_id))k;
    nk_cb(o, LV_EVENT_RELEASED);                             // puede liberar la pantalla
  }
}

void arq_numpad_forget() { o_keys = o_disp = nullptr; nk_hd.bar = nk_hd.back = nullptr; nk_cb = nullptr; pressed_key = -1; ok_enabled = false; disp_txt[0] = '\0'; }

void arq_numpad_build(lv_obj_t *scr, lv_event_cb_t cb, const char *title) {
  arq_theme_init();
  arq_numpad_forget();
  nk_cb = cb;
  arq_screen_apply(scr);
  arq_header_create(&nk_hd, scr, title && title[0] ? title : "Valor", false, cb, ARQ_NK_BACK);
  o_disp = lv_obj_create(scr, nullptr);
  if (o_disp) {
    lv_obj_set_pos(o_disp, LX, DY);
    lv_obj_set_size(o_disp, LW, DH);
    lv_obj_set_design_cb(o_disp, disp_design);
    lv_obj_set_click(o_disp, false);
  }
  o_keys = lv_obj_create(scr, nullptr);
  if (o_keys) {
    lv_obj_set_pos(o_keys, LX, KY);
    lv_obj_set_size(o_keys, LW, KH);
    lv_obj_set_design_cb(o_keys, keys_design);
    lv_obj_set_click(o_keys, true);
    lv_obj_set_event_cb_mks(o_keys, keys_event, 0, "", 0);
  }
}

void arq_numpad_set(const char *text, bool can_ok) {
  strncpy(disp_txt, text ? text : "", sizeof(disp_txt) - 1); disp_txt[sizeof(disp_txt) - 1] = '\0';
  ok_enabled = can_ok;
  if (o_disp) lv_obj_invalidate(o_disp);
  if (o_keys) lv_obj_invalidate(o_keys);
}
