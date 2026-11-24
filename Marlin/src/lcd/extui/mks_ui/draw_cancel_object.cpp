/**
 * Arquis XQ1 — Pantalla "Cancelar objeto" (LVGL)
 *
 *  Izquierda : mapa proporcional de la cama (zoom x1/x2/x4) con un marcador por objeto.
 *  Derecha   : lista paginada (30 objetos = 6 paginas) sincronizada con el mapa, estado del elegido y boton
 *              [Cancelar objeto] (pide confirmacion) / [Restaurar objeto].
 *
 * Aqui solo esta la UNION con Marlin (la vista, arquis_objects_view.cpp, no conoce Marlin). Hay TRES situaciones, de las que depende
 * que la pantalla pueda cancelar:
 *   1. SIN manifiesto (G-code de siempre): posiciones aprendidas mientras se imprime; cancelar/restaurar funciona como antes.
 *   2. CON manifiesto ARQUIS_OBJECTS_V1 verificado y COHERENTE con lo que se imprime (mismo archivo, M486 T = N, cajas reales dentro de las
 *      del manifiesto): posiciones y nombres del manifiesto. Cancelar desde la pantalla solo se habilita si el firmware se compila con
 *      ARQ_OBJ_VISUAL_CANCEL_VALIDATED 1; por defecto 0 (la correspondencia fisica no esta validada: falta la prueba de orientacion).
 *   3. Manifiesto presente pero INVALIDO o INCOHERENTE (cama distinta de la de la maquina, CRC, N distinto, indice fuera de rango...):
 *      se muestran las posiciones aprendidas de solo lectura y la cancelacion queda BLOQUEADA.
 * Las llamadas que actuan sobre la maquina son las de siempre: cancelable.cancel_object() / uncancel_object(), con confirmacion.
 */
#include "../../../inc/MarlinConfigPre.h"

#if ALL(HAS_TFT_LVGL_UI, CANCEL_OBJECTS)

#include "draw_ui.h"
#include <lv_conf.h>
#include "../../../inc/MarlinConfig.h"
#include "../../../feature/cancel_object.h"

#include "arquis_objects_view.h"
#include "arquis_objects.h"
#include "arquis_objects_sd.h"
#include "arquis_machine.h"   // ARQUIS: cama del modelo elegido

#include <stdio.h>
#include <string.h>

// PUERTA DE SEGURIDAD: 1 habilita cancelar/restaurar desde la pantalla cuando el manifiesto esta verificado. Cambiar a 1 SOLO despues de
// (a) imprimir la prueba de orientacion fisica (cura/tools/make_orientation_gcode.py) y confirmar que el mapa coincide con la cama real y
// (b) validar el plugin dentro de Cura 5.11 real (cura/VALIDACION_EN_CURA.md). Sin esto, con manifiesto la pantalla solo muestra.
#ifndef ARQ_OBJ_VISUAL_CANCEL_VALIDATED
  #define ARQ_OBJ_VISUAL_CANCEL_VALIDATED 0
#endif

#define ARQ_OBJ_BOX_MARGIN_DMM  60      // la caja aprendida puede salirse 6 mm de la del manifiesto (anchura de extrusion, retracciones)

enum { MODE_LEGACY = 0, MODE_LOCKED, MODE_MANIFEST };

enum { DLG_OBJ_OK = 301, DLG_OBJ_NO };

static lv_obj_t *scr;
static int8_t    sel = -1;
static int8_t    pending = -1;          // objeto cuyo cancelado espera confirmacion
static uint8_t   page, zoom = 1;
static int16_t   last_order = -1;       // orden de impresion del ultimo objeto activo (impresion de un objeto cada vez)
static char      dlg_title[24], dlg_msg[120];
static arq_obj_model_t Mdl;             // el modelo se reconstruye aqui y la vista lo lee por puntero (sin copias)
static uint8_t   mode;
static const char *lock_note;           // texto fijo para la pantalla

// ---- Origen de datos ------------------------------------------------------------------------------------------------------------------
static bool learned_box(uint8_t i, int32_t *cx, int32_t *cy, int32_t *w, int32_t *d, char *shape) {   // lo que ya se imprimio (mm -> 0,1 mm)
  if (!cancelable.has_bbox(i)) return false;
  const int32_t x0 = cancelable.bbox_x0[i], x1 = cancelable.bbox_x1[i], y0 = cancelable.bbox_y0[i], y1 = cancelable.bbox_y1[i];
  *cx = (x0 + x1) * 5; *cy = (y0 + y1) * 5; *w = (x1 - x0) * 10; *d = (y1 - y0) * 10;
  if (*w < 1) *w = 1;
  if (*d < 1) *d = 1;
  *shape = 'R';
  return true;
}

