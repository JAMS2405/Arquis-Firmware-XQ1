// Simulador PC del asistente del primer arranque y de Ajustes > Maquina (firmware unico XQ1 / Aquila D1).
// Compila el codigo REAL (arquis_machine_ui.cpp, arquis_machine_logic.cpp, arquis_params_view.cpp, widgets, tema) sobre
// LVGL 6.1.1-MKS REAL. No ejecuta Marlin: una "maquina" simulada responde a lo que pide el pegamento (arquis_machine.h):
// finales de carrera (pines), movimientos, home, temperatura del hotend, EEPROM. Uso: build_machine_sim.sh
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "arquis_machine_ui.h"
#include "arquis_params.h"
#include "arquis_widgets.h"
#include "draw_ui.h"
#include "../../../module/planner.h"
#include "../../../module/temperature.h"
#include "../../../module/settings.h"
#include "../../../module/motion.h"
#include "../../../gcode/queue.h"

extern "C" { uint32_t lv_open_gcode_file(char *) { return 0; } void lv_gcode_file_read(uint8_t *) {} void lv_close_gcode_file() {} void lv_log_register_print_cb(void *) {} }

// ---- Maquina simulada ------------------------------------------------------------------------------------------------
static uint32_t sim_ms = 1000;
uint32_t millis() { return sim_ms; }
static bool pin_raw[3];                      // nivel electrico de los finales X, Y, Z
bool sim_read_pin(int p) { return p >= 0 && p < 3 ? pin_raw[p] : false; }
static uint8_t homed;
bool axis_was_homed(const AxisEnum a) { return (homed >> a) & 1; }
void set_axis_unhomed(const AxisEnum a) { homed &= ~(1 << a); }
Planner planner; Planner::planner_settings_t Planner::settings;
bool Planner::has_blocks_queued() { return false; }
Temperature thermalManager; static int16_t hot = 25, hot_tgt = 0;
celsius_t Temperature::wholeDegHotend(const uint8_t) { return hot; }
celsius_t Temperature::degTargetHotend(const uint8_t) { return hot_tgt; }
void Temperature::setTargetHotend(const celsius_t c, const uint8_t) { hot_tgt = c; }
MarlinSettings settings; static int saves, loads; static bool marlin_ee_valid = true;
bool MarlinSettings::save() { ++saves; return true; }
bool MarlinSettings::load() { ++loads; return true; }
bool MarlinSettings::validate() { return marlin_ee_valid; }
GCodeQueue queue; static char last_cmd[64]; static int cmds;
void GCodeQueue::inject(FSTR_P const g) {
  snprintf(last_cmd, sizeof(last_cmd), "%s", g); ++cmds;
  if (!strncmp(g, "G28 ", 4)) homed |= 1 << (g[4] - 'X');                      // el home termina en la simulacion
}
bool GCodeQueue::has_commands_queued() { return false; }
DISP_STATE_STACK disp_state_stack; GCFG gCfgItems; lv_group_t *g;
static int ready_draws, set_draws, para_draws, spi_saves;
// Como en MKS: quien sale borra su pantalla (lv_clear_*) y quien entra crea la suya; aqui no se borra la anterior.
static lv_obj_t * new_screen() { lv_obj_t *s = lv_obj_create(nullptr, nullptr); lv_scr_load(s); return s; }
lv_obj_t* lv_screen_create(DISP_STATE st, const char *) { disp_state_stack._disp_state[++disp_state_stack._disp_index] = st; return new_screen(); }
void lv_draw_set() { ++set_draws; new_screen(); }
void lv_draw_ready_print() { ++ready_draws; new_screen(); }
void lv_draw_machine_para() { ++para_draws; new_screen(); }
void update_spi_flash() { ++spi_saves; }
bool arq_numpad_value_ok(const char *s) { if (!s) return false; for (; *s; ++s) if (*s >= '0' && *s <= '9') return true; return false; }   // = arquis_params.cpp

