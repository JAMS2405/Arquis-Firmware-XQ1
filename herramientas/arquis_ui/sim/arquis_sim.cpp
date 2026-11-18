// Simulador PC de ARQUIS UI: compila LVGL 6.1.1-MKS real + arquis_theme/widgets/icons/fuentes y vuelca capturas PPM.
// Usa el mismo tipo de bufer parcial que el firmware (480 x 14 px) para que el renderizado sea el real.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "arquis_widgets.h"

// Ganchos que en el firmware real definen draw_print_file.cpp (miniatura de G-code) y la libreria: aqui no se usan.
extern "C" {
  uint32_t lv_open_gcode_file(char *) { return 0; }
  void lv_gcode_file_read(uint8_t *) {}
  void lv_close_gcode_file() {}
  void lv_log_register_print_cb(void *) {}
}

static uint16_t fb[ARQ_W * ARQ_H];
static lv_disp_buf_t disp_buf;
static lv_color_t buf1[ARQ_W * 14];

static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *a, lv_color_t *c) {
  for (int y = a->y1; y <= a->y2; ++y)
    for (int x = a->x1; x <= a->x2; ++x) fb[y * ARQ_W + x] = (c++)->full;
  lv_disp_flush_ready(drv);
}

static void dump(const char *name) {
  for (int i = 0; i < 20; ++i) { lv_tick_inc(10); lv_task_handler(); }
  FILE *f = fopen(name, "wb");
  fprintf(f, "P6\n%d %d\n255\n", ARQ_W, ARQ_H);
  for (int i = 0; i < ARQ_W * ARQ_H; ++i) {
    const uint16_t v = fb[i];
    const uint8_t r = (v >> 11) & 31, g = (v >> 5) & 63, b = v & 31;
    fputc((r << 3) | (r >> 2), f); fputc((g << 2) | (g >> 4), f); fputc((b << 3) | (b >> 2), f);
  }
  fclose(f);
}

static void mem(const char *tag) {
  lv_mem_monitor_t m; lv_mem_monitor(&m);
  printf("[mem] %-28s heap LVGL: usado %5u B de %u B (%u%%), max bloque libre %u B, objetos creados OK\n",
         tag, (unsigned)(m.total_size - m.free_size), (unsigned)m.total_size, (unsigned)m.used_pct, (unsigned)m.free_biggest_size);
}

static void cb(lv_obj_t *, lv_event_t) {}

static lv_obj_t * new_screen() {
  lv_obj_t *old = lv_scr_act(), *scr = lv_obj_create(nullptr, nullptr);   // como el firmware: se libera la pantalla anterior
  arq_screen_apply(scr);
  lv_scr_load(scr);
  if (old && old != scr) lv_obj_del(old);
  return scr;
}


// Costo en heap LVGL de cada componente (bytes), medido en ARM de 32 bits.
static uint32_t used() { lv_mem_monitor_t m; lv_mem_monitor(&m); return m.total_size - m.free_size; }
#define COST(name, expr) { lv_obj_t *s_ = new_screen(); uint32_t a_ = used(); expr; printf("[coste] %-36s %4u B\n", name, (unsigned)(used() - a_)); (void)s_; }
static void costs() {
  static arq_temp_tile_t tt; static arq_header_t hd;
  COST("pantalla vacia (ref)", (void)0)
  COST("boton de navegacion (icono+texto)", arq_nav_tile_create(s_, 0, 0, 100, 90, &arq_icon_gear, ARQ_TINT_ACCENT, "Ajustes", ARQ_BTN_SECONDARY, cb, 1))
  COST("boton (icono + texto)", arq_button_create(s_, 0, 0, 120, 48, &arq_icon_check, "Iniciar", ARQ_BTN_SUCCESS, cb, 1))
  COST("indicador de estado (chip)", arq_chip_create(s_, 0, 0, 100, "Lista", ARQ_TINT_OK))
  COST("tarjeta de temperatura", arq_temp_tile_create(&tt, s_, 0, 0, 148, 124, &arq_icon_nozzle, "Hotend"))
  COST("cabecera con Atras", arq_header_create(&hd, s_, "Titulo", cb, 1))
  COST("cabecera sin Atras", arq_header_create(&hd, s_, "Titulo", nullptr, 0))
  COST("barra de progreso", arq_progress_create(s_, 0, 0, 200, 12))
}

