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

#include "arquis_pid_view.h"

#include <stdio.h>
#include <string.h>

#define CY0   (ARQ_HEADER_H + 8)
#define CY1   (ARQ_H - ARQ_MARGIN)
#define LX    ARQ_MARGIN
#define FW    (ARQ_W - 2 * ARQ_MARGIN)                  // 460
#define HW    ((FW - ARQ_GAP) / 2)                      // 226
#define DEG   "\xC2\xB0"
#define PW_MAX 14

static struct {
  arq_header_t hd;
  lv_obj_t *scr, *pw[PW_MAX];
  uint8_t   npw;
  lv_event_cb_t cb;
  int8_t    built;
  bool      valid;
  arq_pid_view_model_t M;
  lv_obj_t *pick[ARQ_PID_HEATERS], *tmid, *cmid, *chip, *start, *big, *list, *bar, *apply;
  arq_list_row_t rows[4];
  char v1[12], v2[12], t1[32], t2[32], t3[40], t4[72], t5[28];
} V;

void arq_pid_view_forget() { memset(&V, 0, sizeof(V)); V.built = -1; }

static void panel_clear() {
  for (uint8_t i = 0; i < V.npw; ++i) if (V.pw[i]) lv_obj_del(V.pw[i]);
  memset(V.pw, 0, sizeof(V.pw)); V.npw = 0;
  V.tmid = V.cmid = V.chip = V.start = V.big = V.list = V.bar = V.apply = nullptr;
  for (uint8_t i = 0; i < ARQ_PID_HEATERS; ++i) V.pick[i] = nullptr;
}
static lv_obj_t * keep(lv_obj_t *o) { if (o && V.npw < PW_MAX) V.pw[V.npw++] = o; return o; }
static lv_obj_t * btn(lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h, const lv_img_dsc_t *ic, const char *t, arq_btn_kind_t k, uint16_t id) {
  return keep(arq_button_create(V.scr, x, y, w, h, ic, t, k, V.cb, id));
}
static lv_obj_t * card(lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h, arq_layout_t l) {
  return keep(arq_card_create(V.scr, x, y, w, h, l, ARQ_BTN_SECONDARY, nullptr, 0));
}

static void build_setup() {
  const lv_coord_t y1 = CY0, h1 = 76;
  V.pick[0] = keep(arq_card_create(V.scr, LX, y1, HW, h1, ARQ_LAYOUT_PICK, ARQ_BTN_SECONDARY, V.cb, ARQ_PV_HOT));
  arq_card_set_icon(V.pick[0], &arq_icon_nozzle, ARQ_TINT_ACCENT); arq_card_set_title(V.pick[0], "Hotend", ARQ_TEXT_MUTED); arq_card_set_value(V.pick[0], V.v1, ARQ_TEXT_NUM_L);
  V.pick[1] = keep(arq_card_create(V.scr, LX + HW + ARQ_GAP, y1, HW, h1, ARQ_LAYOUT_PICK, ARQ_BTN_SECONDARY, V.cb, ARQ_PV_BED));
  arq_card_set_icon(V.pick[1], &arq_icon_bed, ARQ_TINT_ACCENT); arq_card_set_title(V.pick[1], "Cama", ARQ_TEXT_MUTED); arq_card_set_value(V.pick[1], V.v2, ARQ_TEXT_NUM_L);
  const lv_coord_t y2 = y1 + h1 + 4, y3 = y2 + 48;
  btn(LX, y2, 80, 44, &arq_icon_minus, nullptr, ARQ_BTN_SECONDARY, ARQ_PV_TDEC);
  V.tmid = card(LX + 84, y2, FW - 168, 44, ARQ_LAYOUT_BUTTON); arq_card_set_title(V.tmid, V.t1, ARQ_TEXT_BODY);
  btn(LX + FW - 80, y2, 80, 44, &arq_icon_plus, nullptr, ARQ_BTN_SECONDARY, ARQ_PV_TINC);
  btn(LX, y3, 80, 44, &arq_icon_minus, nullptr, ARQ_BTN_SECONDARY, ARQ_PV_CDEC);
  V.cmid = card(LX + 84, y3, FW - 168, 44, ARQ_LAYOUT_BUTTON); arq_card_set_title(V.cmid, V.t2, ARQ_TEXT_BODY);
  btn(LX + FW - 80, y3, 80, 44, &arq_icon_plus, nullptr, ARQ_BTN_SECONDARY, ARQ_PV_CINC);
  const lv_coord_t y4 = y3 + 48;
  V.chip = keep(arq_chip_create(V.scr, LX, y4, FW, V.t4, ARQ_TINT_ACCENT));
  V.start = btn(LX, y4 + 32, FW, CY1 - (y4 + 32), nullptr, "Iniciar autoajuste", ARQ_BTN_PRIMARY, ARQ_PV_START);
}