// Pegamento simulado (mismo contrato que arquis_machine.cpp)
static arq_mc_cfg_t cur; static bool rec_valid, test_mode; static const char *notice; static int commits;
static arq_mc_cfg_t marlin;                  // lo que "Marlin" tiene en RAM (pasos, velocidades...)
uint8_t arq_mc_profile_id() { return rec_valid ? cur.profile : 0; }
uint8_t arq_mc_flags() { return cur.flags; }
const char *arq_mc_name() { const arq_mc_profile_t *p = arq_mc_profile(arq_mc_profile_id()); return p ? p->name : "ARQUIS"; }
bool arq_mc_needs_wizard() { return !rec_valid || !(cur.flags & ARQ_MCF_DONE); }
bool arq_mc_printing() { return false; }
uint8_t arq_mc_block_reason() {
  if (!test_mode) { if (!rec_valid) return ARQ_BLK_NOPROFILE; if (!(cur.flags & ARQ_MCF_DONE)) return ARQ_BLK_WIZARD;
                    if ((cur.flags & 6) != 6) return ARQ_BLK_UNVERIFIED; }
  return ARQ_BLK_NONE;
}
bool arq_mc_motion_ok() { return !arq_mc_block_reason(); }
const char *arq_mc_block_text(uint8_t r) { return r ? "Bloqueado (simulado)." : ""; }
void arq_mc_idle() {}
uint8_t arq_mc_tmc_verify(const bool) { return ARQ_TMC_OK; }
float arq_mc_shaping_min() { return 25; }
void arq_mc_test_mode(const bool on) { test_mode = on; }
void arq_mc_notice(const char *t) { notice = t; }
const char *arq_mc_take_notice() { const char *t = notice; notice = nullptr; return t; }
void arq_mc_read_current(arq_mc_cfg_t *c) {
  *c = marlin; c->profile = arq_mc_profile_id(); c->flags = cur.flags; c->es_inv = cur.es_inv; c->dir_inv = cur.dir_inv;
  for (int p = ARQ_MP_BED_X; p <= ARQ_MP_Z_MAX; ++p) c->v[p] = cur.v[p];
  for (int i = 0; i < 4; ++i) { c->v[ARQ_MP_STEPS_X + i] = planner.settings.axis_steps_per_mm[i]; }
}
void arq_mc_apply(const arq_mc_cfg_t *c, const bool mech) {
  if (c->es_inv != cur.es_inv) cur.flags &= ~ARQ_MCF_ES_OK;
  if (c->dir_inv != cur.dir_inv) cur.flags &= ~(ARQ_MCF_DIR_OK | ARQ_MCF_E_OK);
  cur.es_inv = c->es_inv; cur.dir_inv = c->dir_inv;
  for (int p = ARQ_MP_BED_X; p <= ARQ_MP_Z_MAX; ++p) cur.v[p] = c->v[p];
  if (mech) { marlin = *c; for (int i = 0; i < 4; ++i) planner.settings.axis_steps_per_mm[i] = c->v[ARQ_MP_STEPS_X + i]; }
}
uint8_t arq_mc_commit(const arq_mc_cfg_t *c, int *bad) {
  const int b = arq_mc_validate(c, 25); if (bad) *bad = b;
  if (b >= 0) return ARQ_MCS_E_INVALID;
  arq_mc_apply(c, true); cur.flags = c->flags; cur.profile = c->profile; rec_valid = true; ++commits; settings.save();
  return ARQ_MCS_OK;
}
uint8_t arq_mc_restore_all() { arq_mc_cfg_t c; arq_mc_factory(cur.profile, &c); c.flags = cur.flags; return arq_mc_commit(&c, nullptr); }

