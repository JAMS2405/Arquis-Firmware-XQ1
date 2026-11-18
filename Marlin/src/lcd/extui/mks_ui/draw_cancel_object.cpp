/**
 * Arquis XQ1 — Pantalla "Cancelar objeto" (LVGL)
 *
 *  Izquierda : mapa de la cama. Cada objeto ya iniciado aparece como un rectangulo en su posicion real
 *              (la posicion se aprende mientras imprime). Verde = imprimiendo, gris = otro, rojo + X = cancelado.
 *  Derecha   : fichas numeradas de todos los objetos (tambien los que aun no empiezan), info del seleccionado
 *              y boton [Cancelar] (pide confirmacion) / [Restaurar] si ya estaba cancelado.
 */
#include "../../../inc/MarlinConfigPre.h"

#if ALL(HAS_TFT_LVGL_UI, CANCEL_OBJECTS)

#include "draw_ui.h"
#include <lv_conf.h>
#include "../../../inc/MarlinConfig.h"
#include "../../../feature/cancel_object.h"

#define CO_MAX      16
#define MAP_X        6
#define MAP_Y       40
#define MAP_SZ     266
#define MAP_PAD      3

enum { ID_CO_CHIP0 = 1, ID_CO_RECT0 = 101, ID_CO_CANCEL = 200, ID_CO_BACK = 201 };

static lv_obj_t *scr, *mapPanel, *lblInfo, *btnCancel, *lblCancel;
static lv_obj_t *chip[CO_MAX], *chipLbl[CO_MAX], *rectB[CO_MAX], *rectLbl[CO_MAX];
static int8_t sel = -1;
static bool armed = false;
static millis_t armedAt = 0;

// estilos: [estado 0=pendiente 1=activo 2=cancelado][0=normal 1=seleccionado]
static lv_style_t st[3][2], stPanel, stOpBtn, stBtnRed, stBtnArm, stBtnGreen, stBtnGray, stBtnOff;

static void mk(lv_style_t &s, uint32_t fill, uint32_t border, uint8_t bw, uint8_t radius) {
  lv_style_copy(&s, &lv_style_plain);
  s.body.main_color = s.body.grad_color = lv_color_hex(fill);
  s.body.border.color = lv_color_hex(border);
  s.body.border.width = bw;
  s.body.border.opa   = LV_OPA_COVER;
  s.body.radius       = radius;
  s.body.shadow.width = 0;
  s.text.color        = LV_COLOR_WHITE;
}

static void init_styles() {
  static bool done = false;
  if (done) return;
  done = true;
  mk(st[0][0], 0x1E2A44, 0x3E5C86, 1, 4);  mk(st[0][1], 0x1E2A44, 0xFFFFFF, 3, 4);  // pendiente
  mk(st[1][0], 0x16A34A, 0x86EFAC, 1, 4);  mk(st[1][1], 0x16A34A, 0xFFFFFF, 3, 4);  // imprimiendo
  mk(st[2][0], 0x991B1B, 0xF87171, 1, 4);  mk(st[2][1], 0x991B1B, 0xFFFFFF, 3, 4);  // cancelado
  mk(stPanel,    0x0F1A2E, 0x2B4A73, 2, 6);
  mk(stOpBtn,    0x2A1218, 0xEF4444, 2, 8);   // boton del menu Opciones
  mk(stBtnRed,   0xDC2626, 0xFCA5A5, 1, 6);
  mk(stBtnArm,   0xEA580C, 0xFFEDD5, 3, 6);
  mk(stBtnGreen, 0x16A34A, 0x86EFAC, 1, 6);
  mk(stBtnGray,  0x1E2A44, 0x5C7FA8, 1, 6);
  mk(stBtnOff,   0x141C2E, 0x2B3A57, 1, 6);
}

static uint8_t state_of(const int8_t i) {
  if (cancelable.is_canceled(i)) return 2;
  return (i == cancelable.active_object) ? 1 : 0;
}

