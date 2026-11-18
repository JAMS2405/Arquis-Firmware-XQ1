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
#include "arquis_ctrl_view.h"
#include "arquis_files_view.h"
#include "arquis_tools_view.h"
#include "arquis_params_view.h"

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
static int g_thumb_x = 2, g_thumb_y = 36;
static int g_thumb_pct = -1;   // >= 0: simula la miniatura del G-code que la interfaz original pinta fuera de LVGL (x 2, y 36, 200x200)
static void paint_thumb(int pct) {
  const int line = 200 - 200 * pct / 100;
  for (int y = 0; y < 200; ++y)
    for (int x = 0; x < 200; ++x) {
      const int cx = x - 100, cy = y - 110, d2 = cx * cx / 2 + cy * cy;
      uint16_t c = d2 < 70 * 70 ? 0xFD20 : d2 < 90 * 90 ? 0xC618 : 0x7BEF;       // "pieza" naranja con borde gris
      if (y < line) c = (c >> 1) & 0x7BEF;                                         // pendiente: atenuado
      if (y == line && pct < 100) c = 0x05BF;                                      // linea de altura
      fb[(g_thumb_y + y) * ARQ_W + g_thumb_x + x] = c;
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

// ---- Pantallas de control (Etapa 4) ---------------------------------------------------------------------------------------------
static int c_last = -1, c_count = 0;
static void ccb(lv_obj_t *o, lv_event_t ev) { if (ev == LV_EVENT_RELEASED) { ++c_count; c_last = o->mks_obj_id; } }
static arq_temps_model_t tmodel(int sel, int hc, int ht, int bc, int bt, int step = 5) { arq_temps_model_t m; memset(&m, 0, sizeof(m)); m.hot_cur = hc; m.hot_tgt = ht; m.hot_tol = 3; m.bed_cur = bc; m.bed_tgt = bt; m.bed_tol = 3; m.sel = sel; m.step = step; m.p1 = sel ? 60 : 200; m.p2 = sel ? 100 : 240; return m; }
static void temps_screen(const arq_temps_model_t &m, const char *shot, bool hot = true, bool bed = true) { lv_obj_t *s = new_screen(); arq_temps_build(s, ccb, hot, bed, "PLA", "ABS"); arq_temps_update(&m); if (shot) dump(shot); }
static const char *const fan_names[2] = { "Capa", "Aux 1" };
static void fans_screen(uint8_t n, uint8_t sel, uint8_t p0, uint8_t p1, const char *shot) { lv_obj_t *s = new_screen(); arq_fanctl_build(s, ccb, n, fan_names); arq_fanctl_model_t m; memset(&m, 0, sizeof(m)); m.n = n; m.sel = sel; m.pct[0] = p0; m.pct[1] = p1; arq_fanctl_update(&m); if (shot) dump(shot); }
static arq_move_model_t mmodel(int step, int homed) { arq_move_model_t m; memset(&m, 0, sizeof(m)); m.pos_cmm[0] = 12550; m.pos_cmm[1] = -340; m.pos_cmm[2] = 1205; m.homed = homed; m.step = step; return m; }
static void move_screen(const arq_move_model_t &m, const char *shot) { lv_obj_t *s = new_screen(); arq_move_build(s, ccb, false); arq_move_update(&m); if (shot) dump(shot); }
static arq_extrude_model_t emodel(int cur, int tgt, bool ok, int amount) { arq_extrude_model_t m; memset(&m, 0, sizeof(m)); m.cur = cur; m.tgt = tgt; m.tol = 3; m.min_temp = 170; m.hot_enough = ok; m.amount = amount; m.step_mm = 5; m.speed = 1; return m; }
static void extrude_screen(const arq_extrude_model_t &m, const char *shot, bool multi = false) { lv_obj_t *s = new_screen(); arq_extrude_build(s, ccb, multi); arq_extrude_update(&m); if (shot) dump(shot); }
static void opts_screen(bool working, bool objs, bool autooff, const char *shot) {
  lv_obj_t *s = new_screen();
  arq_opt_item_t it[ARQ_OPT_MAX]; uint8_t n = 0;
  it[n++] = { &arq_icon_temp, "Temperatura", nullptr, ARQ_BTN_SECONDARY };
  it[n++] = { &arq_icon_filament, "Filamento", nullptr, ARQ_BTN_SECONDARY };
  it[n++] = { &arq_icon_fan, "Ventiladores", nullptr, ARQ_BTN_SECONDARY };
  it[n++] = { &arq_icon_power, "Autoapagado", autooff ? "Activado" : "Desactivado", (uint8_t)(autooff ? ARQ_BTN_SUCCESS : ARQ_BTN_SECONDARY) };
  if (working) { it[n++] = { &arq_icon_speed, "Velocidad", nullptr, ARQ_BTN_SECONDARY }; it[n++] = { &arq_icon_level, "Babystep", nullptr, ARQ_BTN_SECONDARY }; }
  else { it[n++] = { &arq_icon_down, "Extruir", nullptr, ARQ_BTN_SECONDARY }; it[n++] = { &arq_icon_move, "Mover", nullptr, ARQ_BTN_SECONDARY }; }
  if (objs) it[n++] = { &arq_icon_objects, "Objetos", nullptr, ARQ_BTN_SECONDARY };
  arq_opts_build(s, ccb, "Opciones", it, n);
  if (shot) dump(shot);
}

// ---- Explorador de archivos --------------------------------------------------------------------------------------------------------
static const char *const fnames[ARQ_FILE_ROWS] = { "soporte_motor_nema17_v3_final.gcode", "tapa_caja_electronica.gcode", "pruebas", "benchy.gcode", "calibracion_flujo_0_4mm.gcode", "engrane_helicoidal_z32.gcode" };
static arq_files_model_t fmodel(int n, uint8_t mask, bool up, bool down, bool nocard = false, const char *title = "Archivos") {
  arq_files_model_t m; memset(&m, 0, sizeof(m)); m.n = n; m.dir_mask = mask; m.can_up = up; m.can_down = down; m.no_card = nocard; m.title = title;
  for (int i = 0; i < ARQ_FILE_ROWS; ++i) m.name[i] = fnames[i]; return m; }
static void files_screen(const arq_files_model_t &m, const char *shot) { lv_obj_t *s = new_screen(); arq_files_build(s, ccb); arq_files_update(&m); if (shot) dump(shot); }
static void confirm_screen(const char *file, const char *shot, bool thumb = false) { lv_obj_t *s = new_screen(); arq_confirm_build(s, ccb, file, thumb); if (shot) { if (thumb) { g_thumb_x = ARQ_CONFIRM_THUMB_X; g_thumb_y = ARQ_CONFIRM_THUMB_Y; g_thumb_pct = 0; } dump(shot); g_thumb_pct = -1; g_thumb_x = 2; g_thumb_y = 36; } }

// ---- Etapa 5: Velocidad, Nivelar cama, Sistema, Babystep ---------------------------------------------------------------------------
static void speed_screen(int feed, int flow, int sel, int step, const char *shot) { lv_obj_t *s = new_screen(); arq_speed_build(s, ccb); arq_speed_model_t m = { (int16_t)feed, (int16_t)flow, (uint8_t)sel, (uint8_t)step }; arq_speed_update(&m); if (shot) dump(shot); }
static void level_screen(const char *shot) { lv_obj_t *s = new_screen(); arq_level_model_t m; memset(&m, 0, sizeof(m)); m.bed_x = m.bed_y = 220; int xs[5] = { 30, 190, 190, 30, 110 }, ys[5] = { 30, 30, 190, 190, 110 }; for (int i = 0; i < 5; ++i) { m.x[i] = xs[i]; m.y[i] = ys[i]; } arq_level_build(s, ccb, &m); if (shot) dump(shot); }
static arq_info_list_t il, ir;
static void info_screen(const char *shot) {
  memset(&il, 0, sizeof(il)); memset(&ir, 0, sizeof(ir));
  il.title = "Sistema"; ir.title = "Diagn\xC3\xB3stico";
  const char *ln[7] = { "M\xC3\xA1quina", "Firmware", "Placa", "Compilaci\xC3\xB3n", "Encendida", "Impresiones", "Tiempo total" }, *lv[7] = { "Arquis XQ1", "Marlin 2.1.2.10", "MKS Robin Nano V3", "2026-11-13", "2 h 15 min", "12 (10 ok)", "41 h 20 min" };
  const char *rn[7] = { "Sensor hotend", "Sensor cama", "Final X", "Final Y", "Final Z", "Tarjeta SD", "Marlin" }, *rv[7] = { "25\xC2\xB0", "24\xC2\xB0", "Abierto", "Abierto", "Activado", "Montada", "Normal" };
  const uint8_t dots[7] = { ARQ_TINT_OK, ARQ_TINT_OK, ARQ_TINT_MUTED, ARQ_TINT_MUTED, ARQ_TINT_WARN, ARQ_TINT_OK, ARQ_TINT_OK };
  for (int i = 0; i < 7; ++i) { il.rows[i].label = ln[i]; snprintf(il.rows[i].v1, sizeof(il.rows[i].v1), "%s", lv[i]); il.rows[i].dot = ARQ_TINT_AUTO; rv[i]; ir.rows[i].label = rn[i]; snprintf(ir.rows[i].v1, sizeof(ir.rows[i].v1), "%s", rv[i]); ir.rows[i].dot = dots[i]; }
  il.n = ir.n = 7;
  lv_obj_t *s = new_screen(); arq_info_build(s, ccb, "Sistema y diagn\xC3\xB3stico", &il, &ir); arq_info_refresh(); if (shot) dump(shot);
}
static void baby_screen(int off_um, int step, const char *shot) { lv_obj_t *s = new_screen(); arq_move_build(s, ccb, true); arq_move_model_t m; memset(&m, 0, sizeof(m)); m.step = step; m.z_off_um = off_um; arq_move_update(&m); if (shot) dump(shot); }

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

// ---- Etapa 5B: lista de parametros y teclado numerico -----------------------------------------------------------------------------
static arq_pl_row_t plrow(const char *name, uint8_t kind, const char *v1, const char *v2, int id1, int id2, bool on = false) {
  arq_pl_row_t r; memset(&r, 0, sizeof(r)); r.name = name; r.kind = kind; r.id1 = (int16_t)id1; r.id2 = (int16_t)id2; r.on = on;
  if (v1) strncpy(r.v1, v1, sizeof(r.v1) - 1);
  if (v2) strncpy(r.v2, v2, sizeof(r.v2) - 1);
  return r; }
static void pl_screen(int which, const char *shot) {
  lv_obj_t *s = new_screen();
  arq_pl_begin(s, which == 3 ? "Modo silencioso (StealthChop)" : which == 2 ? "Posiciones de nivelaci\xC3\xB3n" : which == 1 ? "Aceleraci\xC3\xB3n" : "Par\xC3\xA1metros de m\xC3\xA1quina");
  arq_pl_back(s, ccb, 1);
  if (which == 0) { arq_pl_row(s, 0, nullptr, nullptr); }
  const arq_pl_row_t nav[4] = { plrow("Configuraci\xC3\xB3n de la m\xC3\xA1quina", ARQ_PL_NAV, 0, 0, 2, -1), plrow("Motores", ARQ_PL_NAV, 0, 0, 3, -1), plrow("Nivelaci\xC3\xB3n", ARQ_PL_NAV, 0, 0, 4, -1), plrow("Ajustes avanzados", ARQ_PL_NAV, 0, 0, 5, -1) };
  const arq_pl_row_t acc[4] = { plrow("Aceleraci\xC3\xB3n de impresi\xC3\xB3n", ARQ_PL_EDIT1, "500.0", 0, 2, -1), plrow("Aceleraci\xC3\xB3n de retracci\xC3\xB3n", ARQ_PL_EDIT1, "1000.0", 0, 3, -1), plrow("Aceleraci\xC3\xB3n de desplazamiento", ARQ_PL_EDIT1, "1500.0", 0, 4, -1), plrow("Aceleraci\xC3\xB3n m\xC3\xA1xima del eje X", ARQ_PL_EDIT1, "3000", 0, 5, -1) };
  const arq_pl_row_t pos[4] = { plrow("Punto 1 (X, Y)", ARQ_PL_EDIT2, "30", "30", 2, 3), plrow("Punto 2 (X, Y)", ARQ_PL_EDIT2, "190", "30", 4, 5), plrow("Punto 3 (X, Y)", ARQ_PL_EDIT2, "190", "190", 6, 7), plrow("Posici\xC3\xB3n X de pausa (mm, -1 = ninguna)", ARQ_PL_EDIT2, "-1", "-1", 8, 9) };
  const arq_pl_row_t tmc[4] = { plrow("Eje X", ARQ_PL_ONOFF, 0, 0, 2, -1, true), plrow("Eje Y", ARQ_PL_ONOFF, 0, 0, 3, -1, false), plrow("Eje Z", ARQ_PL_ONOFF, 0, 0, 4, -1, true), plrow("Extrusor 0", ARQ_PL_ONOFF, 0, 0, 5, -1, false) };
  const arq_pl_row_t *src = which == 3 ? tmc : which == 2 ? pos : which == 1 ? acc : nav;
  for (int i = 0; i < 4; ++i) arq_pl_row(s, i, &src[i], ccb);
  if (which == 1 || which == 3) arq_pl_page(s, "Siguiente", ccb, 9);
  if (shot) dump(shot);
}
static void numpad_screen(const char *title, const char *val, bool ok, const char *shot) { lv_obj_t *s = new_screen(); arq_numpad_build(s, ccb, title); arq_numpad_set(val, ok); if (shot) dump(shot); }
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

    // 10) Pantallas de control (Etapa 4) --------------------------------------------------------------------------------------
    { new_screen(); run(); const uint32_t b = used(); uint32_t pk = 0;
      auto peak = [&](const char *n, int count) { printf("[mem] %s: pico %u B (%d objetos)\n", n, (unsigned)pk, count); pk = 0; };
      (void)peak;
      struct S { const char *name; void (*mk)(); } scr[] = {
        { "Temperatura", [] { temps_screen(tmodel(0, 187, 210, 44, 60), nullptr); } },
        { "Ventiladores", [] { fans_screen(2, 0, 50, 0, nullptr); } },
        { "Mover", [] { move_screen(mmodel(1, 7), nullptr); } },
        { "Extrusion", [] { extrude_screen(emodel(210, 210, true, 35), nullptr); } },
        { "Opciones", [] { opts_screen(true, true, true, nullptr); } } };
      for (auto &c : scr) { new_screen(); const uint32_t b1 = used(); c.mk(); const uint32_t u = used() - b1; printf("[mem] %s: %u B\n", c.name, (unsigned)u); CHECK(u < 5200, "%s: %u B de heap (< 5200 B)", c.name, (unsigned)u); if (u > pk) pk = u; }
      // peor caso: la pantalla mas grande con un dialogo abierto
      arq_dialog_open("Cambiar filamento", "Se pausara la impresion y el cabezal se movera a la posicion de pausa.", "Pausar y cambiar", "Volver", false, dlg_cb, 901, 902); run();
      lv_mem_monitor_t mm; lv_mem_monitor(&mm); printf("[mem] peor caso (Opciones + dialogo): usado %u B, libre %u B\n", (unsigned)(mm.total_size - mm.free_size), (unsigned)mm.free_size);
      CHECK(mm.free_size >= 8192, "margen de heap en control + dialogo: %u B libres", (unsigned)mm.free_size); arq_dialog_close();
      for (int i = 0; i < 100; ++i) for (auto &c : scr) c.mk();
      new_screen(); run(); const uint32_t u1 = used();
      for (int i = 0; i < 100; ++i) for (auto &c : scr) c.mk();
      new_screen(); run(); const uint32_t u2 = used();
      CHECK(u2 <= u1 && u1 <= b + 16, "500 + 500 ciclos de las 5 pantallas: sin fugas (%u B, %u B; base %u B)", (unsigned)u1, (unsigned)u2, (unsigned)b); }

    { // toques: cada control envia su identificador (y el boton Atras)
      temps_screen(tmodel(0, 187, 210, 44, 60), nullptr);
      struct T { int x, y, id; const char *n; } tt[] = { { 20, 22, ARQ_TID_BACK, "Atras" }, { 100, 90, ARQ_TID_HOT, "tarjeta Hotend" }, { 100, 180, ARQ_TID_BED, "tarjeta Cama" },
        { 50, 290, ARQ_TID_S1, "paso 1" }, { 110, 290, ARQ_TID_S5, "paso 5" }, { 170, 290, ARQ_TID_S10, "paso 10" }, { 300, 170, ARQ_TID_DEC, "menos" }, { 410, 170, ARQ_TID_INC, "mas" },
        { 270, 280, ARQ_TID_P1, "preajuste 1" }, { 345, 280, ARQ_TID_P2, "preajuste 2" }, { 430, 280, ARQ_TID_OFF, "apagar" } };
      for (auto &t : tt) { c_count = 0; tap(t.x, t.y); CHECK(c_count == 1 && c_last == t.id, "Temperatura: toque en %s -> id %d (recibido %d)", t.n, t.id, c_last); }
      fans_screen(2, 0, 50, 0, nullptr);
      struct T2 { int x, y, id; const char *n; } t2[] = { { 100, 90, ARQ_FID_F0, "Capa" }, { 100, 180, ARQ_FID_F1, "Aux 1" }, { 300, 180, ARQ_FID_DEC, "menos" }, { 410, 180, ARQ_FID_INC, "mas" },
        { 270, 280, ARQ_FID_OFF, "apagar" }, { 345, 280, ARQ_FID_HALF, "50%" }, { 430, 280, ARQ_FID_FULL, "100%" } };
      for (auto &t : t2) { c_count = 0; tap(t.x, t.y); CHECK(c_count == 1 && c_last == t.id, "Ventiladores: toque en %s -> id %d (recibido %d)", t.n, t.id, c_last); }
      fans_screen(1, 0, 10, 0, nullptr); c_count = 0; tap(100, 90); CHECK(c_count == 0, "un solo ventilador: la tarjeta no es un boton");
      move_screen(mmodel(1, 7), nullptr);
      struct T3 { int x, y, id; const char *n; } t3[] = { { 120, 90, ARQ_MID_YP, "Y+" }, { 50, 160, ARQ_MID_XN, "X-" }, { 190, 160, ARQ_MID_XP, "X+" }, { 120, 235, ARQ_MID_YN, "Y-" },
        { 285, 100, ARQ_MID_ZP, "Z+" }, { 285, 220, ARQ_MID_ZN, "Z-" }, { 400, 175, ARQ_MID_S01, "0,1 mm" }, { 400, 230, ARQ_MID_S1, "1 mm" }, { 400, 290, ARQ_MID_S10, "10 mm" } };
      for (auto &t : t3) { c_count = 0; tap(t.x, t.y); CHECK(c_count == 1 && c_last == t.id, "Mover: toque en %s -> id %d (recibido %d)", t.n, t.id, c_last); }
      extrude_screen(emodel(210, 210, true, 0), nullptr);
      c_count = 0; tap(360, 90); CHECK(c_count == 1 && c_last == ARQ_EID_IN, "Extrusion: con el hotend caliente 'Extruir' responde");
      c_count = 0; tap(360, 180); CHECK(c_count == 1 && c_last == ARQ_EID_OUT, "Extrusion: con el hotend caliente 'Retraer' responde");
      extrude_screen(emodel(28, 0, false, 0), nullptr);
      c_count = 0; tap(360, 90); CHECK(c_count == 0, "SEGURIDAD: con el hotend frio 'Extruir' no recibe toques");
      c_count = 0; tap(360, 180); CHECK(c_count == 0, "SEGURIDAD: con el hotend frio 'Retraer' no recibe toques");
      c_count = 0; tap(120, 290); CHECK(c_count == 1 && c_last == ARQ_EID_TEMP, "Extrusion en frio: el acceso a Temperatura sigue disponible");
      opts_screen(true, true, false, nullptr);
      c_count = 0; tap(60, 120); CHECK(c_count == 1 && c_last == ARQ_XID_ITEM0, "Opciones: primer acceso -> id %d", c_last);
      c_count = 0; tap(420, 120); CHECK(c_count == 1 && c_last == ARQ_XID_ITEM0 + 3, "Opciones: cuarto acceso (autoapagado) -> id %d", c_last);
      c_count = 0; tap(20, 22); CHECK(c_count == 1 && c_last == ARQ_XID_BACK, "Opciones: Atras");
      // un cambio de dato no redibuja de mas
      lv_disp_t *d = lv_disp_get_default(); temps_screen(tmodel(0, 187, 210, 44, 60), nullptr); run();
      arq_temps_model_t same = tmodel(0, 187, 210, 44, 60); arq_temps_update(&same); CHECK(d->inv_p == 0, "Temperatura: modelo sin cambios, nada que redibujar");
      arq_temps_model_t chg = tmodel(0, 188, 210, 44, 60); arq_temps_update(&chg); CHECK(d->inv_p > 0 && d->inv_p <= 6, "Temperatura: un grado de cambio invalida pocas zonas (%u)", (unsigned)d->inv_p);
      new_screen(); run(); }

    // 11) Explorador de archivos ----------------------------------------------------------------------------------------------
    { new_screen(); run(); const uint32_t b = used();
      files_screen(fmodel(6, 1 << 2, false, true), nullptr); const uint32_t u = used() - b;
      printf("[mem] explorador: %u B\n", (unsigned)u);
      CHECK(u < 3000, "explorador: %u B de heap (< 3000 B), 5 objetos con 6 filas", (unsigned)u);
      for (int i = 0; i < 300; ++i) files_screen(fmodel(i % 7, i & 5, i & 1, i & 2), nullptr);
      new_screen(); run(); const uint32_t u1 = used();
      for (int i = 0; i < 300; ++i) files_screen(fmodel(i % 7, i & 5, i & 1, i & 2), nullptr);
      new_screen(); run(); const uint32_t u2 = used();
      CHECK(u2 <= u1 && u1 <= b + 16, "explorador, 300 + 300 ciclos: sin fugas (%u B, %u B; base %u B)", (unsigned)u1, (unsigned)u2, (unsigned)b);
      // toques: cada fila devuelve su indice mediante arq_files_pick (como el glue de draw_print_file.cpp)
      static int pick_last; static lv_obj_t *list_obj;
      auto fcb = [](lv_obj_t *o, lv_event_t ev) { if (ev != LV_EVENT_RELEASED) return; c_last = o->mks_obj_id;
        if (o->mks_obj_id == ARQ_FLID_LIST) { lv_indev_t *in = lv_indev_get_act(); lv_point_t p; lv_indev_get_point(in, &p); pick_last = arq_files_pick(o, &p); } };
      (void)list_obj;
      { lv_obj_t *sc = new_screen(); arq_files_build(sc, fcb); arq_files_model_t m = fmodel(6, 1 << 2, true, true); arq_files_update(&m); run();
        for (int i = 0; i < 6; ++i) { pick_last = -9; tap(200, 52 + i * 42 + 19); CHECK(pick_last == i, "explorador: tocar la fila %d devuelve %d", i + 1, pick_last); }
        pick_last = -9; tap(200, 52 + 40); CHECK(pick_last == -1, "explorador: el hueco entre filas no selecciona nada");
        c_last = -1; tap(444, 100); CHECK(c_last == ARQ_FLID_UP, "explorador: pagina anterior"); c_last = -1; tap(444, 250); CHECK(c_last == ARQ_FLID_DOWN, "explorador: pagina siguiente");
        c_last = -1; tap(20, 22); CHECK(c_last == ARQ_FLID_BACK, "explorador: Atras");
        arq_files_model_t m2 = fmodel(3, 0, false, false); arq_files_update(&m2); run(); pick_last = -9; tap(200, 52 + 4 * 42 + 19); CHECK(pick_last == -1, "explorador: una fila sin archivo no se puede tocar");
        arq_files_model_t m3 = fmodel(6, 0, false, true); arq_files_update(&m3); run(); c_last = -1; tap(444, 100); CHECK(c_last == -1, "explorador: sin pagina anterior el boton esta deshabilitado");
        confirm_screen("soporte.gcode", nullptr, true); c_count = 0; tap(280, 240); CHECK(c_count == 1 && c_last == ARQ_PC_CANCEL, "confirmar: Cancelar"); c_count = 0; tap(420, 240); CHECK(c_count == 1 && c_last == ARQ_PC_OK, "confirmar: Imprimir");
        new_screen(); run(); } }
    // 12) Etapa 5: Velocidad, Nivelar cama, Sistema, Babystep ----------------------------------------------------------------
    { new_screen(); run(); const uint32_t b = used();
      struct S { const char *name; void (*mk)(); } scr[] = {
        { "Velocidad", [] { speed_screen(100, 85, 0, 5, nullptr); } }, { "Nivelar cama", [] { level_screen(nullptr); } },
        { "Sistema", [] { info_screen(nullptr); } }, { "Babystep", [] { baby_screen(-1250, 0, nullptr); } } };
      for (auto &c : scr) { new_screen(); const uint32_t b1 = used(); c.mk(); const uint32_t u = used() - b1; printf("[mem] %s: %u B\n", c.name, (unsigned)u); CHECK(u < 5200, "%s: %u B de heap (< 5200 B)", c.name, (unsigned)u); }
      for (int i = 0; i < 100; ++i) for (auto &c : scr) c.mk();
      new_screen(); run(); const uint32_t u1 = used();
      for (int i = 0; i < 100; ++i) for (auto &c : scr) c.mk();
      new_screen(); run(); const uint32_t u2 = used();
      CHECK(u2 <= u1 && u1 <= b + 16, "Etapa 5: 400 + 400 ciclos de las 4 pantallas: sin fugas (%u B, %u B; base %u B)", (unsigned)u1, (unsigned)u2, (unsigned)b);
      speed_screen(100, 85, 0, 5, nullptr);
      struct T { int x, y, id; const char *n; } t4[] = { { 20, 22, ARQ_SID_BACK, "Atras" }, { 100, 90, ARQ_SID_FEED, "Movimiento" }, { 100, 180, ARQ_SID_FLOW, "Extrusion" }, { 50, 290, ARQ_SID_S1, "1%" },
        { 110, 290, ARQ_SID_S5, "5%" }, { 170, 290, ARQ_SID_S10, "10%" }, { 300, 250, ARQ_SID_DEC, "menos" }, { 410, 250, ARQ_SID_INC, "mas" } };
      for (auto &t : t4) { c_count = 0; tap(t.x, t.y); CHECK(c_count == 1 && c_last == t.id, "Velocidad: toque en %s -> id %d (recibido %d)", t.n, t.id, c_last); }
      level_screen(nullptr);
      // las posiciones de los 5 botones caen donde corresponde: 1 abajo-izquierda, 2 abajo-derecha, 3 arriba-derecha, 4 arriba-izquierda, 5 centro
      struct P { int x, y, id; const char *n; } pp[] = { { 6 + 4 + 5 + 27, 52 + 4 + 5 + 27 + 190 - 0, ARQ_LID_P1, "punto 1" } };
      (void)pp;
      { lv_obj_t *sc = lv_scr_act(); int found = 0; for (lv_obj_t *c = lv_obj_get_child_back(sc, nullptr); c; c = lv_obj_get_child_back(sc, c)) { if (lv_obj_get_click(c) && c->mks_obj_id >= ARQ_LID_P1 && c->mks_obj_id <= ARQ_LID_P5) {
            lv_area_t a; lv_obj_get_coords(c, &a); c_count = 0; tap((a.x1 + a.x2) / 2, (a.y1 + a.y2) / 2); if (c_count == 1 && c_last == c->mks_obj_id) ++found; } }
        CHECK(found == 5, "Nivelar cama: los 5 puntos reciben su toque (%d de 5)", found); }
      c_count = 0; tap(20, 22); CHECK(c_count == 1 && c_last == ARQ_LID_BACK, "Nivelar cama: Atras");
      baby_screen(0, 1, nullptr);
      struct T5 { int x, y, id; const char *n; } t5[] = { { 120, 90, ARQ_MID_YP, "Y+" }, { 50, 160, ARQ_MID_XN, "X-" }, { 285, 100, ARQ_MID_ZP, "Z+" }, { 285, 220, ARQ_MID_ZN, "Z-" }, { 400, 145, ARQ_MID_S01, "0,01 mm" }, { 400, 210, ARQ_MID_S1, "0,05 mm" }, { 400, 278, ARQ_MID_S10, "0,1 mm" } };
      for (auto &t : t5) { c_count = 0; tap(t.x, t.y); CHECK(c_count == 1 && c_last == t.id, "Babystep: toque en %s -> id %d (recibido %d)", t.n, t.id, c_last); }
      new_screen(); run(); }
  }

    // 13) Etapa 5B: lista de parametros y teclado numerico -------------------------------------------------------------------
    { static lv_point_t pt; static bool down = false;
      static lv_indev_drv_t idrv; lv_indev_drv_init(&idrv); idrv.type = LV_INDEV_TYPE_POINTER;
      idrv.read_cb = [](lv_indev_drv_t *, lv_indev_data_t *d) -> bool { d->point = pt; d->state = down ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL; return false; };
      lv_indev_drv_register(&idrv);
      auto tap = [&](int x, int y) { pt.x = x; pt.y = y; down = true; run(6); down = false; run(6); };
      new_screen(); run(); const uint32_t b = used();
      struct S { const char *name; int w; } sc5[] = { { "Parametros (navegacion)", 0 }, { "Parametros (edicion)", 1 }, { "Parametros (2 valores)", 2 }, { "Parametros (interruptores)", 3 } };
      for (auto &c : sc5) { new_screen(); const uint32_t b1 = used(); pl_screen(c.w, nullptr); const uint32_t u = used() - b1; printf("[mem] %s: %u B\n", c.name, (unsigned)u); CHECK(u < 3200, "%s: %u B de heap (< 3200 B)", c.name, (unsigned)u); }
      { new_screen(); const uint32_t b1 = used(); numpad_screen("Aceleraci\xC3\xB3n de impresi\xC3\xB3n", "500", true, nullptr); const uint32_t u = used() - b1; printf("[mem] teclado: %u B\n", (unsigned)u); CHECK(u < 4000, "teclado numerico: %u B de heap (< 4000 B)", (unsigned)u); }
      for (int i = 0; i < 150; ++i) { for (auto &c : sc5) pl_screen(c.w, nullptr); numpad_screen("Valor", "12.5", i & 1, nullptr); }
      new_screen(); run(); const uint32_t u1 = used();
      for (int i = 0; i < 150; ++i) { for (auto &c : sc5) pl_screen(c.w, nullptr); numpad_screen("Valor", "12.5", i & 1, nullptr); }
      new_screen(); run(); const uint32_t u2 = used();
      CHECK(u2 <= u1 && u1 <= b + 16, "Etapa 5B: 750 + 750 pantallas: sin fugas (%u B, %u B; base %u B)", (unsigned)u1, (unsigned)u2, (unsigned)b);
      // toques sobre la lista: cada fila / caja devuelve el id que espera el manejador original
      pl_screen(1, nullptr); run();
      for (int i = 0; i < 4; ++i) { c_count = 0; tap(200, 52 + i * 62 + 27); CHECK(c_count == 1 && c_last == 2 + i, "lista de edicion: tocar la fila %d -> id %d (recibido %d)", i + 1, 2 + i, c_last); }
      c_count = 0; tap(200, 52 + 58); CHECK(c_count == 0, "lista: el hueco entre filas no pulsa nada");
      c_count = 0; tap(20, 22); CHECK(c_count == 1 && c_last == 1, "lista: Atras");
      c_count = 0; tap(420, 22); CHECK(c_count == 1 && c_last == 9, "lista: boton de pagina");
      pl_screen(2, nullptr); run();
      c_count = 0; tap(440, 52 + 27); CHECK(c_count == 1 && c_last == 2, "2 valores: caja derecha -> id1");
      c_count = 0; tap(330, 52 + 27); CHECK(c_count == 1 && c_last == 3, "2 valores: caja izquierda -> id2");
      c_count = 0; tap(80, 52 + 27); CHECK(c_count == 0, "2 valores: el nombre no es pulsable");
      pl_screen(3, nullptr); run();
      c_count = 0; tap(200, 52 + 27); CHECK(c_count == 1 && c_last == 2, "interruptor: tocar la fila -> id");
      arq_pl_set_on(false); arq_pl_set_on(true);
      CHECK(arq_pl_last_name() && strcmp(arq_pl_last_name(), "Eje X") == 0, "lista: recuerda el nombre de la fila pulsada para el titulo del teclado");
      // teclado: cada tecla devuelve su id; Aceptar deshabilitado no responde
      numpad_screen("Valor", "10", true, nullptr); run();
      const int kx[4] = { 10 + 54, 10 + 54 + 117, 10 + 54 + 234, 10 + 54 + 351 }; const int ky[4] = { 106 + 22, 106 + 22 + 51, 106 + 22 + 102, 106 + 22 + 153 };
      const int want[4][4] = { { ARQ_NK_1, 2, 3, ARQ_NK_BKSP }, { 4, 5, 6, ARQ_NK_CLEAR }, { 7, 8, 9, ARQ_NK_CANCEL }, { ARQ_NK_MINUS, ARQ_NK_0, ARQ_NK_POINT, ARQ_NK_OK } };
      int okk = 0; for (int r = 0; r < 4; ++r) for (int c = 0; c < 4; ++c) { c_count = 0; tap(kx[c], ky[r]); if (c_count == 1 && c_last == want[r][c]) ++okk; }
      CHECK(okk == 16, "teclado: las 16 teclas devuelven su id (%d de 16)", okk);
      c_count = 0; tap(20, 22); CHECK(c_count == 1 && c_last == ARQ_NK_BACK, "teclado: flecha Atras");
      numpad_screen("Valor", "-", false, nullptr); run(); c_count = 0; tap(kx[3], ky[3]); CHECK(c_count == 0, "teclado: Aceptar no responde con un valor no valido");
      c_count = 0; tap(kx[0], ky[0]); CHECK(c_count == 1 && c_last == ARQ_NK_1, "teclado: los digitos siguen respondiendo");
      new_screen(); run(); }

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
  { temps_screen(tmodel(0, 187, 210, 44, 60), "ctl_1_temp_hotend");
    temps_screen(tmodel(1, 25, 0, 60, 60, 10), "ctl_2_temp_cama");
    fans_screen(2, 0, 50, 0, "ctl_3_ventiladores");
    fans_screen(1, 0, 100, 0, "ctl_4_ventilador_unico");
    move_screen(mmodel(1, 7), "ctl_5_mover");
    move_screen(mmodel(0, 3), "ctl_6_mover_sin_home");
    extrude_screen(emodel(210, 210, true, 35), "ctl_7_extruir");
    extrude_screen(emodel(28, 0, false, 0), "ctl_8_extruir_frio");
    opts_screen(true, true, true, "ctl_9_opciones_imprimiendo");
    opts_screen(false, false, false, "ctl_10_opciones_pausa");
    files_screen(fmodel(6, 1 << 2, false, true), "fil_1_lista");
    files_screen(fmodel(4, 1 << 2, true, false, false, "pruebas"), "fil_2_carpeta");
    files_screen(fmodel(0, 0, false, false, true), "fil_3_sin_sd");
    files_screen(fmodel(0, 0, false, false, false, "pruebas"), "fil_4_vacia");
    confirm_screen("soporte_motor_nema17_v3_final.gcode", "fil_5_confirmar", true);
    confirm_screen("tapa_caja.gcode", "fil_6_confirmar_sin_miniatura", false);
    { lv_obj_t *sc = new_screen(); arq_opt_item_t it[8] = { { &arq_icon_temp, "Temperatura", nullptr, 1 }, { &arq_icon_down, "Extruir", nullptr, 1 }, { &arq_icon_move, "Mover", nullptr, 1 }, { &arq_icon_home, "Home", nullptr, 1 },
        { &arq_icon_level, "Nivel. auto", nullptr, 1 }, { &arq_icon_level, "Nivelar cama", nullptr, 1 }, { &arq_icon_filament, "Filamento", nullptr, 1 }, { &arq_icon_tools, "M\xC3\xA1s", nullptr, 1 } };
      arq_opts_build(sc, ccb, "Herramientas", it, 8); dump("st5_5_herramientas"); }
    { lv_obj_t *sc = new_screen(); arq_opt_item_t it[5] = { { &arq_icon_gear, "Par\xC3\xA1metros", nullptr, 1 }, { &arq_icon_file, "Memoria", nullptr, 1 }, { &arq_icon_fan, "Ventiladores", nullptr, 1 }, { &arq_icon_tools, "Sistema", nullptr, 1 }, { &arq_icon_power, "Apagar", nullptr, 1 } };
      arq_opts_build(sc, ccb, "Ajustes", it, 5); dump("st5_6_ajustes"); }
    { lv_obj_t *sc = new_screen(); arq_opt_item_t it[6] = { { &arq_icon_home, "Home todo", nullptr, 1 }, { &arq_icon_right, "Home X", nullptr, 1 }, { &arq_icon_up, "Home Y", nullptr, 1 }, { &arq_icon_down, "Home Z", nullptr, 1 }, { &arq_icon_power, "Motores off", nullptr, 1 }, { &arq_icon_power, "Motores XY", nullptr, 1 } };
      arq_opts_build(sc, ccb, "Home", it, 6); dump("st5_7_home"); }
    speed_screen(100, 85, 0, 5, "st5_1_velocidad"); level_screen("st5_2_nivelar"); info_screen("st5_3_sistema"); baby_screen(-1250, 0, "st5_4_babystep");
    pl_screen(0, "st5b_1_parametros"); pl_screen(1, "st5b_2_aceleracion"); pl_screen(2, "st5b_3_dos_valores"); pl_screen(3, "st5b_4_interruptores");
    numpad_screen("Aceleraci\xC3\xB3n de impresi\xC3\xB3n", "500.0", true, "st5b_5_teclado"); numpad_screen("Posici\xC3\xB3n X", "-", false, "st5b_6_teclado_invalido");

  }
  tests();
  return fails ? 1 : 0;
}
