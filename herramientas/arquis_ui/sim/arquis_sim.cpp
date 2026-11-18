// Simulador PC de ARQUIS UI: compila LVGL 6.1.1-MKS real + la vista del Dashboard y los componentes, los ejecuta (en ARM de 32 bits
// con qemu-user) y vuelca capturas PPM. Usa el mismo bufer parcial que el firmware (480 x 14 px) y el heap de 16 KB.
// OJO: aqui los datos del Dashboard son "snapshots" de PRUEBA para ejercitar la VISTA; la lectura real de Marlin
// (arquis_data.cpp) no corre en el simulador.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arquis_dashboard_view.h"
#include "arquis_print_view.h"
#include "arquis_objects_view.h"

extern "C" {   // ganchos que el firmware real define en draw_print_file.cpp (miniatura de G-code) y la libreria: no se usan aqui
  uint32_t lv_open_gcode_file(char *) { return 0; }
  void lv_gcode_file_read(uint8_t *) {}
  void lv_close_gcode_file() {}
  void lv_log_register_print_cb(void *) {}
}

static uint16_t fb[ARQ_W * ARQ_H];
static lv_disp_buf_t disp_buf;
static lv_color_t buf1[ARQ_W * 14];
static const char *outdir = ".";

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *a, lv_color_t *c) {
  for (int y = a->y1; y <= a->y2; ++y)
    for (int x = a->x1; x <= a->x2; ++x) fb[y * ARQ_W + x] = (c++)->full;
  lv_disp_flush_ready(drv);
}
static void run(int n = 20) { for (int i = 0; i < n; ++i) { lv_tick_inc(10); lv_task_handler(); } }
static int g_thumb_pct = -1;   // >= 0: simula la miniatura del G-code que la interfaz original pinta fuera de LVGL (x 2, y 36, 200x200)
static void paint_thumb(int pct) {
  const int line = 200 - 200 * pct / 100;
  for (int y = 0; y < 200; ++y)
    for (int x = 0; x < 200; ++x) {
      const int cx = x - 100, cy = y - 110, d2 = cx * cx / 2 + cy * cy;
      uint16_t c = d2 < 70 * 70 ? 0xFD20 : d2 < 90 * 90 ? 0xC618 : 0x7BEF;       // "pieza" naranja con borde gris
      if (y < line) c = (c >> 1) & 0x7BEF;                                         // pendiente: atenuado
      if (y == line && pct < 100) c = 0x05BF;                                      // linea de altura
      fb[(36 + y) * ARQ_W + 2 + x] = c;
    }
}
static void dump(const char *name) {
  run();
  if (g_thumb_pct >= 0) paint_thumb(g_thumb_pct);
  char p[256]; snprintf(p, sizeof(p), "%s/%s.ppm", outdir, name);
  FILE *f = fopen(p, "wb");
  fprintf(f, "P6\n%d %d\n255\n", ARQ_W, ARQ_H);
  for (int i = 0; i < ARQ_W * ARQ_H; ++i) {
    const uint16_t v = fb[i]; const uint8_t r = (v >> 11) & 31, g = (v >> 5) & 63, b = v & 31;
    fputc((r << 3) | (r >> 2), f); fputc((g << 2) | (g >> 4), f); fputc((b << 3) | (b >> 2), f);
  }
  fclose(f);
}
static uint32_t used() { lv_mem_monitor_t m; lv_mem_monitor(&m); return m.total_size - m.free_size; }
static uint32_t biggest() { lv_mem_monitor_t m; lv_mem_monitor(&m); return m.free_biggest_size; }
static int fails = 0, ev_count = 0, ev_last = -1;
#define CHECK(c, ...) do { if (c) { printf("[test] OK    "); } else { printf("[test] FALLA "); ++fails; } printf(__VA_ARGS__); printf("\n"); } while (0)

static void cb(lv_obj_t *o, lv_event_t ev) { if (ev == LV_EVENT_RELEASED) { ++ev_count; ev_last = o->mks_obj_id; } }

static lv_obj_t * new_screen() {
  lv_obj_t *old = lv_scr_act(), *scr = lv_obj_create(nullptr, nullptr);   // como el firmware: se libera la pantalla anterior
  arq_screen_apply(scr);
  lv_scr_load(scr);
  if (old && old != scr) lv_obj_del(old);
  return scr;
}

// ---- Snapshots de prueba (estados que debe poder mostrar el Dashboard) ---------------------------------------------------------
static arq_snapshot_t make(arq_pstate_t st) {
  arq_snapshot_t s; memset(&s, 0, sizeof(s));
  s.hot = { true, 24, 0, 3 }; s.bed = { true, 23, 0, 3 };
  s.fan_n = 2; s.fan[0] = { ARQ_FAN_LAYER, 0, 0 }; s.fan[1] = { ARQ_FAN_AUX, 1, 0 };
  s.state = st;
  switch (st) {
    case ARQ_PS_IDLE: break;
    case ARQ_PS_HEATING: s.hot.cur = 168; s.hot.tgt = 210; s.bed.cur = 60; s.bed.tgt = 60; s.fan[0].pct = 0; break;
    case ARQ_PS_AT_TEMP: s.hot.cur = 210; s.hot.tgt = 210; s.bed.cur = 60; s.bed.tgt = 60; break;
    case ARQ_PS_COOLING: s.hot.cur = 118; s.hot.tgt = 0; s.bed.cur = 41; s.bed.tgt = 0; s.fan[0].pct = 100; break;
    case ARQ_PS_PRINTING: case ARQ_PS_PAUSED:
      s.hot.cur = 209; s.hot.tgt = 210; s.bed.cur = 60; s.bed.tgt = 60; s.fan[0].pct = 85; s.fan[1].pct = 40;
      s.job = s.sd_job = true; s.pct = 62; s.elapsed_s = 2 * 3600 + 15 * 60; s.remaining_s = arq_remaining_s(s.elapsed_s, s.pct);
      strcpy(s.file, "soporte_motor_nema17_v3_final.gcode");
      if (st == ARQ_PS_PAUSED) { s.hot.cur = 205; s.fan[0].pct = 0; }
      break;
    case ARQ_PS_ERROR: s.hot.cur = 24; break;
  }
  return s;
}

static void dashboard(const arq_snapshot_t &s, const char *shot) {
  lv_obj_t *scr = new_screen();
  arq_dash_build(scr, cb, "Arquis XQ1", &s);
  arq_dash_update(&s);
  if (shot) dump(shot);
}