static void set_lbl(lv_obj_t *lbl, lv_obj_t *par, const char *txt) {
  lv_obj_set_style(lbl, &tft_style_label_rel);
  lv_label_set_text(lbl, txt);
  lv_obj_align(lbl, par, LV_ALIGN_CENTER, 0, 0);
}

void lv_refresh_cancel_object() {
  if (!scr) return;
  const int n = cancelable.object_count > CO_MAX ? CO_MAX : cancelable.object_count;
  const float scale = (float)(MAP_SZ - 2 * MAP_PAD) / (float)((int)X_BED_SIZE > (int)Y_BED_SIZE ? (int)X_BED_SIZE : (int)Y_BED_SIZE);
  char buf[24];

  if (armed && ELAPSED(millis(), armedAt + 4000UL)) armed = false;   // la confirmacion caduca a los 4 s

  for (int i = 0; i < CO_MAX; i++) {
    if (i >= n) {
      lv_obj_set_hidden(chip[i], true);
      lv_obj_set_hidden(rectB[i], true);
      continue;
    }
    const uint8_t s = state_of(i);
    const uint8_t isel = (i == sel) ? 1 : 0;

    lv_obj_set_hidden(chip[i], false);
    lv_btn_set_style_both(chip[i], &st[s][isel]);
    if (s == 2) strcpy(buf, "X"); else sprintf(buf, "%d", i + 1);
    set_lbl(chipLbl[i], chip[i], buf);

    if (cancelable.has_bbox(i)) {
      const int16_t x0 = cancelable.bbox_x0[i], x1 = cancelable.bbox_x1[i];
      const int16_t y0 = cancelable.bbox_y0[i], y1 = cancelable.bbox_y1[i];
      int16_t w = (int16_t)((x1 - x0) * scale), h = (int16_t)((y1 - y0) * scale);
      const int16_t px = MAP_PAD + (int16_t)(x0 * scale), py = MAP_PAD + (int16_t)((Y_BED_SIZE - y1) * scale);
      int16_t ox = 0, oy = 0;
      if (w < 22) { ox = (22 - w) / 2; w = 22; }      // objetos chicos: tamano minimo tocable, centrado
      if (h < 22) { oy = (22 - h) / 2; h = 22; }
      lv_obj_set_pos(rectB[i], px - ox, py - oy);
      lv_obj_set_size(rectB[i], w, h);
      lv_obj_set_hidden(rectB[i], false);
      lv_btn_set_style_both(rectB[i], &st[s][isel]);
      set_lbl(rectLbl[i], rectB[i], buf);
    }
    else
      lv_obj_set_hidden(rectB[i], true);
  }

  // info + boton
  if (n == 0) {
    lv_label_set_text(lblInfo, "Sin objetos");
    lv_btn_set_style_both(btnCancel, &stBtnOff);
    set_lbl(lblCancel, btnCancel, "Cancelar");
  }
  else if (sel < 0 || sel >= n) {
    lv_label_set_text(lblInfo, "Toca un objeto");
    lv_btn_set_style_both(btnCancel, &stBtnOff);
    set_lbl(lblCancel, btnCancel, "Cancelar");
  }
  else {
    const uint8_t s = state_of(sel);
    sprintf(buf, "#%d %s", sel + 1, s == 2 ? "CANC." : s == 1 ? "ACTIVO" : "espera");
    lv_label_set_text(lblInfo, buf);
    if (s == 2) { lv_btn_set_style_both(btnCancel, &stBtnGreen); set_lbl(lblCancel, btnCancel, "Restaurar"); }
    else if (armed) { lv_btn_set_style_both(btnCancel, &stBtnArm); set_lbl(lblCancel, btnCancel, "Seguro?"); }
    else { lv_btn_set_style_both(btnCancel, &stBtnRed); set_lbl(lblCancel, btnCancel, "Cancelar"); }
  }
}