static uint8_t learned_label(uint8_t i, char *out, uint8_t n) { return (uint8_t)snprintf(out, n, "Objeto %d", i + 1); }

static bool manifest_box(uint8_t i, int32_t *cx, int32_t *cy, int32_t *w, int32_t *d, char *shape) {
  if (i >= arq_obj.n) return false;
  const ArqObjRec &r = arq_obj.rec[i];
  *cx = r.cx; *cy = r.cy; *w = r.w; *d = r.d; *shape = (char)r.shape;
  return true;
}

static uint8_t manifest_label(uint8_t i, char *out, uint8_t n) { return arq_obj_label(out, n, i); }

static const arq_obj_src_t SRC_LEARNED  = { learned_box,  learned_label };
static const arq_obj_src_t SRC_MANIFEST = { manifest_box, manifest_label };

// ---- Confianza: cuando se puede usar el manifiesto -------------------------------------------------------------------------------------
// Se evalua en cada refresco (coste: <= 30 comparaciones). Solo puede empeorar mientras dura la impresion (las cajas aprendidas solo crecen).
static void eval_mode() {
  lock_note = nullptr;
  mode = MODE_LEGACY;
  const uint8_t st = arq_man_status();
  if (st == ARQ_MAN_NONE) return;                                                   // G-code sin manifiesto: comportamiento de siempre
  if (st == ARQ_MAN_BAD) {                                                          // presente pero invalido: nunca habilita nada
    mode = MODE_LOCKED;
    lock_note = (arq_man_errors() & ARQ_E_BED) ? "Cama del G-code distinta" : "Manifiesto no v\xC3\xA1lido";
    return;
  }
  // OK: solo vale si es el manifiesto del archivo que se imprime (no uno cargado al elegir otro archivo)
  if (arq_obj.file_id == 0 || arq_obj.file_id != arq_objload_printing_file_id()) return;   // impresion sin SD o de otro archivo: como si no hubiera
  mode = MODE_LOCKED;
  if (arq_obj.org[0] != 'F' || arq_obj.org[1] != 'L') { lock_note = "Orientaci\xC3\xB3n no soportada"; return; }
  if (cancelable.overflow) { lock_note = "\xC3\x8Dndice fuera de rango"; return; }
  if (cancelable.declared_count != (int8_t)arq_obj.n || cancelable.object_count > (int8_t)arq_obj.n) { lock_note = "N\xC2\xBA de objetos distinto"; return; }
  for (uint8_t i = 0; i < arq_obj.n; ++i)                                           // lo realmente impreso debe caer dentro de lo que dice el manifiesto
    if (cancelable.has_bbox(i) && !arq_box_within(arq_obj.rec[i], cancelable.bbox_x0[i], cancelable.bbox_y0[i], cancelable.bbox_x1[i], cancelable.bbox_y1[i], ARQ_OBJ_BOX_MARGIN_DMM)) {
      lock_note = "Posici\xC3\xB3n no coincide";
      return;
    }
  mode = MODE_MANIFEST;
  #if !ARQ_OBJ_VISUAL_CANCEL_VALIDATED
    lock_note = "Cancelaci\xC3\xB3n no validada";
  #endif
}

static bool can_act_from_screen() {
  if (mode == MODE_LEGACY) return true;
  #if ARQ_OBJ_VISUAL_CANCEL_VALIDATED
    return mode == MODE_MANIFEST;
  #else
    return false;
  #endif
}

