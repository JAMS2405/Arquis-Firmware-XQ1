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

#include "arquis_theme.h"

// Todos los estilos viven en RAM estatica (sin heap de LVGL).
static lv_style_t st_focus, st_scrim, st_chip, st_screen, st_panel, st_panel_pr, st_header, st_bar_bg, st_bar_indic;
static lv_style_t st_text[ARQ_TEXT_COUNT];
static lv_style_t st_icon[ARQ_TINT_COUNT];
static lv_style_t st_dot[ARQ_TINT_COUNT];
static lv_style_t st_btn[ARQ_BTN_COUNT][2];   // [tipo][0=reposo, 1=pulsado]
static lv_style_t st_btn_ina;                  // deshabilitado (comun)
static bool inited = false;

lv_color_t arq_tint_color(arq_tint_t t) {
  switch (t) {
    case ARQ_TINT_ACCENT: return ARQ_COL_BLUE;
    case ARQ_TINT_TEXT:   return ARQ_COL_TEXT;
    case ARQ_TINT_MUTED:  return ARQ_COL_MUTED;
    case ARQ_TINT_DARK:   return ARQ_COL_BG;
    case ARQ_TINT_OK:     return ARQ_COL_GREEN;
    case ARQ_TINT_WARN:   return ARQ_COL_WARN;
    case ARQ_TINT_DANGER: return ARQ_COL_DANGER;
    default:              return ARQ_COL_TEXT;
  }
}

static void flat(lv_style_t &s, lv_color_t bg, lv_coord_t radius) {
  lv_style_copy(&s, &lv_style_plain);
  s.body.main_color = s.body.grad_color = bg;
  s.body.radius = radius;
  s.body.opa = LV_OPA_COVER;
  s.body.border.width = 0;
  s.body.shadow.width = 0;
  s.body.padding.top = s.body.padding.bottom = s.body.padding.left = s.body.padding.right = 0;
  s.body.padding.inner = 0;
}

static void text_style(lv_style_t &s, lv_color_t c, const lv_font_t *f) {
  lv_style_copy(&s, &lv_style_plain);
  s.text.color = c;
  s.text.font = f;
  s.text.letter_space = 0;
  s.text.line_space = 2;
  s.text.opa = LV_OPA_COVER;
}

static void btn_style(lv_style_t &s, lv_color_t bg, lv_color_t border, lv_coord_t bw, lv_color_t txt, lv_opa_t opa = LV_OPA_COVER) {
  flat(s, bg, ARQ_RADIUS);
  s.body.opa = opa;
  s.body.border.color = border;
  s.body.border.width = bw;
  s.body.border.opa = opa;
  s.body.border.part = LV_BORDER_FULL;
  s.text.color = txt;
  s.text.font = &arq_font_text16;
  s.image.color = txt;
  s.image.intense = LV_OPA_COVER;
}

