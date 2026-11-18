/**
 * Arquis XQ1 — Pantalla "Cancelar objeto" (LVGL)
 *
 *  Izquierda : mapa de la cama. Cada objeto ya iniciado aparece como un rectangulo en su posicion real
 *              (la posicion se aprende mientras imprime). Verde = imprimiendo, gris = en espera, rojo + X = cancelado.
 *  Derecha   : fichas numeradas de todos los objetos (tambien los que aun no empiezan), estado del seleccionado
 *              y boton [Cancelar objeto] (pide confirmacion) / [Restaurar objeto] si ya estaba cancelado.
 *
 * La version anterior creaba ~70 objetos LVGL y agotaba el heap de 16 KB (la placa se reiniciaba al entrar). Ahora son 6 objetos
 * fijos (arquis_objects_view.cpp) sin importar cuantos objetos tenga el G-code. Aqui solo esta la union con Marlin.
 * Las llamadas que actuan sobre la maquina son las mismas de siempre: cancelable.cancel_object() / uncancel_object().
 */
#include "../../../inc/MarlinConfigPre.h"

#if ALL(HAS_TFT_LVGL_UI, CANCEL_OBJECTS)

#include "draw_ui.h"
#include <lv_conf.h>
#include "../../../inc/MarlinConfig.h"
#include "../../../feature/cancel_object.h"

#include "arquis_objects_view.h"

#include <stdio.h>
#include <string.h>

enum { DLG_OBJ_OK = 301, DLG_OBJ_NO };

static lv_obj_t *scr;
static int8_t    sel = -1;
static int8_t    pending = -1;          // objeto cuyo cancelado espera confirmacion
static char      dlg_title[24], dlg_msg[96];

static void build_model(arq_obj_model_t *m) {
  memset(m, 0, sizeof(*m));
  const int total = cancelable.object_count > 0 ? cancelable.object_count : 0;
  const int n = total > ARQ_OBJ_MAX ? ARQ_OBJ_MAX : total;
  m->n = (uint8_t)n; m->total = (uint8_t)total;
  m->active = (cancelable.active_object >= 0 && cancelable.active_object < n) ? cancelable.active_object : -1;
  m->sel = (sel >= 0 && sel < n) ? sel : -1;
  m->bed_x = (int16_t)X_BED_SIZE; m->bed_y = (int16_t)Y_BED_SIZE;
  for (int i = 0; i < n; ++i) {
    if (cancelable.is_canceled(i)) m->canceled |= (1UL << i);
    if (cancelable.has_bbox(i)) {
      m->has_box |= (1UL << i);
      m->x0[i] = cancelable.bbox_x0[i]; m->y0[i] = cancelable.bbox_y0[i]; m->x1[i] = cancelable.bbox_x1[i]; m->y1[i] = cancelable.bbox_y1[i];
    }
  }
}

void lv_refresh_cancel_object() {
  if (!scr) return;
  arq_obj_model_t m;
  build_model(&m);
  arq_objects_update(&m);
}

static void dialog_cb(lv_obj_t *obj, lv_event_t event) {
  if (event != LV_EVENT_RELEASED) return;
  const int id = obj->mks_obj_id;
  arq_dialog_close();
  if (id == DLG_OBJ_OK && pending >= 0 && pending < cancelable.object_count && !cancelable.is_canceled(pending))
    cancelable.cancel_object(pending);
  pending = -1;
  lv_refresh_cancel_object();
}

static void event_handler(lv_obj_t *obj, lv_event_t event) {
  if (event != LV_EVENT_RELEASED) return;
  if (arq_dialog_is_open()) return;

  switch (obj->mks_obj_id) {
    case ARQ_OID_BACK: goto_previous_ui(); return;

    case ARQ_OID_MAP:
    case ARQ_OID_GRID: {
      lv_indev_t *in = lv_indev_get_act();
      if (!in) return;
      lv_point_t p;
      lv_indev_get_point(in, &p);
      const int8_t k = arq_objects_pick(obj, &p);
      if (k >= 0) sel = k;
    } break;

    case ARQ_OID_ACTION:
      if (sel >= 0 && sel < cancelable.object_count && sel < ARQ_OBJ_MAX) {
        if (cancelable.is_canceled(sel)) cancelable.uncancel_object(sel);      // restaurar: no destruye nada, sin confirmacion
        else {                                                                  // cancelar: SIEMPRE pide confirmacion
          pending = sel;
          snprintf(dlg_title, sizeof(dlg_title), "Cancelar objeto %d", sel + 1);
          snprintf(dlg_msg, sizeof(dlg_msg), "Se dejar\xC3\xA1 de imprimir el objeto %d. Los dem\xC3\xA1s objetos siguen imprimi\xC3\xA9ndose.", sel + 1);
          if (!arq_dialog_open(dlg_title, dlg_msg, "Cancelar objeto", "Volver", true, dialog_cb, DLG_OBJ_OK, DLG_OBJ_NO)) pending = -1;   // sin memoria: no se cancela nada
        }
      }
      break;
  }
  lv_refresh_cancel_object();
}

// Boton del menu Opciones (pantalla original de MKS, hasta que se rediseñe en la etapa 4)
static lv_style_t stOpBtn;

lv_obj_t* lv_cancel_object_button_create(lv_obj_t *par, lv_coord_t x, lv_coord_t y, lv_event_cb_t cb, const int id) {
  static bool done = false;
  if (!done) {
    done = true;
    lv_style_copy(&stOpBtn, &lv_style_plain);
    stOpBtn.body.main_color = stOpBtn.body.grad_color = lv_color_hex(0x2A1218);
    stOpBtn.body.border.color = lv_color_hex(0xEF4444);
    stOpBtn.body.border.width = 2;
    stOpBtn.body.border.opa   = LV_OPA_COVER;
    stOpBtn.body.radius       = 8;
    stOpBtn.body.shadow.width = 0;
    stOpBtn.text.color        = LV_COLOR_WHITE;
  }
  lv_obj_t *b = lv_btn_create(par, x, y, BTN_X_PIXEL, BTN_Y_PIXEL, cb, id, &stOpBtn);
  lv_obj_t *l = lv_label_create_empty(b);
  lv_obj_set_style(l, &tft_style_label_rel);
  lv_label_set_text(l, "Quitar\nobjeto");
  lv_obj_align(l, b, LV_ALIGN_CENTER, 0, 0);
  return b;
}

void lv_draw_cancel_object() {
  scr = lv_screen_create(CANCEL_OBJECT_UI, "");
  arq_screen_apply(scr);
  pending = -1;
  if (sel >= cancelable.object_count) sel = -1;
  if (sel < 0 && cancelable.active_object >= 0) sel = cancelable.active_object;   // preseleccionar el que se imprime
  arq_objects_build(scr, event_handler);
  lv_refresh_cancel_object();
}

void lv_clear_cancel_object() {
  arq_dialog_close();                                // un dialogo en la capa superior sobreviviria al cambio de pantalla
  arq_objects_forget();
  if (scr) lv_obj_del(scr);
  scr = nullptr;
}

#endif // HAS_TFT_LVGL_UI && CANCEL_OBJECTS