// ---- LVGL, capturas y toques --------------------------------------------------------------------------------------------
static uint16_t fb[ARQ_W * ARQ_H]; static lv_disp_buf_t disp_buf; static lv_color_t buf1[ARQ_W * 14]; static const char *outdir = ".";
static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *a, lv_color_t *c) { for (int y = a->y1; y <= a->y2; ++y) for (int x = a->x1; x <= a->x2; ++x) fb[y * ARQ_W + x] = (c++)->full; lv_disp_flush_ready(drv); }
static uint32_t used() { lv_mem_monitor_t m; lv_mem_monitor(&m); return m.total_size - m.free_size; }
static uint32_t peak_used;
static void run(int n = 12) { for (int i = 0; i < n; ++i) { sim_ms += 10; lv_tick_inc(10); lv_task_handler(); arq_machine_tick(); if (used() > peak_used) peak_used = used(); } }
static int shots;
static void dump(const char *name) {
  run(); char p[256]; snprintf(p, sizeof(p), "%s/%02d_%s.ppm", outdir, ++shots, name);
  FILE *f = fopen(p, "wb"); fprintf(f, "P6\n%d %d\n255\n", ARQ_W, ARQ_H);
  for (int i = 0; i < ARQ_W * ARQ_H; ++i) { const uint16_t v = fb[i]; const uint8_t r = (v >> 11) & 31, g = (v >> 5) & 63, b = v & 31; fputc((r << 3) | (r >> 2), f); fputc((g << 2) | (g >> 4), f); fputc((b << 3) | (b >> 2), f); }
  fclose(f);
}
static int fails = 0, checks = 0;
#define CHECK(c, ...) do { ++checks; if (c) printf("[maquina] OK    "); else { printf("[maquina] FALLA "); ++fails; } printf(__VA_ARGS__); printf("\n"); } while (0)
static lv_point_t pt; static bool down;
static void tap(int x, int y) { pt = { (lv_coord_t)x, (lv_coord_t)y }; down = true; run(4); down = false; run(6); }
// Geometria de la lista de parametros (arquis_params_view.cpp) y del dialogo (arquis_widgets.cpp)
static int row_y(int i) { return 52 + i * 62 + 27; }
enum { BOX1_X = 405, BOX2_X = 293, NAME_X = 100, NEXT_X = 415, HDR_Y = 22, BACK_X = 22, DLG_OK2_X = 331, DLG_NO_X = 149, DLG_OK1_X = 240, DLG_Y = 208 };
static void next() { tap(NEXT_X, HDR_Y); }
static void back() { tap(BACK_X, HDR_Y); }
static void dlg_ok() { tap(DLG_OK2_X, DLG_Y); }
static void dlg_no() { tap(DLG_NO_X, DLG_Y); }
static void dlg_ok1() { tap(DLG_OK1_X, DLG_Y); }
static void key(int r, int c) { tap(10 + c * (109 + 8) + 54, 106 + r * (46 + 6) + 23); }
static void type_num(const char *s) {                      // teclado: 1 2 3 / 4 5 6 / 7 8 9 / - 0 .
  key(1, 3);                                               // Borrar todo
  for (; *s; ++s) {
    if (*s == '.') key(3, 2); else if (*s == '-') key(3, 0); else if (*s == '0') key(3, 1);
    else { const int d = *s - '1'; key(d / 3, d % 3); }
  }
  key(3, 3);                                               // Aceptar
}
static void press_es(int a) { pin_raw[a] = !pin_raw[a]; run(30); pin_raw[a] = !pin_raw[a]; run(30); }