static void build_running() {
  V.big = card(LX, CY0, HW, 190, ARQ_LAYOUT_BIG);
  arq_card_set_title(V.big, V.t3, ARQ_TEXT_MUTED); arq_card_set_value(V.big, V.v1, ARQ_TEXT_NUM_L); arq_card_set_sub(V.big, V.t2, ARQ_TEXT_MUTED);
  V.list = card(LX + HW + ARQ_GAP, CY0, HW, 100, ARQ_LAYOUT_LIST);
  arq_card_set_rows(V.list, V.rows, 3);
  V.bar = keep(arq_progress_create(V.scr, LX + HW + ARQ_GAP, CY0 + 112, HW, 10));
  btn(LX, CY0 + 198, FW, CY1 - (CY0 + 198), nullptr, "Cancelar", ARQ_BTN_DANGER, ARQ_PV_CANCEL);
}

static void build_result() {
  V.list = card(LX, CY0, FW, 100, ARQ_LAYOUT_LIST);
  arq_card_set_title(V.list, V.t3, ARQ_TEXT_BODY);
  arq_card_set_rows(V.list, V.rows, 3);
  V.apply = btn(LX, CY0 + 108, FW, 70, &arq_icon_check, "Aplicar y guardar", ARQ_BTN_SUCCESS, ARQ_PV_APPLY);
  btn(LX, CY0 + 184, FW, CY1 - (CY0 + 184), nullptr, "Descartar", ARQ_BTN_SECONDARY, ARQ_PV_DISCARD);
}

static void build_msg() {
  V.big = card(LX, CY0, FW, CY1 - CY0, ARQ_LAYOUT_DIALOG);
  arq_card_set_title(V.big, V.t3, ARQ_TEXT_BODY);
  arq_card_set_sub(V.big, V.M.msg, ARQ_TEXT_MUTED);
  btn(LX + 24, CY1 - 12 - 48, FW - 48, 48, nullptr, "Aceptar", ARQ_BTN_PRIMARY, ARQ_PV_OK);
}