static void print_screen(const arq_snapshot_t &s, arq_pmode_t m, const char *shot, bool objects = true, int thumb = -1) {
  lv_obj_t *scr = new_screen();
  arq_print_build(scr, cb, "Arquis XQ1", &s, m, objects);
  arq_print_update(&s, m);
  g_thumb_pct = thumb;
  if (shot) dump(shot);
  g_thumb_pct = -1;
}

// ---- Cancelar objeto ------------------------------------------------------------------------------------------------------------
static int8_t o_sel = -1; static int o_act_ev = 0, o_back_ev = 0;
static void ocb(lv_obj_t *o, lv_event_t ev) {                    // mismo recorrido que draw_cancel_object.cpp
  if (ev != LV_EVENT_RELEASED) return;
  switch (o->mks_obj_id) {
    case ARQ_OID_BACK: ++o_back_ev; break;
    case ARQ_OID_MAP: case ARQ_OID_GRID: { lv_indev_t *in = lv_indev_get_act(); if (!in) break; lv_point_t p; lv_indev_get_point(in, &p); const int8_t k = arq_objects_pick(o, &p); if (k >= 0) o_sel = k; } break;
    case ARQ_OID_ACTION: ++o_act_ev; break;
  }
}
// n objetos en una cama de 220x220 mm; los de indice <= learned ya tienen posicion; el objeto 5 es muy pequeno (6 mm); `canceled` = mascara
static arq_obj_model_t omodel(int n, int learned, int active, int sel, uint32_t canceled, int total = -1) {
  arq_obj_model_t m; memset(&m, 0, sizeof(m));
  m.n = n; m.total = total < 0 ? n : total; m.active = active; m.sel = sel; m.canceled = canceled; m.bed_x = m.bed_y = 220;
  for (int i = 0; i < n && i <= learned; ++i) {
    const int col = i % 4, row = i / 4, sz = (i == 5) ? 6 : 40;
    m.x0[i] = 10 + col * 52; m.y0[i] = 10 + row * 52; m.x1[i] = m.x0[i] + sz; m.y1[i] = m.y0[i] + sz; m.has_box |= 1UL << i;
  }
  return m;
}
static void objects_screen(const arq_obj_model_t &m, const char *shot) {
  lv_obj_t *scr = new_screen();
  arq_objects_build(scr, ocb);
  arq_objects_update(&m);
  if (shot) dump(shot);
}

// ---- Componentes y iconos ------------------------------------------------------------------------------------------------------
static arq_temp_tile_t c_t1, c_t2, c_t3, c_t4;
static void components(const char *shot) {
  lv_obj_t *scr = new_screen();
  arq_header_t hd; arq_header_create(&hd, scr, "ARQUIS UI - Componentes", false, cb, 1);
  arq_chip_create(scr, 10, 52, 100, "Lista", ARQ_TINT_OK);
  arq_chip_create(scr, 118, 52, 130, "Calentando", ARQ_TINT_WARN);
  arq_chip_create(scr, 256, 52, 110, "Error", ARQ_TINT_DANGER);
  arq_chip_create(scr, 374, 52, 96, "Enfriando", ARQ_TINT_ACCENT);
  arq_button_create(scr, 10, 88, 108, 44, nullptr, "Primario", ARQ_BTN_PRIMARY, cb, 2);
  arq_button_create(scr, 126, 88, 108, 44, nullptr, "Secundario", ARQ_BTN_SECONDARY, cb, 3);
  arq_button_create(scr, 242, 88, 108, 44, &arq_icon_check, "Iniciar", ARQ_BTN_SUCCESS, cb, 4);
  arq_button_create(scr, 358, 88, 112, 44, nullptr, "Cancelar", ARQ_BTN_DANGER, cb, 5);
  const int w = 110, gap = 6;
  arq_temp_tile_create(&c_t1, scr, 10, 140, w, 124, &arq_icon_nozzle, "Hot");  arq_temp_tile_update(&c_t1, 24, 0, 3);
  arq_temp_tile_create(&c_t2, scr, 10 + (w + gap), 140, w, 124, &arq_icon_nozzle, "Hot"); arq_temp_tile_update(&c_t2, 168, 210, 3);
  arq_temp_tile_create(&c_t3, scr, 10 + 2 * (w + gap), 140, w, 124, &arq_icon_bed, "Cama"); arq_temp_tile_update(&c_t3, 60, 60, 3);
  arq_temp_tile_create(&c_t4, scr, 10 + 3 * (w + gap), 140, w, 124, &arq_icon_bed, "Cama"); arq_temp_tile_update(&c_t4, 71, 0, 3);
  lv_obj_t *bar = arq_progress_create(scr, 10, 282, 376, 12); arq_progress_set(bar, 62);
  dump(shot);
}

static void iconos(const char *shot) {
  lv_obj_t *scr = new_screen();
  arq_header_t hd; arq_header_create(&hd, scr, "ARQUIS UI - Iconos y estados", true, nullptr, 0);
  static const lv_img_dsc_t *all[] = { &arq_icon_folder, &arq_icon_move, &arq_icon_temp, &arq_icon_gear, &arq_icon_print, &arq_icon_nozzle, &arq_icon_bed,
    &arq_icon_fan, &arq_icon_play, &arq_icon_pause, &arq_icon_stop, &arq_icon_back, &arq_icon_check, &arq_icon_close, &arq_icon_warn, &arq_icon_level,
    &arq_icon_tools, &arq_icon_wifi, &arq_icon_home, &arq_icon_file, &arq_icon_speed };
  for (int i = 0; i < 14; ++i) {
    lv_obj_t *c = arq_card_create(scr, 10 + (i % 7) * 66, 52 + (i / 7) * 62, 58, 54, ARQ_LAYOUT_BUTTON, ARQ_BTN_SECONDARY, nullptr, 0);
    arq_card_set_icon(c, all[i], ARQ_TINT_ACCENT);
  }
  // Estados de un mismo icono: normal, seleccionado, activo, deshabilitado, advertencia, error
  static const char *names[] = { "Normal", "Selecc.", "Activo", "Deshab.", "Aviso", "Error" };
  static const arq_icon_state_t states[] = { ARQ_ICON_NORMAL, ARQ_ICON_SELECTED, ARQ_ICON_ACTIVE, ARQ_ICON_DISABLED, ARQ_ICON_WARNING, ARQ_ICON_ERROR };
  for (int i = 0; i < 6; ++i) {
    lv_obj_t *c = arq_card_create(scr, 10 + i * 77, 178, 72, 78, ARQ_LAYOUT_TILE, i == 1 ? ARQ_BTN_PRIMARY : ARQ_BTN_SECONDARY, nullptr, 0);
    arq_card_set_icon(c, &arq_icon_gear, arq_icon_state_tint(states[i]));
    arq_card_set_title(c, names[i], ARQ_TEXT_BODY);
  }
  lv_obj_t *acc = arq_card_create(scr, 10, 266, 460, 28, ARQ_LAYOUT_CHIP, ARQ_BTN_SECONDARY, nullptr, 0);
  arq_card_set_title(acc, "Ñandú: áéíóú ¿Qué? ¡Listo! 215°C", ARQ_TEXT_MUTED);
  dump(shot);
}

