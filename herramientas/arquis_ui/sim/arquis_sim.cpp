// Simulador PC de ARQUIS UI: compila LVGL 6.1.1-MKS real + la vista del Dashboard y los componentes, los ejecuta (en ARM de 32 bits
// con qemu-user) y vuelca capturas PPM. Usa el mismo bufer parcial que el firmware (480 x 14 px) y el heap de 16 KB.
// OJO: aqui los datos del Dashboard son "snapshots" de PRUEBA para ejercitar la VISTA; la lectura real de Marlin
// (arquis_data.cpp) no corre en el simulador.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arquis_dashboard_view.h"

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
static void dump(const char *name) {
  run();
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
  CHECK(used() == base, "300 ciclos crear/destruir Dashboard: heap final %u B (base %u B)", (unsigned)used(), (unsigned)base);
  CHECK(biggest() >= base_big, "sin fragmentacion: mayor bloque libre %u B (>= base %u B)", (unsigned)biggest(), (unsigned)base_big);

  // 2) Pico de memoria del Dashboard por estado
  uint32_t peak = 0;
  for (int st = 0; st < 7; ++st) { new_screen(); const uint32_t b = used(); arq_snapshot_t s = make((arq_pstate_t)st); dashboard(s, nullptr); const uint32_t u = used() - b; if (u > peak) peak = u; printf("[mem] Dashboard estado %d: %u B\n", st, (unsigned)u); }
  CHECK(peak < 6000, "pico del Dashboard %u B (< 6000 B)", (unsigned)peak);

  // 3) Navegacion Dashboard <-> otra pantalla, con cambio de modo (reposo <-> imprimiendo)
  for (int i = 0; i < 200; ++i) { arq_snapshot_t s = make(i % 2 ? ARQ_PS_PRINTING : ARQ_PS_IDLE); dashboard(s, nullptr); screen_stub(12); }
  new_screen(); run();
  CHECK(used() == base, "200 idas y vueltas Dashboard<->pantalla de 12 botones: heap %u B (base %u B)", (unsigned)used(), (unsigned)base);

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
  tests();
  return fails ? 1 : 0;
}