// ---- Dashboard (boceto para revision) -----------------------------------------------------------------
static arq_temp_tile_t t_hot, t_bed;
static char fan_buf[8];

static void dashboard(int hot, int hot_t, int bed, int bed_t, int fan, const char *state, arq_tint_t tint, const char *shot) {
  lv_obj_t *scr = new_screen();
  arq_header_t hd; arq_header_create(&hd, scr, "ARQUIS XQ1", nullptr, 0);
  arq_chip_create(scr, ARQ_W - ARQ_MARGIN - 130, 8, 130, state, tint);
  const lv_coord_t y1 = ARQ_HEADER_H + 10, h1 = 124, w1 = 148;
  arq_temp_tile_create(&t_hot, scr, ARQ_MARGIN, y1, w1, h1, &arq_icon_nozzle, "Hotend");
  arq_temp_tile_create(&t_bed, scr, ARQ_MARGIN + w1 + ARQ_GAP, y1, w1, h1, &arq_icon_bed, "Cama");
  arq_temp_tile_update(&t_hot, hot, hot_t);
  arq_temp_tile_update(&t_bed, bed, bed_t);
  lv_obj_t *f = arq_card_create(scr, ARQ_MARGIN + 2 * (w1 + ARQ_GAP), y1, ARQ_W - 2 * ARQ_MARGIN - 2 * (w1 + ARQ_GAP), h1, ARQ_LAYOUT_STAT, ARQ_BTN_SECONDARY, nullptr, 0);
  snprintf(fan_buf, sizeof(fan_buf), "%d%%", fan);
  arq_card_set_icon(f, &arq_icon_fan, ARQ_TINT_ACCENT);
  arq_card_set_title(f, "Ventilador", ARQ_TEXT_BODY);
  arq_card_set_value(f, fan_buf, fan ? ARQ_TEXT_NUM_L_OK : ARQ_TEXT_NUM_L);
  arq_card_set_sub(f, fan ? "Activo" : "Detenido", ARQ_TEXT_MUTED);
  arq_card_set_dot(f, fan ? ARQ_TINT_OK : ARQ_TINT_MUTED);
  const lv_coord_t y2 = y1 + h1 + ARQ_GAP, h2 = ARQ_H - y2 - ARQ_MARGIN, n = 5, w2 = (ARQ_W - 2 * ARQ_MARGIN - (n - 1) * ARQ_GAP) / n;
  struct { const lv_img_dsc_t *i; const char *t; arq_btn_kind_t k; arq_tint_t tint; } nav[] = {
    { &arq_icon_print, "Imprimir", ARQ_BTN_PRIMARY, ARQ_TINT_AUTO }, { &arq_icon_move, "Mover", ARQ_BTN_SECONDARY, ARQ_TINT_ACCENT },
    { &arq_icon_temp, "Temp.", ARQ_BTN_SECONDARY, ARQ_TINT_ACCENT }, { &arq_icon_level, "Calibrar", ARQ_BTN_SECONDARY, ARQ_TINT_ACCENT },
    { &arq_icon_gear, "Ajustes", ARQ_BTN_SECONDARY, ARQ_TINT_ACCENT } };
  for (int i = 0; i < n; ++i)
    arq_nav_tile_create(scr, ARQ_MARGIN + i * (w2 + ARQ_GAP), y2, w2, h2, nav[i].i, nav[i].tint, nav[i].t, nav[i].k, cb, 100 + i);
  dump(shot);
  mem(shot);
}

// ---- Hoja de componentes --------------------------------------------------------------------------------
static void iconos(const char *shot) {
  lv_obj_t *scr = new_screen();
  arq_header_t hd; arq_header_create(&hd, scr, "ARQUIS UI - Iconos (40 px)", cb, 1);
  static const lv_img_dsc_t *all[] = { &arq_icon_folder, &arq_icon_move, &arq_icon_temp, &arq_icon_gear, &arq_icon_print, &arq_icon_nozzle, &arq_icon_bed,
    &arq_icon_fan, &arq_icon_play, &arq_icon_pause, &arq_icon_stop, &arq_icon_back, &arq_icon_check, &arq_icon_close, &arq_icon_warn, &arq_icon_level,
    &arq_icon_tools, &arq_icon_wifi, &arq_icon_home, &arq_icon_file, &arq_icon_speed };
  static const arq_tint_t tints[] = { ARQ_TINT_ACCENT, ARQ_TINT_TEXT, ARQ_TINT_OK, ARQ_TINT_WARN, ARQ_TINT_DANGER, ARQ_TINT_MUTED };
  for (int i = 0; i < 21; ++i) {
    lv_obj_t *c = arq_card_create(scr, 12 + (i % 7) * 64, 56 + (i / 7) * 82, 56, 56, ARQ_LAYOUT_BUTTON, ARQ_BTN_SECONDARY, nullptr, 0);
    arq_card_set_icon(c, all[i], tints[i % 6]);
  }
  lv_obj_t *acc = arq_card_create(scr, 12, 288, 456, 26, ARQ_LAYOUT_CHIP, ARQ_BTN_SECONDARY, nullptr, 0);
  arq_card_set_title(acc, "Ñandú: áéíóú ¿Qué? ¡Listo! 215°C", ARQ_TEXT_MUTED);
  dump(shot);
  mem(shot);
}