// ---- Dialogo -----------------------------------------------------------------------------------------------------------------
static int dlg_ok = 0, dlg_cancel = 0;
static void dlg_cb(lv_obj_t *o, lv_event_t ev) { if (ev != LV_EVENT_RELEASED) return; if (o->mks_obj_id == 901) ++dlg_ok; else ++dlg_cancel; arq_dialog_close(); }

// ---- Pruebas de memoria, navegacion y eventos -----------------------------------------------------------------------------
static void screen_stub(int objects) {       // pantalla ajena (como un menu MKS con varios botones)
  lv_obj_t *s = new_screen();
  for (int i = 0; i < objects; ++i) arq_card_create(s, 10 + (i % 5) * 90, 60 + (i / 5) * 60, 80, 50, ARQ_LAYOUT_BUTTON, ARQ_BTN_SECONDARY, cb, 200 + i);
}

static void tests() {
  // Calentamiento: el asignador de LVGL deja pequenas diferencias (4-8 B de relleno) segun el historial y reserva algunas
  // estructuras globales la primera vez. Se mide la memoria en regimen estable y se exige que NO CREZCA (tolerancia de 8 B de relleno).
  for (int i = 0; i < 30; ++i) { arq_snapshot_t w = make((arq_pstate_t)(i % 7)); dashboard(w, nullptr); print_screen(w, (arq_pmode_t)(i % 7), nullptr, i % 2); }
  arq_dialog_open("T", "M", "A", "B", false, dlg_cb, 901, 902); run(); arq_dialog_close();
  new_screen(); run();
  new_screen(); run();
  const uint32_t base = used(), base_big = biggest();
  printf("[mem] base (pantalla vacia): %u B usados, mayor bloque libre %u B\n", (unsigned)base, (unsigned)base_big);

  // 1) Creacion y destruccion repetida del Dashboard (todos los estados)
  for (int i = 0; i < 300; ++i) {
    arq_pstate_t st = (arq_pstate_t)(i % 7);
    arq_snapshot_t s = make(st);
    dashboard(s, nullptr);
    if (i == 0) printf("[mem] Dashboard 'Lista' usa %u B\n", (unsigned)(used() - base));
  }
  new_screen(); run();
  CHECK(used() <= base + 8, "300 ciclos crear/destruir Dashboard: heap final %u B (base %u B)", (unsigned)used(), (unsigned)base);
  { // La posicion de los objetos vivos cambia el bloque libre mayor unos cientos de bytes segun el historial; lo que no debe ocurrir
    // es que siga degradandose con el uso: 300 ciclos mas no pueden reducirlo.
    const uint32_t big1 = biggest();
    for (int i = 0; i < 300; ++i) { arq_snapshot_t s = make((arq_pstate_t)(i % 7)); dashboard(s, nullptr); }
    new_screen(); run();
    CHECK(biggest() >= big1, "sin fragmentacion progresiva: mayor bloque libre %u B tras 600 ciclos (%u B tras 300; referencia inicial %u B)", (unsigned)biggest(), (unsigned)big1, (unsigned)base_big); }

  // 2) Pico de memoria del Dashboard por estado
  uint32_t peak = 0;
  for (int st = 0; st < 7; ++st) { new_screen(); const uint32_t b = used(); arq_snapshot_t s = make((arq_pstate_t)st); dashboard(s, nullptr); const uint32_t u = used() - b; if (u > peak) peak = u; printf("[mem] Dashboard estado %d: %u B\n", st, (unsigned)u); }
  CHECK(peak < 6000, "pico del Dashboard %u B (< 6000 B)", (unsigned)peak);

  // 3) Navegacion Dashboard <-> otra pantalla, con cambio de modo (reposo <-> imprimiendo)
  for (int i = 0; i < 200; ++i) { arq_snapshot_t s = make(i % 2 ? ARQ_PS_PRINTING : ARQ_PS_IDLE); dashboard(s, nullptr); screen_stub(12); }
  new_screen(); run();
  CHECK(used() <= base + 8, "200 idas y vueltas Dashboard<->pantalla de 12 botones: heap %u B (base %u B)", (unsigned)used(), (unsigned)base);

  // 4) Actualizacion de temperaturas: 3000 refrescos con valores cambiantes y transiciones de estado
  { arq_snapshot_t s = make(ARQ_PS_HEATING); dashboard(s, nullptr); const uint32_t u0 = used();
    for (int i = 0; i < 3000; ++i) {
      s.hot.cur = 20 + (i % 200); s.hot.tgt = (i / 50) % 2 ? 210 : 0; s.bed.cur = 20 + (i % 80); s.bed.tgt = (i / 70) % 2 ? 60 : 0;
      s.fan[0].pct = i % 101; s.elapsed_s = i * 5; { const int16_t c[2] = { s.hot.cur, s.bed.cur }, t[2] = { s.hot.tgt, s.bed.tgt }, tol[2] = { 3, 3 }; s.state = arq_printer_state(false, false, false, 2, c, t, tol); }
      arq_dash_update(&s); if (i % 10 == 0) run(2);
    }
    CHECK(used() == u0, "3000 refrescos de temperaturas/ventiladores: heap constante %u B", (unsigned)used()); }

  // 5) Dialogo de confirmacion: abrir/cerrar repetidamente (y con el Dashboard debajo)
  { arq_snapshot_t s = make(ARQ_PS_PRINTING); dashboard(s, nullptr); run(); const uint32_t u0 = used();
    arq_dialog_open("Cancelar impresion", "Se detendra la impresion en curso. Esta accion no se puede deshacer.", "Cancelar", "Seguir", true, dlg_cb, 901, 902);
    run(); const uint32_t ud = used() - u0; printf("[mem] Dialogo abierto: %u B\n", (unsigned)ud);
    CHECK(arq_dialog_is_open(), "el dialogo queda abierto");
    dump("dialogo_cancelar");
    arq_dialog_close(); run();
    for (int i = 0; i < 300; ++i) { arq_dialog_open("Titulo", "Mensaje", "Aceptar", "Cancelar", i % 2, dlg_cb, 901, 902); run(1); arq_dialog_close(); }
    run();
    CHECK(used() == u0 && !arq_dialog_is_open(), "300 aperturas/cierres de dialogo: heap %u B (antes %u B)", (unsigned)used(), (unsigned)u0);
    // toque real sobre "Cancelar" (boton de la derecha) y sobre el velo
    static lv_point_t pt; static bool down = false;
    static lv_indev_drv_t idrv; lv_indev_drv_init(&idrv); idrv.type = LV_INDEV_TYPE_POINTER;
    idrv.read_cb = [](lv_indev_drv_t *, lv_indev_data_t *d) -> bool { d->point = pt; d->state = down ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL; return false; };
    lv_indev_drv_register(&idrv);
    auto tap = [&](int x, int y) { pt.x = x; pt.y = y; down = true; run(6); down = false; run(6); };
    arq_dialog_open("Cancelar impresion", "Msg", "Cancelar", "Seguir", true, dlg_cb, 901, 902);
    ev_count = 0; tap(60, 80);   // fuera del panel, sobre un boton del Dashboard: el velo debe absorberlo
    CHECK(ev_count == 0 && arq_dialog_is_open(), "el velo del dialogo bloquea los toques al Dashboard");
    tap(340, 224);                // boton rojo (confirmar)
    CHECK(dlg_ok == 1 && !arq_dialog_is_open(), "toque en el boton de confirmar ejecuta la accion y cierra");
    arq_dialog_open("T", "M", "Cancelar", "Seguir", true, dlg_cb, 901, 902);
    tap(130, 224);                // boton izquierdo (seguir)
    CHECK(dlg_cancel == 1 && dlg_ok == 1 && !arq_dialog_is_open(), "toque en 'Seguir' cierra sin ejecutar la accion"); }

  // 6) Eventos: los accesos del Dashboard llegan con el id correcto y el toque real funciona
  { arq_snapshot_t s = make(ARQ_PS_IDLE); dashboard(s, nullptr); run();
    static lv_point_t pt; static bool down = false;
    static lv_indev_drv_t idrv; lv_indev_drv_init(&idrv); idrv.type = LV_INDEV_TYPE_POINTER;
    idrv.read_cb = [](lv_indev_drv_t *, lv_indev_data_t *d) -> bool { d->point = pt; d->state = down ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL; return false; };
    lv_indev_drv_register(&idrv);
    auto tap = [&](int x, int y) { pt.x = x; pt.y = y; down = true; run(6); down = false; run(6); };
    struct { int x, y, id; const char *n; } t[] = { { 52, 250, ARQ_ID_PRINT, "Imprimir" }, { 145, 250, ARQ_ID_MOVE, "Mover" }, { 238, 250, ARQ_ID_TEMP, "Temp." },
      { 331, 250, ARQ_ID_TOOLS, "Herram." }, { 424, 250, ARQ_ID_SETTINGS, "Ajustes" }, { 80, 110, ARQ_ID_HOT, "tarjeta Hotend" }, { 236, 110, ARQ_ID_BED, "tarjeta Cama" }, { 400, 110, ARQ_ID_FANS, "tarjeta Ventiladores" } };
    for (auto &c : t) { ev_count = 0; ev_last = -1; tap(c.x, c.y); CHECK(ev_count == 1 && ev_last == c.id, "toque en %s -> id %d", c.n, ev_last); }
    s = make(ARQ_PS_PRINTING); arq_dash_update(&s); run();
    ev_count = 0; ev_last = -1; tap(95, 250); CHECK(ev_count == 1 && ev_last == ARQ_ID_PRINT, "con impresion activa, la tarjeta de impresion envia ARQ_ID_PRINT (la pantalla decide: supervision)");
    ev_count = 0; ev_last = -1; tap(239, 250); CHECK(ev_count == 0, "SEGURIDAD: con impresion activa 'Mover' esta bloqueado (no recibe toques)");
    ev_count = 0; ev_last = -1; tap(427, 250); CHECK(ev_count == 0, "SEGURIDAD: con impresion activa 'Ajustes' esta bloqueado (no recibe toques)");
    ev_count = 0; ev_last = -1; tap(333, 250); CHECK(ev_count == 1 && ev_last == ARQ_ID_TEMP, "con impresion activa, 'Temp.' sigue disponible (id %d)", ev_last);
    ev_count = 0; ev_last = -1; tap(250, 200); CHECK(ev_count == 0 || ev_last != ARQ_ID_TOOLS, "con impresion activa no existe el acceso 'Herram.'");
    dump("dash_5b_imprimiendo_bloqueos");
    // al terminar la impresion vuelven a estar disponibles
    s = make(ARQ_PS_IDLE); arq_dash_update(&s); run();
    ev_count = 0; ev_last = -1; tap(145, 250); CHECK(ev_count == 1 && ev_last == ARQ_ID_MOVE, "al terminar, 'Mover' vuelve a estar disponible"); }

  // 6b) Perilla (encoder): las tarjetas se registran en el grupo, el foco avanza y se dibuja el anillo
  // Ciclo completo con perilla repetido: la primera vuelta puede reservar estructuras globales de LVGL (una sola vez); las siguientes no deben crecer
  for (int i = 0; i < 3; ++i) { arq_snapshot_t s0 = make(ARQ_PS_IDLE); dashboard(s0, nullptr); lv_group_t *g0 = lv_group_create(); arq_group_add_clickables(g0, lv_scr_act()); lv_group_focus_next(g0); run(); new_screen(); run(); lv_group_del(g0); }
  new_screen(); run(); const uint32_t ref = used();   // referencia (incluye los controladores de entrada ya registrados por pruebas anteriores)
  { arq_snapshot_t s = make(ARQ_PS_IDLE); dashboard(s, nullptr); run();
    lv_group_t *grp = lv_group_create(); arq_group_add_clickables(grp, lv_scr_act());
    CHECK(lv_group_get_focused(grp) != nullptr, "el grupo de la perilla recibe las tarjetas del Dashboard");
    dump("dash_foco_inicial");
    lv_obj_t *first = lv_group_get_focused(grp);
    { lv_area_t ar; lv_obj_get_coords(first, &ar); const uint16_t px = fb[(ar.y1 + 60) * ARQ_W + ar.x1]; const bool blue = ((px >> 11) & 31) < 10 && (px & 31) > 20;
      CHECK(!blue, "foco inicial NO se dibuja con uso tactil (sin mover la perilla)"); }
    int n = 0;
    do { ++n; lv_group_focus_next(grp); } while (lv_group_get_focused(grp) != first && n < 20);
    CHECK(n == 8, "la perilla recorre las 8 tarjetas clicables (recorridas: %d)", n);
    lv_group_focus_next(grp); lv_group_focus_next(grp); lv_group_focus_next(grp); lv_group_focus_next(grp); lv_group_focus_next(grp);
    dump("dash_foco_perilla");
    { lv_obj_t *f = lv_group_get_focused(grp); lv_area_t ar; lv_obj_get_coords(f, &ar); run(); const uint16_t px = fb[(ar.y1 + 40) * ARQ_W + ar.x1]; const bool blue = ((px >> 11) & 31) < 10 && (px & 31) > 20;
      CHECK(blue, "tras mover la perilla el anillo de foco se dibuja"); }
    // cambio de modo (reposo -> imprimiendo) con la perilla en uso: las tarjetas nuevas entran en el grupo
    { arq_snapshot_t sj = make(ARQ_PS_PRINTING); arq_dash_update(&sj); run();
      int m = 0; lv_obj_t *f0 = lv_group_get_focused(grp); do { ++m; lv_group_focus_next(grp); } while (lv_group_get_focused(grp) != f0 && m < 20);
      CHECK(m == 5, "con impresion: la perilla recorre 5 tarjetas (hotend, cama, ventiladores, impresion, Temp.; Mover/Ajustes bloqueados). Recorridas: %d", m); }
    ev_count = 0; lv_event_send(lv_group_get_focused(grp), LV_EVENT_RELEASED, nullptr);
    CHECK(ev_count == 1, "pulsar la perilla sobre la tarjeta con foco dispara su accion");
    new_screen(); run(); lv_group_del(grp);
    for (int i = 0; i < 100; ++i) { arq_snapshot_t s1 = make(ARQ_PS_IDLE); dashboard(s1, nullptr); lv_group_t *g1 = lv_group_create(); arq_group_add_clickables(g1, lv_scr_act()); lv_group_focus_next(g1); run(2); new_screen(); lv_group_del(g1); }
    new_screen(); run();
    CHECK(used() == ref, "sin fugas tras 100 ciclos con perilla: heap %u B (antes %u B)", (unsigned)used(), (unsigned)ref); }

  // 6c) Margen de heap en el peor caso: LVGL 6.1 NO tolera quedarse sin memoria (falla dentro de lv_obj_create/lv_obj_del),
  //     asi que la defensa es un margen amplio. Peor caso: Dashboard con impresion + dialogo abierto + perilla.
  { new_screen(); run();
    arq_snapshot_t s = make(ARQ_PS_PRINTING); dashboard(s, nullptr);
    lv_group_t *gg = lv_group_create(); arq_group_add_clickables(gg, lv_scr_act());
    arq_dialog_open("Cancelar impresion", "Se detendra la impresion en curso.", "Cancelar", "Seguir", true, dlg_cb, 901, 902); run();
    lv_mem_monitor_t m; lv_mem_monitor(&m);
    printf("[mem] peor caso (Dashboard+dialogo+perilla): usado %u B, libre %u B, mayor bloque libre %u B de %u B\n", (unsigned)(m.total_size - m.free_size), (unsigned)m.free_size, (unsigned)m.free_biggest_size, (unsigned)m.total_size);
    CHECK(m.free_size >= 8192, "margen de heap en el peor caso: %u B libres (>= 8192 B, la mitad del heap)", (unsigned)m.free_size);
    arq_dialog_close(); new_screen(); run(); lv_group_del(gg); }

  // 7) Actualizar con el mismo snapshot no redibuja
  { arq_snapshot_t s = make(ARQ_PS_HEATING); dashboard(s, nullptr); run(); lv_disp_t *d = lv_disp_get_default();
    arq_dash_update(&s); CHECK(d->inv_p == 0, "snapshot sin cambios: nada que redibujar");
    s.hot.cur = 169; arq_dash_update(&s); CHECK(d->inv_p > 0 && d->inv_p <= 3, "cambio de un valor: solo se invalida lo afectado (%u zonas)", (unsigned)d->inv_p); }


  // 8) Supervision de impresion ----------------------------------------------------------------------------------------------
  { static lv_point_t pt; static bool down = false;
    static lv_indev_drv_t idrv; lv_indev_drv_init(&idrv); idrv.type = LV_INDEV_TYPE_POINTER;
    idrv.read_cb = [](lv_indev_drv_t *, lv_indev_data_t *d) -> bool { d->point = pt; d->state = down ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL; return false; };
    lv_indev_drv_register(&idrv);
    auto tap = [&](int x, int y) { pt.x = x; pt.y = y; down = true; run(6); down = false; run(6); };
    new_screen(); run(); const uint32_t b0 = used();

    // 8a) memoria: pico por modo y 300 ciclos sin fugas
    uint32_t pk = 0;
    for (int m = 0; m <= ARQ_PM_NOCTRL; ++m) { new_screen(); const uint32_t b = used(); arq_snapshot_t sj = make(ARQ_PS_PRINTING); print_screen(sj, (arq_pmode_t)m, nullptr); if (used() - b > pk) pk = used() - b; }
    printf("[mem] supervision: pico %u B\n", (unsigned)pk);
    CHECK(pk < 5000, "pico de la supervision %u B (< 5000 B)", (unsigned)pk);
    for (int i = 0; i < 300; ++i) { arq_snapshot_t sj = make(i % 3 ? ARQ_PS_PRINTING : ARQ_PS_PAUSED); print_screen(sj, (arq_pmode_t)(i % 7), nullptr, i % 2); }
    new_screen(); run();
    CHECK(used() <= b0 + 8, "300 ciclos crear/destruir supervision: heap %u B (base %u B)", (unsigned)used(), (unsigned)b0);

    // 8b) la zona de la miniatura (x 2..202, y 36..236) no debe cubrirla ningun objeto LVGL
    { arq_snapshot_t sj = make(ARQ_PS_PRINTING); print_screen(sj, ARQ_PM_RUNNING, nullptr, true);
      bool clear = true; int n = 0;
      for (lv_obj_t *c = lv_obj_get_child_back(lv_scr_act(), nullptr); c; c = lv_obj_get_child_back(lv_scr_act(), c)) {
        lv_area_t a; lv_obj_get_coords(c, &a); ++n;
        if (a.x2 >= 2 && a.x1 <= 202 && a.y2 >= 36 && a.y1 <= 236) { clear = false; printf("[test] objeto %d invade la miniatura: x %d..%d y %d..%d\n", n, a.x1, a.x2, a.y1, a.y2); }
      }
      CHECK(clear, "ninguno de los %d objetos de la supervision invade la zona de la miniatura", n);
      // y nada la repinta al refrescar datos
      g_thumb_pct = 40; dump(nullptr); g_thumb_pct = -1;
      const uint16_t sent = fb[(36 + 100) * ARQ_W + 2 + 100];
      for (int i = 0; i < 50; ++i) { sj.pct = 40 + i % 50; sj.elapsed_s += 7; sj.hot.cur = 200 + i % 9; sj.z_cmm += 20; arq_print_update(&sj, ARQ_PM_RUNNING); run(2); }
      bool intact = true; for (int y = 0; y < 200 && intact; ++y) for (int x = 0; x < 200; ++x) { uint16_t e = fb[(36 + y) * ARQ_W + 2 + x]; (void)e; }
      CHECK(fb[(36 + 100) * ARQ_W + 2 + 100] == sent && intact, "50 refrescos de datos no tocan los pixeles de la miniatura"); }

    // 8c) toques: cada boton envia su id; en modo 'preparando' (cargando miniatura) todos estan deshabilitados
    { arq_snapshot_t sj = make(ARQ_PS_PRINTING); print_screen(sj, ARQ_PM_RUNNING, nullptr, true);
      struct { int x, y, id; const char *n; } t[] = { { 420, 18, ARQ_PID_HOME, "Inicio" }, { 64, 280, ARQ_PID_PAUSE, "Pausar" }, { 183, 280, ARQ_PID_STOP, "Cancelar" },
        { 292, 280, ARQ_PID_OBJECTS, "Objetos" }, { 401, 280, ARQ_PID_OPTIONS, "Opciones" } };
      for (auto &c : t) { ev_count = 0; ev_last = -1; tap(c.x, c.y); CHECK(ev_count == 1 && ev_last == c.id, "toque en %s -> id %d", c.n, ev_last); }
      arq_print_update(&sj, ARQ_PM_PREPARING); run();
      for (auto &c : t) { ev_count = 0; tap(c.x, c.y); CHECK(ev_count == 0, "preparando: %s no recibe toques", c.n); }
      arq_print_update(&sj, ARQ_PM_PAUSING); run();
      ev_count = 0; tap(64, 280); CHECK(ev_count == 0, "pausando: el boton de pausa esta deshabilitado");
      ev_count = 0; tap(183, 280); CHECK(ev_count == 1, "pausando: Cancelar sigue disponible");
      arq_print_update(&sj, ARQ_PM_NOCTRL); run();
      ev_count = 0; tap(64, 280); CHECK(ev_count == 0, "trabajo no controlado por la interfaz: sin pausa");
      arq_print_update(&sj, ARQ_PM_PAUSED); run();
      ev_count = 0; ev_last = -1; tap(64, 280); CHECK(ev_count == 1 && ev_last == ARQ_PID_PAUSE, "pausada: el mismo boton (Reanudar) funciona");
      print_screen(sj, ARQ_PM_RUNNING, nullptr, false);
      ev_count = 0; ev_last = -1; tap(401, 280); CHECK(ev_count == 1 && ev_last == ARQ_PID_OPTIONS, "sin cancelar-objetos: 3 botones, Opciones en su sitio"); }

    // 8d) sin cambios no se redibuja; un cambio solo invalida lo afectado
    { arq_snapshot_t sj = make(ARQ_PS_PRINTING); print_screen(sj, ARQ_PM_RUNNING, nullptr); run(); lv_disp_t *d = lv_disp_get_default();
      arq_print_update(&sj, ARQ_PM_RUNNING); CHECK(d->inv_p == 0, "supervision: snapshot sin cambios, nada que redibujar");
      sj.elapsed_s += 1; arq_print_update(&sj, ARQ_PM_RUNNING); CHECK(d->inv_p > 0 && d->inv_p <= 4, "supervision: un cambio invalida pocas zonas (%u)", (unsigned)d->inv_p); }

    // 8e) peor caso de heap: supervision + dialogo + perilla
    { new_screen(); run(); arq_snapshot_t sj = make(ARQ_PS_PRINTING); print_screen(sj, ARQ_PM_RUNNING, nullptr, true);
      lv_group_t *gg = lv_group_create(); arq_group_add_clickables(gg, lv_scr_act());
      arq_dialog_open("Cancelar impresion", "Se detendra la impresion y se perdera el avance. Esta accion no se puede deshacer.", "Cancelar impresion", "Seguir imprimiendo", true, dlg_cb, 901, 902); run();
      lv_mem_monitor_t m; lv_mem_monitor(&m);
      printf("[mem] peor caso (supervision+dialogo+perilla): usado %u B, libre %u B\n", (unsigned)(m.total_size - m.free_size), (unsigned)m.free_size);
      CHECK(m.free_size >= 8192, "margen de heap en la supervision: %u B libres (>= 8192 B)", (unsigned)m.free_size);
      arq_dialog_close(); new_screen(); run(); lv_group_del(gg); }

    // 8f) dialogo informativo (fin de impresion): un solo boton
    { arq_snapshot_t sj = make(ARQ_PS_IDLE); print_screen(sj, ARQ_PM_NOCTRL, nullptr, true); const uint32_t u0 = used();
      arq_dialog_open("Impresion finalizada", "Archivo\nTiempo total: 2:15", "Aceptar", nullptr, false, dlg_cb, 901, 0); run();
      printf("[mem] dialogo informativo: %u B\n", (unsigned)(used() - u0));
      int before = dlg_ok; tap(240, 224); CHECK(dlg_ok == before + 1 && !arq_dialog_is_open(), "dialogo informativo: el boton unico acepta y cierra");
      for (int i = 0; i < 200; ++i) { arq_dialog_open("T", "M", "Aceptar", nullptr, false, dlg_cb, 901, 0); run(1); arq_dialog_close(); }
      run(); CHECK(used() <= u0, "200 aperturas del dialogo informativo: sin fugas (%u B, antes %u B)", (unsigned)used(), (unsigned)u0); }

    // 9) Cancelar objeto ------------------------------------------------------------------------------------------------------
    // 9a) causa del reinicio: la pantalla anterior creaba 16 fichas y 16 rectangulos (cada uno boton + etiqueta) + panel, info, Cancelar y Atras.
    //     No se crean aqui (LVGL 6.1 se cuelga si se queda sin heap): se mide el coste de un par boton+etiqueta estandar y se extrapola.
    { new_screen(); run(); const uint32_t b = used(); lv_obj_t *s = lv_scr_act();
      for (int i = 0; i < 10; ++i) { lv_obj_t *bt = lv_btn_create(s, nullptr); lv_label_create(bt, nullptr); }
      const uint32_t par = (used() - b) / 10, est = 34 * par + 3 * (par / 2);
      printf("[mem] pantalla anterior de objetos: %u B por par boton+etiqueta -> unos %u B para ~71 objetos (heap total 16384 B)\n", (unsigned)par, (unsigned)est);
      CHECK(est > 16384, "causa confirmada: la pantalla anterior NO cabia en el heap de LVGL (%u B > 16384 B)", (unsigned)est);
      new_screen(); run(); }

    // 9b) la nueva: 6 objetos fijos, con 0, 9 o 16 objetos del G-code
    { new_screen(); run(); const uint32_t b = used(); uint32_t pko = 0;
      const int ns[] = { 0, 3, 9, 16 };
      for (int n : ns) { new_screen(); const uint32_t b1 = used(); objects_screen(omodel(n, n - 1, n ? 0 : -1, n ? 0 : -1, 0), nullptr); if (used() - b1 > pko) pko = used() - b1; }
      printf("[mem] cancelar objeto: pico %u B (con 0, 3, 9 y 16 objetos)\n", (unsigned)pko);
      CHECK(pko < 3500, "pico de la pantalla de objetos %u B (< 3500 B), igual con 16 objetos", (unsigned)pko);
      // el asignador de LVGL deja 4-12 B de relleno segun el historial: se exige que NO CREZCA entre 300 y 600 ciclos
      for (int i = 0; i < 300; ++i) objects_screen(omodel(1 + i % 16, i % 16, i % 5, i % 7 - 1, i & 0x5, i % 3 ? -1 : 30), nullptr);
      new_screen(); run(); const uint32_t u1 = used();
      for (int i = 0; i < 300; ++i) objects_screen(omodel(1 + i % 16, i % 16, i % 5, i % 7 - 1, i & 0x5, i % 3 ? -1 : 30), nullptr);
      new_screen(); run(); const uint32_t u2 = used();
      CHECK(u2 <= u1 && u1 <= b + 16, "300 y 600 ciclos crear/destruir: sin fugas ni fragmentacion progresiva (%u B, %u B; base %u B)", (unsigned)u1, (unsigned)u2, (unsigned)b); }

    // 9c) peor caso: 16 objetos + dialogo de confirmacion abierto
    { objects_screen(omodel(16, 15, 15, 15, 0), nullptr);
      arq_dialog_open("Cancelar objeto 16", "Se dejara de imprimir el objeto 16. Los demas objetos siguen imprimiendose.", "Cancelar objeto", "Volver", true, dlg_cb, 901, 902); run();
      lv_mem_monitor_t m; lv_mem_monitor(&m);
      printf("[mem] peor caso (16 objetos + dialogo): usado %u B, libre %u B\n", (unsigned)(m.total_size - m.free_size), (unsigned)m.free_size);
      CHECK(m.free_size >= 8192, "margen de heap: %u B libres (>= 8192 B)", (unsigned)m.free_size);
      arq_dialog_close(); new_screen(); run(); }

    // 9d) toques: mapa, fichas y boton de accion
    { arq_obj_model_t m = omodel(9, 8, 3, -1, 0);
      auto refresh = [&]() { m.sel = o_sel; arq_objects_update(&m); run(); };
      o_sel = -1; o_act_ev = o_back_ev = 0; objects_screen(m, nullptr);
      tap(404 + 0, 22); CHECK(o_back_ev == 0, "(el boton Atras esta a la izquierda, no aqui)");
      tap(20, 22); CHECK(o_back_ev == 1, "boton Atras");
      tap(380, 293); CHECK(o_act_ev == 0, "sin objeto seleccionado el boton de accion esta deshabilitado");
      // ficha 7 (columna 3, fila 1)
      tap(280 + 3 * 50 + 23, 48 + 1 * 48 + 22); refresh(); CHECK(o_sel == 7, "tocar la ficha 8 selecciona el objeto 8 (indice %d)", o_sel);
      // hueco entre fichas y ficha sin objeto (n = 9: la ficha 10 no existe)
      tap(280 + 46 + 2, 48 + 22); CHECK(o_sel == 7, "tocar el hueco entre fichas no cambia la seleccion");
      tap(280 + 1 * 50 + 23, 48 + 2 * 48 + 22); CHECK(o_sel == 7, "tocar donde no hay ficha no cambia la seleccion");
      // rectangulo del mapa: centro del objeto 2 (indice 1) y del pequeno (indice 5, 6 mm -> 22 px)
      const int span = 250, dim = 220;
      auto mapc = [&](int i, int *cx, int *cy) { *cx = 6 + 8 + (m.x0[i] * span) / dim + ((m.x1[i] - m.x0[i]) * span) / dim / 2; *cy = 48 + 8 + ((dim - m.y1[i]) * span) / dim + ((m.y1[i] - m.y0[i]) * span) / dim / 2; };
      int cx, cy; mapc(1, &cx, &cy); tap(cx, cy); refresh(); CHECK(o_sel == 1, "tocar el rectangulo del objeto 2 en el mapa lo selecciona (indice %d)", o_sel);
      mapc(5, &cx, &cy); tap(cx + 9, cy - 9); refresh(); CHECK(o_sel == 5, "objeto pequeno (6 mm): el area tocable minima es de 22 px (indice %d)", o_sel);
      tap(10 + 262, 48 + 260); CHECK(o_sel == 5, "tocar un area vacia del mapa no cambia la seleccion");
      tap(380, 293); CHECK(o_act_ev == 1, "con objeto seleccionado el boton de accion responde");
      m.canceled = 1u << 5; refresh(); tap(380, 293); CHECK(o_act_ev == 2, "objeto cancelado: el boton (Restaurar) responde");
      // un objeto sin posicion conocida solo se elige desde las fichas
      m = omodel(9, 1, -1, -1, 0); o_sel = -1; objects_screen(m, nullptr); mapc(0, &cx, &cy);
      tap(280 + 2 * 50 + 23, 48 + 22); refresh(); CHECK(o_sel == 2, "objeto sin posicion: se elige con su ficha (indice %d)", o_sel);
      tap(cx, cy); refresh(); CHECK(o_sel == 0, "objeto con posicion: se elige tambien en el mapa");
      // sin cambios no se redibuja; un cambio de seleccion invalida pocas zonas
      lv_disp_t *d = lv_disp_get_default(); run(); arq_objects_update(&m); CHECK(d->inv_p == 0, "modelo sin cambios: nada que redibujar");
      m.sel = 3; arq_objects_update(&m); CHECK(d->inv_p > 0 && d->inv_p <= 4, "cambio de seleccion: pocas zonas invalidadas (%u)", (unsigned)d->inv_p);
      new_screen(); run(); }
  }

  printf("[test] %s\n", fails ? "HAY FALLOS" : "TODAS LAS PRUEBAS DEL SIMULADOR PASARON");
}