static void fill_texts(const arq_pid_view_model_t &m) {
  const char *name = m.heater == ARQ_PID_BED ? "cama" : "hotend";
  switch (m.phase) {
    case ARQ_PVP_SETUP: {
      snprintf(V.v1, sizeof(V.v1), "%d" DEG, (int)m.temp_hot); snprintf(V.v2, sizeof(V.v2), "%d" DEG, (int)m.temp_bed);
      snprintf(V.t1, sizeof(V.t1), "Objetivo   %d" DEG "C", (int)m.target);
      snprintf(V.t2, sizeof(V.t2), "Ciclos   %d", (int)m.cycles);
      char a[16], b[16], c[16];
      arq_pid_fmt_num(a, sizeof a, m.cur_pid[m.heater][0], 2); arq_pid_fmt_num(b, sizeof b, m.cur_pid[m.heater][1], 3); arq_pid_fmt_num(c, sizeof c, m.cur_pid[m.heater][2], 2);
      snprintf(V.t4, sizeof(V.t4), "PID actual %s:  Kp %.9s  Ki %.9s  Kd %.9s", name, a, b, c);
    } break;
    case ARQ_PVP_RUNNING: {
      snprintf(V.t3, sizeof(V.t3), "Ajustando %s", name);
      snprintf(V.v1, sizeof(V.v1), "%d" DEG, (int)m.temp_cur);
      snprintf(V.t2, sizeof(V.t2), "Ciclo %d de %d", (int)(m.cycle < m.cycles ? m.cycle + 1 : m.cycles), (int)m.cycles);
      memset(V.rows, 0, sizeof(V.rows));
      static const char *const lab[3] = { "Objetivo", "Ciclo", "Tiempo" };
      for (uint8_t i = 0; i < 3; ++i) { V.rows[i].label = lab[i]; V.rows[i].dot = ARQ_TINT_AUTO; }
      snprintf(V.rows[0].v1, sizeof(V.rows[0].v1), "%d" DEG "C", (int)m.target);
      snprintf(V.rows[1].v1, sizeof(V.rows[1].v1), "%d / %d", (int)m.cycle, (int)m.cycles);
      arq_pid_fmt_time(V.rows[2].v1, sizeof(V.rows[2].v1), m.elapsed_s);
    } break;
    case ARQ_PVP_RESULT: {
      snprintf(V.t3, sizeof(V.t3), "Resultado %s: nuevo (anterior)", name);
      memset(V.rows, 0, sizeof(V.rows));
      static const char *const lab[3] = { "Kp", "Ki", "Kd" };
      static const uint8_t dec[3] = { 2, 3, 2 };
      for (uint8_t i = 0; i < 3; ++i) {
        V.rows[i].label = lab[i]; V.rows[i].dot = ARQ_TINT_AUTO;
        arq_pid_fmt_num(V.rows[i].v1, sizeof(V.rows[i].v1), m.new_pid[i], dec[i]);
        char o[16]; arq_pid_fmt_num(o, sizeof o, m.old_pid[i], dec[i]);
        snprintf(V.rows[i].v2, sizeof(V.rows[i].v2), "(%.7s)", o);
      }
    } break;
    default:
      snprintf(V.t3, sizeof(V.t3), "%s", m.msg_kind == ARQ_PV_MSG_OK ? "Listo" : m.msg_kind == ARQ_PV_MSG_ERR ? "Error" : "Aviso");
      break;
  }
}

static void apply_dynamic(const arq_pid_view_model_t &m) {
  switch (m.phase) {
    case ARQ_PVP_SETUP:
      for (uint8_t i = 0; i < ARQ_PID_HEATERS; ++i) {
        arq_card_set_selected(V.pick[i], m.heater == i);
        arq_card_set_disabled(V.pick[i], !m.has[i]);
        arq_card_refresh(V.pick[i]);
      }
      arq_card_refresh(V.tmid); arq_card_refresh(V.cmid); arq_card_refresh(V.chip);
      arq_card_set_disabled(V.start, !m.has[m.heater]);
      break;
    case ARQ_PVP_RUNNING:
      arq_card_set_percent(V.bar, m.cycles ? (uint8_t)(m.cycle * 100 / m.cycles) : 0);
      arq_card_refresh(V.big); arq_card_refresh(V.list);
      break;
    default: arq_card_refresh(V.list ? V.list : V.big); break;
  }
}

void arq_pid_view_build(lv_obj_t *scr, lv_event_cb_t cb) {
  arq_pid_view_forget();
  V.scr = scr; V.cb = cb;
  arq_header_create(&V.hd, scr, "Autoajuste PID", false, cb, ARQ_PV_BACK);
}

void arq_pid_view_update(const arq_pid_view_model_t *m) {
  if (!V.scr) return;
  const bool rebuild = V.built != (int8_t)m->phase;
  if (!rebuild && V.valid && memcmp(&V.M, m, sizeof(*m)) == 0) return;
  memcpy(&V.M, m, sizeof(*m)); V.valid = true;
  fill_texts(V.M);
  if (rebuild) {
    panel_clear();
    switch (m->phase) {
      case ARQ_PVP_SETUP:   build_setup(); break;
      case ARQ_PVP_RUNNING: build_running(); break;
      case ARQ_PVP_RESULT:  build_result(); break;
      default:              build_msg(); break;
    }
    V.built = (int8_t)m->phase;
  }
  apply_dynamic(V.M);
}