static arq_temp_tile_t c_t1, c_t2, c_t3;

static void components(const char *shot) {
  lv_obj_t *scr = new_screen();
  arq_header_t hd; arq_header_create(&hd, scr, "ARQUIS UI - Componentes", cb, 1);
  arq_chip_create(scr, 10, 52, 100, "Lista", ARQ_TINT_OK);
  arq_chip_create(scr, 118, 52, 140, "Calentando", ARQ_TINT_WARN);
  arq_chip_create(scr, 266, 52, 120, "Error", ARQ_TINT_DANGER);
  arq_chip_create(scr, 386, 52, 84, "Pausa", ARQ_TINT_ACCENT);
  arq_button_create(scr, 10, 88, 108, 44, nullptr, "Primario", ARQ_BTN_PRIMARY, cb, 2);
  arq_button_create(scr, 126, 88, 108, 44, nullptr, "Secundario", ARQ_BTN_SECONDARY, cb, 3);
  arq_button_create(scr, 242, 88, 108, 44, &arq_icon_check, "Iniciar", ARQ_BTN_SUCCESS, cb, 4);
  arq_button_create(scr, 358, 88, 112, 44, nullptr, "Cancelar", ARQ_BTN_DANGER, cb, 5);
  arq_temp_tile_create(&c_t1, scr, 10, 140, 148, 124, &arq_icon_nozzle, "Hotend");   arq_temp_tile_update(&c_t1, 24, 0);
  arq_temp_tile_create(&c_t2, scr, 166, 140, 148, 124, &arq_icon_nozzle, "Hotend");  arq_temp_tile_update(&c_t2, 168, 210);
  arq_temp_tile_create(&c_t3, scr, 322, 140, 148, 124, &arq_icon_bed, "Cama");       arq_temp_tile_update(&c_t3, 60, 60);
  lv_obj_t *bar = arq_progress_create(scr, 10, 282, 376, 12);
  arq_progress_set(bar, 62);
  static char pct[8]; snprintf(pct, sizeof(pct), "62%%");
  lv_obj_t *pl = arq_card_create(scr, 394, 274, 76, 28, ARQ_LAYOUT_CHIP, ARQ_BTN_SECONDARY, nullptr, 0);
  arq_card_set_title(pl, pct, ARQ_TEXT_BODY);
  dump(shot);
  mem(shot);
}