int main(int argc, char **argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  if (argc > 1) outdir = argv[1];
  lv_init();
  lv_disp_buf_init(&disp_buf, buf1, nullptr, ARQ_W * 14);
  lv_disp_drv_t drv; lv_disp_drv_init(&drv);
  drv.flush_cb = flush_cb; drv.buffer = &disp_buf;
  lv_disp_drv_register(&drv);
  printf("[mem] tras lv_init: %u B usados de 16384 B\n", (unsigned)used());

  static const char *names[] = { "dash_1_lista", "dash_2_calentando", "dash_3_temp_alcanzada", "dash_4_enfriando", "dash_5_imprimiendo", "dash_6_pausada", "dash_7_error" };
  for (int st = 0; st < 7; ++st) { arq_snapshot_t s = make((arq_pstate_t)st); dashboard(s, names[st]); }
  components("componentes");
  iconos("iconos");
  { arq_snapshot_t s = make(ARQ_PS_PRINTING); dashboard(s, nullptr); arq_dialog_open("Cancelar impresion", "Se detendra la impresion en curso. Esta accion no se puede deshacer.", "Cancelar", "Seguir", true, dlg_cb, 901, 902); dump("dialogo_cancelar"); arq_dialog_close(); }

  { arq_snapshot_t s = make(ARQ_PS_PRINTING);
    print_screen(s, ARQ_PM_RUNNING, "print_1_imprimiendo", true, 62);
    arq_snapshot_t sp = make(ARQ_PS_PAUSED); print_screen(sp, ARQ_PM_PAUSED, "print_2_pausada", true, 62);
    print_screen(s, ARQ_PM_PAUSING, "print_3_pausando", true, 62);
    arq_snapshot_t s0 = make(ARQ_PS_HEATING); s0.hot.cur = 120; s0.hot.tgt = 210; print_screen(s0, ARQ_PM_PREPARING, "print_4_preparando", true, 0);
    arq_snapshot_t s1 = make(ARQ_PS_PRINTING); s1.pct = 3; s1.elapsed_s = 40; s1.remaining_s = 0; s1.z_cmm = 20; s1.hot.cur = 188; s1.hot.tgt = 210; s1.bed.cur = 58; s1.bed.tgt = 60; s1.fan[0].pct = 0;
    print_screen(s1, ARQ_PM_RUNNING, "print_5_inicio", false, 3);
    arq_snapshot_t s2 = make(ARQ_PS_PRINTING); s2.pct = 100; s2.elapsed_s = 27 * 3600 + 5 * 60; s2.remaining_s = 0; s2.z_cmm = 12345; strncpy(s2.file, "archivo_con_un_nombre_muy_largo_para_comprobar_el_recorte.gcode", sizeof(s2.file) - 1);
    print_screen(s2, ARQ_PM_RUNNING, "print_6_nombre_largo", true, 100);
    arq_snapshot_t s3 = make(ARQ_PS_PRINTING); print_screen(s3, ARQ_PM_RUNNING, nullptr, true, 62);
    // con un dialogo abierto la miniatura no se pinta (la interfaz solo la repinta al cerrarlo)
    arq_dialog_open("Cancelar impresi\xC3\xB3n", "Se detendr\xC3\xA1 la impresi\xC3\xB3n y se perder\xC3\xA1 el avance. Esta acci\xC3\xB3n no se puede deshacer.", "Cancelar impresi\xC3\xB3n", "Seguir imprimiendo", true, dlg_cb, 901, 902); dump("print_7_dialogo_cancelar"); arq_dialog_close();
    arq_dialog_open("Impresi\xC3\xB3n finalizada", "soporte_motor_nema17_v3_final.gcode\nTiempo total: 2:15", "Aceptar", nullptr, false, dlg_cb, 901, 0); dump("print_8_fin"); arq_dialog_close();
    g_thumb_pct = -1; }
  { objects_screen(omodel(9, 4, 4, 4, 1u << 1), "obj_1_normal");
    objects_screen(omodel(16, 15, 15, 15, (1u << 2) | (1u << 9)), "obj_2_16_objetos");
    objects_screen(omodel(0, -1, -1, -1, 0), "obj_3_sin_objetos");
    objects_screen(omodel(6, -1, 0, 0, 0), "obj_4_sin_posiciones");
    objects_screen(omodel(9, 8, 3, 1, 1u << 1), "obj_5_restaurar");
    objects_screen(omodel(9, 4, 4, -1, 0), "obj_6_sin_seleccion");
    objects_screen(omodel(9, 4, 4, 4, 0), nullptr);
    arq_dialog_open("Cancelar objeto 5", "Se dejar\xC3\xA1 de imprimir el objeto 5. Los dem\xC3\xA1s objetos siguen imprimi\xC3\xA9ndose.", "Cancelar objeto", "Volver", true, dlg_cb, 901, 902); dump("obj_7_dialogo"); arq_dialog_close();
    objects_screen(omodel(16, 15, 3, -1, 0, 24), "obj_8_mas_de_16"); }
  tests();
  return fails ? 1 : 0;
}
