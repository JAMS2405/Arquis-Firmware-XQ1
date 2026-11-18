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

#include "arquis_files_view.h"

#include <string.h>

#define LX     ARQ_MARGIN
#define LY     (ARQ_HEADER_H + 8)
#define LW     400
#define LH     (ARQ_H - ARQ_MARGIN - LY)
#define ROW_H  38
#define ROW_DY 42
#define PX     (LX + LW + ARQ_GAP)

static arq_header_t    header;
static lv_obj_t       *o_list, *o_up, *o_down;
static arq_files_model_t M;
static bool            valid;

static void draw_text_fit(const lv_area_t *mask, lv_coord_t x, lv_coord_t y, lv_coord_t maxw, const char *t, const lv_style_t *st) {
  lv_point_t sz;
  lv_txt_get_size(&sz, t, st->text.font, st->text.letter_space, st->text.line_space, LV_COORD_MAX, LV_TXT_FLAG_NONE);
  char tmp[48];
  if (sz.x > maxw) {                                         // nombre largo: se recorta con "..."
    size_t n = strlen(t); if (n > 43) n = 43;
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

static void draw_text_cx(const lv_area_t *mask, lv_coord_t cx, lv_coord_t y, const char *t, const lv_style_t *st) {
  lv_point_t sz;
  lv_txt_get_size(&sz, t, st->text.font, st->text.letter_space, st->text.line_space, LV_COORD_MAX, LV_TXT_FLAG_NONE);
  lv_area_t a = { (lv_coord_t)(cx - sz.x / 2), y, (lv_coord_t)(cx + sz.x / 2), (lv_coord_t)(y + sz.y) };
  lv_draw_label(&a, mask, st, LV_OPA_COVER, t, LV_TXT_FLAG_NONE, nullptr, nullptr, nullptr, LV_BIDI_DIR_LTR);
}

static void draw_icon(const lv_area_t *mask, lv_coord_t x, lv_coord_t y, const lv_img_dsc_t *icon, const lv_style_t *st) {
  lv_area_t a = { x, y, (lv_coord_t)(x + icon->header.w - 1), (lv_coord_t)(y + icon->header.h - 1) };
  lv_draw_img(&a, mask, icon, st, LV_OPA_COVER);
}

static bool list_design(lv_obj_t *o, const lv_area_t *mask_in, lv_design_mode_t mode) {
  if (mode == LV_DESIGN_COVER_CHK) return false;
  if (mode != LV_DESIGN_DRAW_MAIN) return true;
  lv_area_t a, clip;
  lv_obj_get_coords(o, &a);
  if (!lv_area_intersect(&clip, mask_in, &a)) return true;
  const lv_area_t *mask = &clip;

  if (M.n == 0) {
    const lv_coord_t cx = a.x1 + LW / 2, cy = a.y1 + 86;
    draw_text_cx(mask, cx, cy,      M.no_card ? "Sin tarjeta SD" : "Carpeta vac\xC3\xAD" "a", arq_style_text(ARQ_TEXT_BODY));
    draw_text_cx(mask, cx, cy + 28, M.no_card ? "Inserta una tarjeta con archivos G-code" : "No hay archivos G-code aqu\xC3\xAD", arq_style_text(ARQ_TEXT_MUTED));
    return true;
  }
  for (uint8_t i = 0; i < M.n && i < ARQ_FILE_ROWS; ++i) {
    const bool dir = (M.dir_mask >> i) & 1;
    const lv_area_t r = { a.x1, (lv_coord_t)(a.y1 + i * ROW_DY), (lv_coord_t)(a.x1 + LW - 1), (lv_coord_t)(a.y1 + i * ROW_DY + ROW_H - 1) };
    lv_draw_rect(&r, mask, arq_style_panel(), LV_OPA_COVER);
    draw_icon(mask, r.x1 + 12, r.y1 + (ROW_H - ARQ_ICON_SMALL) / 2, dir ? &arq_icon_s_folder : &arq_icon_s_file, arq_style_icon(dir ? ARQ_TINT_ACCENT : ARQ_TINT_TEXT));
    lv_point_t ts;
    const lv_style_t *st = arq_style_text(ARQ_TEXT_BODY);
    lv_txt_get_size(&ts, "A", st->text.font, st->text.letter_space, st->text.line_space, LV_COORD_MAX, LV_TXT_FLAG_NONE);
    draw_text_fit(mask, r.x1 + 44, r.y1 + (ROW_H - ts.y) / 2, LW - 44 - (dir ? 40 : 14), M.name[i] ? M.name[i] : "", st);
    if (dir) draw_icon(mask, r.x2 - 12 - ARQ_ICON_SMALL, r.y1 + (ROW_H - ARQ_ICON_SMALL) / 2, &arq_icon_s_chev, arq_style_icon(ARQ_TINT_MUTED));
  }
  return true;
}

void arq_files_forget() { o_list = o_up = o_down = nullptr; header.bar = header.back = nullptr; valid = false; memset(&M, 0, sizeof(M)); }

void arq_files_build(lv_obj_t *scr, lv_event_cb_t cb) {
  arq_theme_init();
  arq_files_forget();
  arq_header_create(&header, scr, "Archivos", false, cb, ARQ_FLID_BACK);
  o_list = lv_obj_create(scr, nullptr);
  if (o_list) {
    lv_obj_set_pos(o_list, LX, LY);
    lv_obj_set_size(o_list, LW, LH);
    lv_obj_set_design_cb(o_list, list_design);
    lv_obj_set_click(o_list, true);
    lv_obj_set_event_cb_mks(o_list, cb, ARQ_FLID_LIST, "", 0);
  }
  const lv_coord_t bw = ARQ_W - ARQ_MARGIN - PX, bh = (LH - ARQ_GAP) / 2;
  o_up   = arq_button_create(scr, PX, LY, bw, bh, &arq_icon_up, nullptr, ARQ_BTN_SECONDARY, cb, ARQ_FLID_UP);
  o_down = arq_button_create(scr, PX, LY + bh + ARQ_GAP, bw, LH - bh - ARQ_GAP, &arq_icon_down, nullptr, ARQ_BTN_SECONDARY, cb, ARQ_FLID_DOWN);
}

void arq_files_update(const arq_files_model_t *m) {
  // Siempre se redibuja: los nombres son punteros a buffers que el llamador reutiliza en cada pagina (no se pueden comparar).
  // Solo se llama al abrir la pantalla o al cambiar de pagina/carpeta, nunca de forma periodica.
  M = *m; valid = true;
  if (M.n > ARQ_FILE_ROWS) M.n = ARQ_FILE_ROWS;
  if (header.bar) { arq_card_set_title(header.bar, M.title ? M.title : "Archivos", ARQ_TEXT_BODY); arq_card_refresh(header.bar); }
  if (o_list) lv_obj_invalidate(o_list);
  arq_card_set_disabled(o_up, !M.can_up);
  arq_card_set_disabled(o_down, !M.can_down);
}

int8_t arq_files_pick(lv_obj_t *o, const lv_point_t *p) {
  if (!o || !p || o != o_list) return -1;
  lv_area_t a; lv_obj_get_coords(o, &a);
  const lv_coord_t ry = p->y - a.y1, rx = p->x - a.x1;
  if (rx < 0 || rx >= LW || ry < 0) return -1;
  const int i = ry / ROW_DY;
  if (i >= M.n || ry % ROW_DY >= ROW_H) return -1;
  return (int8_t)i;
}

void arq_confirm_build(lv_obj_t *scr, lv_event_cb_t cb, const char *file_name, bool has_thumb) {
  arq_theme_init();
  arq_header_t hd;
  arq_header_create(&hd, scr, "Imprimir archivo", false, cb, ARQ_PC_CANCEL);
  if (!has_thumb) {                                   // recuadro del mismo tamano que la miniatura
    lv_obj_t *ph = arq_card_create(scr, ARQ_CONFIRM_THUMB_X, ARQ_CONFIRM_THUMB_Y, ARQ_CONFIRM_THUMB_S, ARQ_CONFIRM_THUMB_S, ARQ_LAYOUT_TILE, ARQ_BTN_SECONDARY, nullptr, 0);
    arq_card_set_icon(ph, &arq_icon_file, ARQ_TINT_MUTED);
    arq_card_set_title(ph, "Sin miniatura", ARQ_TEXT_MUTED);
  }
  const lv_coord_t rx = ARQ_CONFIRM_THUMB_X + ARQ_CONFIRM_THUMB_S + ARQ_GAP + 2, rw = ARQ_W - ARQ_MARGIN - rx;
  lv_obj_t *card = arq_card_create(scr, rx, ARQ_CONFIRM_THUMB_Y, rw, 128, ARQ_LAYOUT_DIALOG, ARQ_BTN_SECONDARY, nullptr, 0);
  arq_card_set_title(card, "\xC2\xBFImprimir este archivo?", ARQ_TEXT_BODY);
  arq_card_set_sub(card, file_name, ARQ_TEXT_MUTED);
  const lv_coord_t by = ARQ_CONFIRM_THUMB_Y + 128 + ARQ_GAP, bh = ARQ_CONFIRM_THUMB_Y + ARQ_CONFIRM_THUMB_S - by, bw = (rw - ARQ_GAP) / 2;
  arq_button_create(scr, rx, by, bw, bh, nullptr, "Cancelar", ARQ_BTN_SECONDARY, cb, ARQ_PC_CANCEL);
  arq_button_create(scr, rx + bw + ARQ_GAP, by, rw - bw - ARQ_GAP, bh, nullptr, "Imprimir", ARQ_BTN_PRIMARY, cb, ARQ_PC_OK);
}
