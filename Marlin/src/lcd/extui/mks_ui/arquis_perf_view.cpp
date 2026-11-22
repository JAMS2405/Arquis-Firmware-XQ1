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

#include "arquis_perf_view.h"

#include <stdio.h>
#include <string.h>

#define CY0   (ARQ_HEADER_H + 8)
#define LX    ARQ_MARGIN
#define FW    (ARQ_W - 2 * ARQ_MARGIN)                  // 460
#define HW    ((FW - ARQ_GAP) / 2)                      // 226
#define ROW_H 32
#define ROW_G 3

static struct {
  arq_header_t hd;
  lv_obj_t *scr, *pick[ARQ_PF_COUNT], *tmid, *chip, *minus, *plus, *save, *restore;
  lv_event_cb_t cb;
  bool valid;
  arq_perf_view_model_t M;
  char pt[ARQ_PF_COUNT][48], mid[48], chiptxt[56];
} V;

void arq_perf_view_forget() { memset(&V, 0, sizeof(V)); }

static lv_obj_t * btn(lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h, const lv_img_dsc_t *ic, const char *t, arq_btn_kind_t k, uint16_t id) {
  return arq_button_create(V.scr, x, y, w, h, ic, t, k, V.cb, id);
}

void arq_perf_view_build(lv_obj_t *scr, lv_event_cb_t cb) {
  arq_perf_view_forget();
  V.scr = scr; V.cb = cb;
  arq_header_create(&V.hd, scr, "Rendimiento", false, cb, ARQ_PFV_BACK);
  for (uint8_t i = 0; i < ARQ_PF_COUNT; ++i) {
    const lv_coord_t x = LX + (i % 2) * (HW + ARQ_GAP), y = CY0 + (i / 2) * (ROW_H + ROW_G);
    V.pick[i] = arq_card_create(scr, x, y, HW, ROW_H, ARQ_LAYOUT_BUTTON, ARQ_BTN_SECONDARY, cb, ARQ_PFV_PICK0 + i);
    arq_card_set_title(V.pick[i], V.pt[i], ARQ_TEXT_BODY);
  }
  const lv_coord_t ys = CY0 + 4 * (ROW_H + ROW_G) + 1;            // fila de ajuste
  V.minus = btn(LX, ys, 80, 40, &arq_icon_minus, nullptr, ARQ_BTN_SECONDARY, ARQ_PFV_MINUS);
  V.tmid = arq_card_create(scr, LX + 84, ys, FW - 168, 40, ARQ_LAYOUT_BUTTON, ARQ_BTN_SECONDARY, cb, ARQ_PFV_STEP);
  arq_card_set_title(V.tmid, V.mid, ARQ_TEXT_BODY);
  V.plus = btn(LX + FW - 80, ys, 80, 40, &arq_icon_plus, nullptr, ARQ_BTN_SECONDARY, ARQ_PFV_PLUS);
  V.chip = arq_chip_create(scr, LX, ys + 43, FW, V.chiptxt, ARQ_TINT_ACCENT);
  const lv_coord_t yb = ys + 43 + 32;
  V.restore = btn(LX, yb, HW, ARQ_H - ARQ_MARGIN - yb, nullptr, "Restaurar", ARQ_BTN_SECONDARY, ARQ_PFV_RESTORE);
  V.save    = btn(LX + HW + ARQ_GAP, yb, HW, ARQ_H - ARQ_MARGIN - yb, &arq_icon_check, "Guardar", ARQ_BTN_SUCCESS, ARQ_PFV_SAVE);
}

static void fill_texts(const arq_perf_view_model_t &m) {
  for (uint8_t i = 0; i < ARQ_PF_COUNT; ++i) {
    const arq_pf_param_t p = arq_pf_param(i, m.shaping_min);
    char v[16]; arq_pf_fmt_value(v, sizeof v, i, m.val[i]);
    snprintf(V.pt[i], sizeof(V.pt[i]), "%s  %s%s", p.label, m.has[i] ? v : "--", (m.has[i] && !arq_pf_equal(i, m.val[i], m.ref[i])) ? " *" : "");
  }
  const arq_pf_param_t p = arq_pf_param(m.sel, m.shaping_min);
  char v[16], st[16]; arq_pf_fmt_value(v, sizeof v, m.sel, m.val[m.sel]);
  arq_pf_fmt_value(st, sizeof st, m.sel, m.coarse ? p.coarse : p.fine);
  snprintf(V.mid, sizeof(V.mid), "%s%s%s   (paso %s)", v, strcmp(v, "OFF") ? " " : "", strcmp(v, "OFF") ? p.unit : "", st);
  // linea de estado: primero lo que impide editar; luego el aviso del valor elegido; luego si hay cambios sin guardar
  const char *t = nullptr; uint8_t dot = ARQ_TINT_ACCENT;
  if (m.why) { t = arq_pf_err_text(m.why); dot = ARQ_TINT_WARN; }
  else {
    const uint8_t h = arq_pf_hint(m.sel, m.val[m.sel], m.val[ARQ_PF_AMAX_X], m.val[ARQ_PF_AMAX_Y], m.ref[m.sel]);
    if (h) { t = arq_pf_hint_text(h); dot = ARQ_TINT_WARN; }
    else if (m.dirty) { t = "Cambios sin guardar (se pierden al apagar)"; dot = ARQ_TINT_WARN; }
    else { t = "Valores guardados. Toca el valor para cambiar el paso"; dot = ARQ_TINT_OK; }
  }
  snprintf(V.chiptxt, sizeof(V.chiptxt), "%s", t);
  arq_card_set_dot(V.chip, (arq_tint_t)dot);
}

void arq_perf_view_update(const arq_perf_view_model_t *m) {
  if (!V.scr) return;
  if (V.valid && memcmp(&V.M, m, sizeof(*m)) == 0) return;
  memcpy(&V.M, m, sizeof(*m)); V.valid = true;
  fill_texts(V.M);
  for (uint8_t i = 0; i < ARQ_PF_COUNT; ++i) {
    arq_card_set_selected(V.pick[i], V.M.sel == i);
    arq_card_set_disabled(V.pick[i], !V.M.has[i]);
    arq_card_refresh(V.pick[i]);
  }
  const bool ed = V.M.why == 0;
  arq_card_set_disabled(V.minus, !ed); arq_card_set_disabled(V.plus, !ed);
  arq_card_set_disabled(V.save, !ed); arq_card_set_disabled(V.restore, !ed || !V.M.dirty);
  arq_card_refresh(V.tmid); arq_card_refresh(V.chip);
}