int main(int argc, char **argv) {
  setvbuf(stdout, nullptr, _IONBF, 0); if (argc > 1) outdir = argv[1];
  lv_init(); lv_disp_buf_init(&disp_buf, buf1, nullptr, ARQ_W * 14);
  lv_disp_drv_t drv; lv_disp_drv_init(&drv); drv.flush_cb = flush_cb; drv.buffer = &disp_buf; lv_disp_drv_register(&drv);
  static lv_indev_drv_t idrv; lv_indev_drv_init(&idrv); idrv.type = LV_INDEV_TYPE_POINTER;
  idrv.read_cb = [](lv_indev_drv_t *, lv_indev_data_t *d) -> bool { d->point = pt; d->state = down ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL; return false; };
  lv_indev_drv_register(&idrv);
  g = lv_group_create(); gCfgItems.encoder_enable = true;
  const uint32_t heap0 = used();

  // Primera instalacion sobre un Aquila D1: EEPROM de Marlin valida con sus pasos; sin registro ARQUIS.
  arq_mc_factory(ARQ_MC_AQUILA_D1, &marlin);
  for (int i = 0; i < 4; ++i) { planner.settings.axis_steps_per_mm[i] = marlin.v[ARQ_MP_STEPS_X + i]; }
  arq_mc_factory(ARQ_MC_XQ1, &cur); cur.profile = 0; rec_valid = false;       // RAM sin perfil
  // Finales del Aquila: logica normal (no invertida): en reposo el pin esta a 0 -> "Abierto"
  pin_raw[0] = pin_raw[1] = false; pin_raw[2] = true;                          // Z: logica invertida (Aquila Z_MIN_ENDSTOP_INVERTING true)

  // ---- Paso 1: modelo ----
  marlin_ee_valid = false;                                  // EEPROM de Marlin no valida: no se detecta ni se preselecciona
  arq_machine_wizard_draw(); run(); dump("asistente_1_modelo_sin_eeprom");
  next(); CHECK(arq_dialog_is_open(), "Siguiente sin modelo elegido: aviso"); dlg_ok1();
  marlin_ee_valid = true;                                   // EEPROM valida con los pasos del Aquila: se detecta y se preselecciona
  arq_machine_wizard_draw(); run(); dump("asistente_1_modelo_detectado");
  tap(BOX1_X, row_y(0)); dump("asistente_1_xq1_elegida_aviso");   // elegir XQ1 sobre una EEPROM del Aquila: aviso en el chip
  tap(BOX1_X, row_y(1)); dump("asistente_1_aquila_elegida");
  next(); run();
  CHECK(!arq_dialog_is_open(), "modelo detectado y confirmado: pasa al paso 2");
  // ---- Paso 2: dimensiones ----
  dump("asistente_2_dimensiones");
  tap(BOX1_X, row_y(0)); dump("asistente_2_teclado"); type_num("260"); dump("asistente_2_fuera_de_rango");
  CHECK(arq_dialog_is_open(), "ancho 260 en un Aquila (max 240): rechazado con aviso"); dlg_ok1();
  key(2, 3); run();                                         // Cancelar en el teclado
  next();
  // ---- Paso 3: finales ----
  dump("asistente_3_finales_inicio");
  next(); CHECK(arq_dialog_is_open(), "Siguiente sin comprobar los finales: aviso"); dlg_ok1();
  // logica de Z mal puesta a proposito: el asistente la muestra "Activado" en reposo
  tap(BOX1_X, row_y(2)); run(20); dump("asistente_3_z_logica_equivocada");
  tap(BOX1_X, row_y(2)); run(150);                         // se corrige
  for (int a = 0; a < 3; ++a) press_es(a);
  run(20); dump("asistente_3_finales_comprobados");
  next(); run();
  CHECK(!arq_dialog_is_open(), "con los 3 finales comprobados se puede seguir");
  // ---- Paso 4: direcciones ----
  dump("asistente_4_direcciones");
  tap(BOX2_X, row_y(0)); dump("asistente_4_probar_x"); dlg_ok(); run(80);
  CHECK(!strcmp(last_cmd, "G91\nG1 X10 F600\nG90"), "prueba de X: G91 / G1 X10 F600 / G90 (%s)", last_cmd);
  dump("asistente_4_pregunta_x"); dlg_ok();                // si
  tap(BOX2_X, row_y(1)); dlg_ok(); run(80); dlg_no();      // Y: no -> se invierte
  dump("asistente_4_y_invertida"); dlg_ok1();
  CHECK(((arq_mc_profile(2)->dir_inv ^ 0x02)) != 0, "Y invertido tras responder No");
  tap(BOX2_X, row_y(1)); dlg_ok(); run(80); dlg_ok();      // Y otra vez: si
  tap(BOX2_X, row_y(2)); dlg_ok(); run(80);
  CHECK(!strcmp(last_cmd, "G91\nG1 Z5 F300\nG90"), "prueba de Z: 5 mm a 5 mm/s");
  dlg_ok();
  tap(BOX2_X, row_y(3)); dump("asistente_4_calentar_e"); dlg_ok(); run();
  CHECK(hot_tgt == 200, "extrusor: calienta a 200");
  hot = 199; run(40); dump("asistente_4_e_caliente");
  tap(BOX2_X, row_y(3)); dlg_ok(); run(80);
  CHECK(!strcmp(last_cmd, "M83\nG1 E10 F120\nM82"), "prueba de E: 10 mm relativos");
  dlg_ok(); run();
  CHECK(hot_tgt == 0, "tras probar E se apaga el hotend");
  dump("asistente_4_direcciones_ok");
  next(); run();
  // ---- Paso 5: home ----
  dump("asistente_5_home");
  next(); CHECK(arq_dialog_is_open(), "Siguiente sin home: aviso"); dlg_ok1();
  for (int a = 0; a < 3; ++a) { tap(BOX1_X, row_y(a)); dlg_ok(); run(80); dlg_ok(); }
  CHECK(!strcmp(last_cmd, "G28 Z") && homed == 7, "home X, Y y Z");
  tap(NAME_X, row_y(3)); dump("asistente_5_offsets"); back();
  dump("asistente_5_home_ok");
  next(); run();
  // ---- Paso 6: mecanica ----
  const char *mech[] = { "pasos", "velocidad", "aceleracion_max", "aceleraciones", "corrientes", "shaping" };
  for (int i = 0; i < 6; ++i) { char n[48]; snprintf(n, sizeof n, "asistente_6_%s", mech[i]); dump(n); if (i == 4) { tap(BOX1_X, row_y(3)); type_num("1200"); dump("asistente_6_corriente_1200"); dlg_ok1(); key(2, 3); run(); } next(); }
  // ---- Paso 7: revision ----
  for (int i = 0; i < 9; ++i) { char n[48]; snprintf(n, sizeof n, "asistente_7_revision_%d", i + 1); dump(n); if (i < 8) next(); }
  // ---- Paso 8: guardar ----
  next(); dump("asistente_8_guardar"); dlg_ok(); run(); dump("asistente_8_guardado");
  CHECK(commits == 1 && rec_valid && cur.profile == ARQ_MC_AQUILA_D1, "se guarda el perfil Aquila D1");
  CHECK((cur.flags & 0x0F) == 0x0F, "flags: asistente, finales, direcciones y extrusor comprobados");
  CHECK(cur.v[ARQ_MP_BED_X] == 230 && cur.v[ARQ_MP_Z_MAX] == 240, "medidas del Aquila aplicadas");
  CHECK(fabsf(planner.settings.axis_steps_per_mm[2] - 803.41f) < 1e-3, "pasos Z del Aquila conservados");
  CHECK(spi_saves == 1 && gCfgItems.trammingPos[2].x == 200 && gCfgItems.trammingPos[2].y == 210 && gCfgItems.trammingPos[4].x == 115, "tornillos de nivelacion recalculados para 230 x 240");
  dlg_ok1(); run();
  CHECK(ready_draws == 1 && !arq_mc_needs_wizard(), "termina en la pantalla de inicio y el asistente no vuelve a hacer falta");

  // ---- Ajustes > Maquina ----
  arq_machine_menu_draw(); run(); dump("menu_1");
  next(); dump("menu_2"); next(); dump("menu_3"); next(); dump("menu_4"); next();
  tap(NAME_X, row_y(1)); dump("menu_dimensiones");           // Dimensiones
  tap(BOX1_X, row_y(2)); type_num("200"); run();            // Altura Z 200
  back(); run(); dump("menu_cambios_sin_guardar");
  back(); dump("menu_salir_con_cambios"); dlg_ok(); run(20);   // Guardar
  CHECK(commits == 2 && cur.v[ARQ_MP_Z_MAX] == 200, "menu: Altura Z 200 guardada");
  arq_machine_menu_draw(); run();
  tap(NAME_X, row_y(2)); run();                             // Finales: cambiar la logica de X sin comprobar
  tap(BOX1_X, row_y(0)); run(); dump("menu_final_x_cambiado");
  back(); back(); dlg_ok(); run(20);
  CHECK(!(cur.flags & ARQ_MCF_ES_OK) && arq_mc_block_reason() == ARQ_BLK_UNVERIFIED, "logica cambiada sin comprobar: movimiento bloqueado");
  arq_machine_menu_draw(); run(); tap(BOX1_X, row_y(0)); dump("menu_cambiar_modelo"); dlg_no();
  next(); next(); tap(NAME_X, row_y(1)); dump("menu_restaurar_mecanica"); dlg_no();
  tap(NAME_X, row_y(2)); dump("menu_restaurar_todo"); dlg_no();
  back(); back(); back(); run();
  CHECK(set_draws >= 1, "Volver sale a Ajustes");

  // ---- Memoria (heap de LVGL de 16 KB) ----
  printf("[maquina] heap LVGL: base %u B, pico %u B (compilador del PC, 64 bits: en la placa es menor)\n", (unsigned)heap0, (unsigned)peak_used);
  CHECK(peak_used < 16384, "el pico cabe en el heap de 16 KB (incluso en 64 bits)");
  printf("[maquina] %d comprobaciones, %d fallos, %d capturas\n", checks, fails, shots);
  return fails ? 1 : 0;
}