static void event_handler(lv_obj_t *obj, lv_event_t event) {
  if (event != LV_EVENT_RELEASED) return;
  const int id = obj->mks_obj_id;

  if (id >= ID_CO_CHIP0 && id < ID_CO_CHIP0 + CO_MAX) { sel = id - ID_CO_CHIP0; armed = false; }
  else if (id >= ID_CO_RECT0 && id < ID_CO_RECT0 + CO_MAX) { sel = id - ID_CO_RECT0; armed = false; }
  else if (id == ID_CO_CANCEL) {
    if (sel >= 0 && sel < cancelable.object_count) {
      if (cancelable.is_canceled(sel)) { cancelable.uncancel_object(sel); armed = false; }
      else if (!armed) { armed = true; armedAt = millis(); }
      else { cancelable.cancel_object(sel); armed = false; }
    }
  }
  else if (id == ID_CO_BACK) { goto_previous_ui(); return; }

  lv_refresh_cancel_object();
}

lv_obj_t* lv_cancel_object_button_create(lv_obj_t *par, lv_coord_t x, lv_coord_t y, lv_event_cb_t cb, const int id) {
  init_styles();
  lv_obj_t *b = lv_btn_create(par, x, y, BTN_X_PIXEL, BTN_Y_PIXEL, cb, id, &stOpBtn);
  lv_obj_t *l = lv_label_create_empty(b);
  lv_obj_set_style(l, &tft_style_label_rel);
  lv_label_set_text(l, "Quitar\nobjeto");
  lv_obj_align(l, b, LV_ALIGN_CENTER, 0, 0);
  return b;
}

void lv_draw_cancel_object() {
  init_styles();
  scr = lv_screen_create(CANCEL_OBJECT_UI);
  armed = false;
  if (sel >= cancelable.object_count) sel = -1;
  if (sel < 0 && cancelable.active_object >= 0) sel = cancelable.active_object;   // preseleccionar el que se imprime

  // mapa de cama
  mapPanel = lv_obj_create(scr, nullptr);
  lv_obj_set_style(mapPanel, &stPanel);
  lv_obj_set_pos(mapPanel, MAP_X, MAP_Y);
  lv_obj_set_size(mapPanel, MAP_SZ, MAP_SZ);
  for (int i = 0; i < CO_MAX; i++) {
    rectB[i] = lv_btn_create(mapPanel, 0, 0, 22, 22, event_handler, ID_CO_RECT0 + i, &st[0][0]);
    rectLbl[i] = lv_label_create_empty(rectB[i]);
    lv_obj_set_hidden(rectB[i], true);
  }

  // fichas 4 x 4
  for (int i = 0; i < CO_MAX; i++) {
    const lv_coord_t cx = 280 + (i % 4) * 50, cy = 40 + (i / 4) * 48;
    chip[i] = lv_btn_create(scr, cx, cy, 46, 44, event_handler, ID_CO_CHIP0 + i, &st[0][0]);
    chipLbl[i] = lv_label_create_empty(chip[i]);
    lv_obj_set_hidden(chip[i], true);
  }

  lblInfo = lv_label_create(scr, 280, 236, "");

  btnCancel = lv_btn_create(scr, 280, 270, 134, 46, event_handler, ID_CO_CANCEL, &stBtnOff);
  lblCancel = lv_label_create_empty(btnCancel);
  lv_obj_t *btnBack = lv_btn_create(scr, 418, 270, 58, 46, event_handler, ID_CO_BACK, &stBtnGray);
  lv_obj_t *lblBack = lv_label_create_empty(btnBack);
  lv_obj_set_style(lblBack, &tft_style_label_rel);
  lv_label_set_text(lblBack, "<");
  lv_obj_align(lblBack, btnBack, LV_ALIGN_CENTER, 0, 0);

  lv_refresh_cancel_object();
}

void lv_clear_cancel_object() {
  lv_obj_del(scr);
  scr = nullptr;
}

#endif // HAS_TFT_LVGL_UI && CANCEL_OBJECTS