void arq_theme_init() {
  if (inited) return;
  inited = true;

  flat(st_screen, ARQ_COL_BG, 0);

  flat(st_panel, ARQ_COL_PANEL, ARQ_RADIUS);
  st_panel.body.border.color = ARQ_COL_LINE;
  st_panel.body.border.width = 1;
  st_panel.body.border.opa = LV_OPA_COVER;
  st_panel.body.border.part = LV_BORDER_FULL;

  flat(st_focus, ARQ_COL_BG, ARQ_RADIUS);
  st_focus.body.opa = LV_OPA_TRANSP;
  st_focus.body.border.color = ARQ_COL_BLUE;
  st_focus.body.border.width = 3;
  st_focus.body.border.opa = LV_OPA_COVER;
  st_focus.body.border.part = LV_BORDER_FULL;

  flat(st_scrim, ARQ_COL_BG, 0);
  st_scrim.body.opa = LV_OPA_70;

  st_chip = st_panel;
  st_chip.body.radius = LV_RADIUS_CIRCLE;

  st_panel_pr = st_panel;
  st_panel_pr.body.main_color = st_panel_pr.body.grad_color = ARQ_COL_PANEL_HI;
  st_panel_pr.body.border.color = ARQ_COL_BLUE;       // azul = interaccion

  flat(st_header, ARQ_COL_PANEL, 0);
  st_header.body.border.color = ARQ_COL_LINE;
  st_header.body.border.width = 1;
  st_header.body.border.opa = LV_OPA_COVER;
  st_header.body.border.part = LV_BORDER_BOTTOM;

  flat(st_bar_bg, ARQ_COL_LINE, 4);
  flat(st_bar_indic, ARQ_COL_BLUE, 4);

  text_style(st_text[ARQ_TEXT_BODY],     ARQ_COL_TEXT,  &arq_font_text16);
  text_style(st_text[ARQ_TEXT_MUTED],    ARQ_COL_MUTED, &arq_font_text16);
  text_style(st_text[ARQ_TEXT_ACCENT],   ARQ_COL_BLUE,  &arq_font_text16);
  text_style(st_text[ARQ_TEXT_OK],       ARQ_COL_GREEN, &arq_font_text16);
  text_style(st_text[ARQ_TEXT_WARN],     ARQ_COL_WARN,  &arq_font_text16);
  text_style(st_text[ARQ_TEXT_DARK],     ARQ_COL_BG,    &arq_font_text16);
  text_style(st_text[ARQ_TEXT_NUM_L],    ARQ_COL_TEXT,  &arq_font_num36);
  text_style(st_text[ARQ_TEXT_NUM_L_OK], ARQ_COL_GREEN, &arq_font_num36);
  text_style(st_text[ARQ_TEXT_NUM_L_WARN], ARQ_COL_WARN, &arq_font_num36);
  text_style(st_text[ARQ_TEXT_NUM_M],    ARQ_COL_MUTED, &arq_font_num20);

  for (uint8_t i = 0; i < ARQ_TINT_COUNT; ++i) {
    lv_style_copy(&st_icon[i], &lv_style_plain);
    st_icon[i].image.color = arq_tint_color((arq_tint_t)i);
    st_icon[i].image.intense = LV_OPA_COVER;
    st_icon[i].image.opa = LV_OPA_COVER;
    flat(st_dot[i], arq_tint_color((arq_tint_t)i), LV_RADIUS_CIRCLE);
  }

  btn_style(st_btn[ARQ_BTN_PRIMARY][0],   ARQ_COL_BLUE,     ARQ_COL_BLUE,     0, ARQ_COL_BG);
  btn_style(st_btn[ARQ_BTN_PRIMARY][1],   ARQ_COL_BLUE_DK,  ARQ_COL_BLUE_DK,  0, ARQ_COL_BG);
  btn_style(st_btn[ARQ_BTN_SECONDARY][0], ARQ_COL_PANEL,    ARQ_COL_LINE,     1, ARQ_COL_TEXT);
  btn_style(st_btn[ARQ_BTN_SECONDARY][1], ARQ_COL_PANEL_HI, ARQ_COL_BLUE,     1, ARQ_COL_TEXT);
  btn_style(st_btn[ARQ_BTN_SUCCESS][0],   ARQ_COL_GREEN,    ARQ_COL_GREEN,    0, ARQ_COL_BG);
  btn_style(st_btn[ARQ_BTN_SUCCESS][1],   ARQ_COL_GREEN_DK, ARQ_COL_GREEN_DK, 0, ARQ_COL_BG);
  btn_style(st_btn[ARQ_BTN_DANGER][0],    ARQ_COL_PANEL,    ARQ_COL_DANGER,   2, ARQ_COL_DANGER);   // contorno: no es un bloque rojo enorme
  btn_style(st_btn[ARQ_BTN_DANGER][1],    ARQ_COL_DANGER,   ARQ_COL_DANGER,   2, ARQ_COL_BG);
  btn_style(st_btn[ARQ_BTN_GHOST][0],     ARQ_COL_BG,       ARQ_COL_BG,       0, ARQ_COL_TEXT, LV_OPA_TRANSP);
  btn_style(st_btn[ARQ_BTN_GHOST][1],     ARQ_COL_PANEL_HI, ARQ_COL_PANEL_HI, 0, ARQ_COL_TEXT);
  btn_style(st_btn_ina, ARQ_COL_PANEL, ARQ_COL_LINE, 1, ARQ_COL_MUTED);
}

const lv_style_t* arq_style_screen()        { return &st_screen; }
const lv_style_t* arq_style_panel()         { return &st_panel; }
const lv_style_t* arq_style_panel_pressed() { return &st_panel_pr; }
const lv_style_t* arq_style_focus()         { return &st_focus; }
const lv_style_t* arq_style_scrim()         { return &st_scrim; }
const lv_style_t* arq_style_chip()          { return &st_chip; }
const lv_style_t* arq_style_header()        { return &st_header; }
const lv_style_t* arq_style_bar_bg()        { return &st_bar_bg; }
const lv_style_t* arq_style_bar_indic()     { return &st_bar_indic; }
const lv_style_t* arq_style_text(arq_text_t r)  { return &st_text[r < ARQ_TEXT_COUNT ? r : ARQ_TEXT_BODY]; }
const lv_style_t* arq_style_icon(arq_tint_t t)  { return &st_icon[t < ARQ_TINT_COUNT ? t : ARQ_TINT_TEXT]; }
const lv_style_t* arq_style_dot(arq_tint_t t)   { return &st_dot[t < ARQ_TINT_COUNT ? t : ARQ_TINT_MUTED]; }
const lv_style_t* arq_style_btn(arq_btn_kind_t k, lv_btn_state_t s) {
  if (k >= ARQ_BTN_COUNT) k = ARQ_BTN_SECONDARY;
  switch (s) {
    case LV_BTN_STATE_PR: case LV_BTN_STATE_TGL_PR: return &st_btn[k][1];
    case LV_BTN_STATE_INA: return &st_btn_ina;
    default: return &st_btn[k][0];
  }
}

arq_tint_t arq_icon_state_tint(arq_icon_state_t s) {
  switch (s) {
    case ARQ_ICON_SELECTED: return ARQ_TINT_DARK;      // sobre fondo azul de seleccion
    case ARQ_ICON_ACTIVE:   return ARQ_TINT_OK;
    case ARQ_ICON_DISABLED: return ARQ_TINT_MUTED;
    case ARQ_ICON_WARNING:  return ARQ_TINT_WARN;
    case ARQ_ICON_ERROR:    return ARQ_TINT_DANGER;
    default:                return ARQ_TINT_ACCENT;
  }
}