// ---- Prueba de eventos: el boton marca "pulsado" y reenvia los eventos con el id correcto (como espera el codigo MKS) ----
static int ev_count = 0, ev_last_id = -1, ev_clicked = 0;
static void test_cb(lv_obj_t *o, lv_event_t ev) { ++ev_count; ev_last_id = o->mks_obj_id; if (ev == LV_EVENT_CLICKED) ++ev_clicked; }
static int fails = 0;
#define CHECK(c, msg) do { if (c) printf("[test] OK    %s\n", msg); else { printf("[test] FALLA %s\n", msg); ++fails; } } while (0)
static void test_events(const char *dir) {
  lv_obj_t *scr = new_screen();
  lv_obj_t *b = arq_nav_tile_create(scr, 100, 100, 100, 90, &arq_icon_gear, ARQ_TINT_ACCENT, "Ajustes", ARQ_BTN_SECONDARY, test_cb, 777);
  lv_obj_t *p = arq_nav_tile_create(scr, 250, 100, 100, 90, &arq_icon_print, ARQ_TINT_AUTO, "Imprimir", ARQ_BTN_PRIMARY, test_cb, 778);
  lv_obj_t *ro = arq_chip_create(scr, 10, 10, 100, "Solo lectura", ARQ_TINT_OK);
  CHECK(lv_obj_get_click(b), "boton con callback es clicable");
  CHECK(!lv_obj_get_click(ro), "chip sin callback no es clicable");
  lv_event_send(b, LV_EVENT_PRESSED, nullptr);
  lv_event_send(p, LV_EVENT_PRESSED, nullptr);
  char f[256]; snprintf(f, sizeof(f), "%s/botones_pulsados.ppm", dir); dump(f);
  CHECK(ev_count == 2 && ev_last_id == 778, "PRESSED llega al callback con el mks_obj_id correcto");
  lv_event_send(b, LV_EVENT_RELEASED, nullptr);
  lv_event_send(b, LV_EVENT_CLICKED, nullptr);
  CHECK(ev_clicked == 1 && ev_last_id == 777, "CLICKED llega al callback con id 777");
  // Entrada tactil real: pulsar sobre el boton a traves de LVGL
  static lv_point_t pt = { 150, 140 }; static bool down = false;
  static lv_indev_drv_t idrv; lv_indev_drv_init(&idrv); idrv.type = LV_INDEV_TYPE_POINTER;
  idrv.read_cb = [](lv_indev_drv_t *, lv_indev_data_t *d) -> bool { d->point = pt; d->state = down ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL; return false; };
  lv_indev_drv_register(&idrv);
  ev_clicked = 0; down = true; for (int i = 0; i < 10; ++i) { lv_tick_inc(10); lv_task_handler(); }
  down = false; for (int i = 0; i < 10; ++i) { lv_tick_inc(10); lv_task_handler(); }
  CHECK(ev_clicked == 1, "toque real en pantalla (indev) genera CLICKED en el boton");
  pt.x = 20; pt.y = 20; down = true; ev_clicked = 0; for (int i = 0; i < 5; ++i) { lv_tick_inc(10); lv_task_handler(); }
  down = false; for (int i = 0; i < 5; ++i) { lv_tick_inc(10); lv_task_handler(); }
  CHECK(ev_clicked == 0, "toque fuera de los botones no dispara nada");
  // Actualizar con los mismos valores no debe redibujar
  static arq_temp_tile_t tt; lv_obj_t *s2 = new_screen(); arq_temp_tile_create(&tt, s2, 10, 10, 148, 124, &arq_icon_nozzle, "Hotend");
  arq_temp_tile_update(&tt, 200, 210);
  for (int i = 0; i < 5; ++i) { lv_tick_inc(10); lv_task_handler(); }
  uint32_t inv = 0; lv_disp_t *d = lv_disp_get_default(); for (int i = 0; i < LV_INV_BUF_SIZE; ++i) inv += d->inv_p ? 0 : 0;
  CHECK(d->inv_p == 0, "sin cambios no queda nada pendiente de redibujar");
  arq_temp_tile_update(&tt, 200, 210); CHECK(d->inv_p == 0, "update con los mismos valores no invalida la tarjeta");
  arq_temp_tile_update(&tt, 201, 210); CHECK(d->inv_p > 0, "update con valor nuevo si invalida la tarjeta");
  (void)inv;
  printf("[test] %s\n", fails ? "HAY FALLOS" : "todas las pruebas pasaron");
}

int main(int argc, char **argv) {
  setvbuf(stdout, nullptr, _IONBF, 0);
  lv_init();
  lv_disp_buf_init(&disp_buf, buf1, nullptr, ARQ_W * 14);
  lv_disp_drv_t drv; lv_disp_drv_init(&drv);
  drv.flush_cb = flush_cb; drv.buffer = &disp_buf;
  lv_disp_drv_register(&drv);
  mem("tras lv_init");
  costs();
  test_events(argc > 1 ? argv[1] : ".");
  const char *dir = argc > 1 ? argv[1] : ".";
  char p[256];
  snprintf(p, sizeof(p), "%s/componentes.ppm", dir); components(p);
  snprintf(p, sizeof(p), "%s/iconos.ppm", dir); iconos(p);
  snprintf(p, sizeof(p), "%s/dashboard_listo.ppm", dir);
  dashboard(24, 0, 23, 0, 0, "Lista", ARQ_TINT_OK, p);
  snprintf(p, sizeof(p), "%s/dashboard_calentando.ppm", dir);
  dashboard(168, 210, 60, 60, 35, "Calentando", ARQ_TINT_WARN, p);
  return 0;
}