static void build_model() {
  eval_mode();
  memset(&Mdl, 0, sizeof(Mdl));
  const bool man = (mode == MODE_MANIFEST);
  const int total = cancelable.object_count > 0 ? cancelable.object_count : 0;
  const int n = total > ARQ_OBJ_MAX ? ARQ_OBJ_MAX : total;
  Mdl.src = man ? &SRC_MANIFEST : &SRC_LEARNED;
  Mdl.source = man ? ARQ_SRC_MANIFEST : ARQ_SRC_LEARNED;
  Mdl.n = (uint8_t)n; Mdl.total = (uint8_t)total;
  Mdl.active = (cancelable.active_object >= 0 && cancelable.active_object < n) ? cancelable.active_object : -1;
  if (sel >= n) sel = -1;
  const uint8_t pages = n ? (uint8_t)((n + ARQ_OBJ_ROWS - 1) / ARQ_OBJ_ROWS) : 1;
  if (page >= pages) page = (uint8_t)(pages - 1);
  Mdl.sel = sel;
  Mdl.zoom = zoom;
  Mdl.page = page;
  Mdl.note = lock_note;
  if (man) {
    Mdl.bed = { arq_obj.min_x, arq_obj.min_y, arq_obj.bed_w, arq_obj.bed_d };
    if (Mdl.active >= 0) last_order = arq_obj.rec[Mdl.active].order;
  }
  else Mdl.bed = { 0, 0, (int32_t)(arq_mc_max_pos(0) * 10 + 0.5f), (int32_t)(arq_mc_max_pos(1) * 10 + 0.5f) };   // ARQUIS: cama del modelo
  for (int i = 0; i < n; ++i) {
    const bool canc = cancelable.is_canceled(i);
    if (man) {
      const ArqObjRec &r = arq_obj.rec[i];
      Mdl.state[i] = arq_obj_state(true, true, canc, arq_obj.seq != 0, r.z0, r.ztop, r.order, cancelable.z_print, last_order);
    }
    else Mdl.state[i] = canc ? ARQ_ST_CANCELED : (i == Mdl.active ? ARQ_ST_PRINTING : ARQ_ST_PENDING);
  }
  if (sel < 0 || sel >= n) Mdl.action = ARQ_ACT_NONE;
  else if (!can_act_from_screen()) Mdl.action = ARQ_ACT_LOCKED;
  else Mdl.action = cancelable.is_canceled(sel) ? ARQ_ACT_RESTORE : ARQ_ACT_CANCEL;
}

void lv_refresh_cancel_object() {
  if (!scr) return;
  build_model();
  arq_objects_update(&Mdl);
}

static void dialog_cb(lv_obj_t *obj, lv_event_t event) {
  if (event != LV_EVENT_RELEASED) return;
  const int id = obj->mks_obj_id;
  arq_dialog_close();
  build_model();                                                     // la confianza se vuelve a comprobar AL CONFIRMAR, no solo al abrir el dialogo
  if (id == DLG_OBJ_OK && pending >= 0 && pending < cancelable.object_count && pending < ARQ_OBJ_MAX && can_act_from_screen() && !cancelable.is_canceled(pending))
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
      if (k >= 0) { sel = k; page = (uint8_t)(k / ARQ_OBJ_ROWS); }          // mapa y lista comparten seleccion; la lista salta a la pagina del elegido
      else if (k == ARQ_PICK_PAGE_PREV && page > 0) --page;
      else if (k == ARQ_PICK_PAGE_NEXT) ++page;                             // la vista recorta la pagina a las que existen
      else if (k == ARQ_PICK_ZOOM1) zoom = 1;
      else if (k == ARQ_PICK_ZOOM2) zoom = 2;
      else if (k == ARQ_PICK_ZOOM4) zoom = 4;
    } break;

    case ARQ_OID_ACTION:
      build_model();
      if (Mdl.action == ARQ_ACT_RESTORE) cancelable.uncancel_object(sel);      // restaurar: no destruye nada, sin confirmacion
      else if (Mdl.action == ARQ_ACT_CANCEL) {                                  // cancelar: SIEMPRE pide confirmacion
        pending = sel;
        char nm[24];
        (mode == MODE_MANIFEST ? manifest_label : learned_label)((uint8_t)sel, nm, sizeof(nm));
        snprintf(dlg_title, sizeof(dlg_title), "Cancelar objeto %d", sel + 1);
        snprintf(dlg_msg, sizeof(dlg_msg), "Se dejar\xC3\xA1 de imprimir \"%s\" (objeto %d). Los dem\xC3\xA1s objetos siguen imprimi\xC3\xA9ndose.", nm, sel + 1);
        if (!arq_dialog_open(dlg_title, dlg_msg, "Cancelar objeto", "Volver", true, dialog_cb, DLG_OBJ_OK, DLG_OBJ_NO)) pending = -1;   // sin memoria: no se cancela nada
      }
      break;                                                                    // ACT_LOCKED / ACT_NONE: el boton esta deshabilitado; si llegara un toque, no hace nada
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
  pending = -1; page = 0; zoom = 1; last_order = -1;
  if (sel >= cancelable.object_count) sel = -1;
  if (sel < 0 && cancelable.active_object >= 0) sel = cancelable.active_object;   // preseleccionar el que se imprime
  if (sel >= 0) page = (uint8_t)(sel / ARQ_OBJ_ROWS);
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
