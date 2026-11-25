// Simulador PC del asistente del primer arranque y de Ajustes > Maquina (firmware unico XQ1 / Aquila D1).
// Compila el codigo REAL (arquis_machine_ui.cpp, arquis_machine_logic.cpp, arquis_params_view.cpp, widgets, tema) sobre
// LVGL 6.1.1-MKS REAL, con el pegamento REAL (arquis_machine.cpp). No ejecuta Marlin: una "maquina" simulada
// (../stub_machine/arq_fake_hw.*) responde a lo que pide el pegamento: finales (pines), G-code, home, temperatura,
// EEPROM, drivers TMC2209. Es una SIMULACION: no sustituye a las pruebas fisicas. Uso: build_machine_sim.sh
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <string>
#include "arquis_machine_ui.h"
#include "arquis_params.h"
#include "arquis_widgets.h"
#include "draw_ui.h"
#include "../../../module/planner.h"
#include "../../../module/temperature.h"
#include "../../../module/settings.h"
#include "../../../module/motion.h"
#include "../../../gcode/queue.h"
#include "arq_fake_hw.h"
#include "../../../feature/bedlevel/bedlevel.h"

// El LVGL de MKS usa lv_font_roboto_22 como fuente por defecto (LV_FONT_DEFAULT); en la placa la da el fabricante.
// En el PC se usa la fuente propia (igual que arquis_sim.cpp, mesh_sim.cpp y perf_sim.cpp).
extern "C" { extern lv_font_t arq_font_text16; lv_font_t lv_font_roboto_22 = arq_font_text16; }
extern "C" { uint32_t lv_open_gcode_file(char *) { return 0; } void lv_gcode_file_read(uint8_t *) {} void lv_close_gcode_file() {} void lv_log_register_print_cb(void *) {} }

// ---- Maquina simulada (herramientas/arquis_ui/stub_machine/arq_fake_hw.*) ------------------------------------------
// El pegamento es el REAL (arquis_machine.cpp): EEPROM, registro, bit shaft de los TMC2209, bloqueo del G-code.
static bool pin_raw[3];                      // nivel electrico de los finales X, Y, Z
bool sim_read_pin(int p) { return p >= 0 && p < 3 ? pin_raw[p] : false; }
Temperature thermalManager; static int16_t hot = 25, hot_tgt = 0;
celsius_t Temperature::wholeDegHotend(const uint8_t) { return hot; }
celsius_t Temperature::degTargetHotend(const uint8_t) { return hot_tgt; }
void Temperature::setTargetHotend(const celsius_t c, const uint8_t) { hot_tgt = c; }
DISP_STATE_STACK disp_state_stack; GCFG gCfgItems; lv_group_t *g;
static int ready_draws, set_draws, para_draws, spi_saves;
// Como en MKS: quien sale borra su pantalla (lv_clear_*) y quien entra crea la suya; aqui no se borra la anterior.
// Las pantallas que crea el propio simulador (Ajustes, Inicio, MKS) se borran al crear la siguiente, como haria su lv_clear_*.
static lv_obj_t * new_screen() { lv_obj_t *s = lv_obj_create(nullptr, nullptr); lv_scr_load(s); return s; }
static lv_obj_t *sim_scr;
static void sim_screen() { lv_obj_t *old = sim_scr; sim_scr = new_screen(); if (old) lv_obj_del(old); }
lv_obj_t* lv_screen_create(DISP_STATE st, const char *) { if (disp_state_stack._disp_index < 90) ++disp_state_stack._disp_index; disp_state_stack._disp_state[disp_state_stack._disp_index] = st; return new_screen(); }
void lv_draw_set() { ++set_draws; sim_screen(); }
void lv_draw_ready_print() { ++ready_draws; sim_screen(); }
// Otros parametros (MKS) simulado: como draw_number_key.cpp, cambia un valor de Marlin y hace M500 en el acto
static float mks_steps_x; static bool mks_m500 = true;
void lv_draw_machine_para() {
  ++para_draws; sim_screen();
  if (mks_steps_x > 0) { planner.settings.axis_steps_per_mm[0] = mks_steps_x; if (mks_m500) settings.save(); }
}
static void mks_back() { arq_machine_ui_draw(); }             // lv_clear_machine_para() + draw_return_ui() -> ARQ_MACHINE_UI
void update_spi_flash() { ++spi_saves; }
bool arq_numpad_value_ok(const char *s) { if (!s) return false; for (; *s; ++s) if (*s >= '0' && *s <= '9') return true; return false; }   // = arquis_params.cpp

// Placa con la EEPROM de Marlin de 'prof' (opcionalmente con pasos E distintos) y, si 'rec', registro ARQUIS de ese modelo
static void machine_from(uint8_t prof, bool rec, float steps_e = 0, float steps_z = 0) {
  fake::reset_all();
  arq_mc_cfg_t c; arq_mc_factory(prof, &c);
  if (steps_e > 0) c.v[ARQ_MP_STEPS_E] = steps_e;
  if (steps_z > 0) c.v[ARQ_MP_STEPS_Z] = steps_z;
  fake::marlin_ee.valid = true; memcpy(fake::marlin_ee.v, c.v, sizeof(c.v));
  fake::boot();
  if (rec) { c.flags = 0x0F; arq_mc_commit(&c, nullptr); fake::boot(); }
  const arq_mc_profile_t *p = arq_mc_profile(prof);
  for (int a = 0; a < 3; ++a) pin_raw[a] = (p->es_inv >> a) & 1;   // finales en reposo ("Abierto") con su logica
  disp_state_stack._disp_index = 0;
}
static bool mech_is(uint8_t prof) {                           // la mecanica EN USO es la de fabrica de 'prof'
  arq_mc_cfg_t c; arq_mc_read_current(&c); const arq_mc_profile_t *p = arq_mc_profile(prof);
  for (int i = ARQ_MP_STEPS_X; i <= ARQ_MP_SHP_FY; ++i) if (fabsf(c.v[i] - p->v[i]) > 1e-3f) return false;
  return true;
}

// ---- LVGL, capturas y toques --------------------------------------------------------------------------------------------
static uint16_t fb[ARQ_W * ARQ_H]; static lv_disp_buf_t disp_buf; static lv_color_t buf1[ARQ_W * 14]; static const char *outdir = ".";
static void flush_cb(lv_disp_drv_t *drv, const lv_area_t *a, lv_color_t *c) { for (int y = a->y1; y <= a->y2; ++y) for (int x = a->x1; x <= a->x2; ++x) fb[y * ARQ_W + x] = (c++)->full; lv_disp_flush_ready(drv); }
static uint32_t used() { lv_mem_monitor_t m; lv_mem_monitor(&m); return m.total_size - m.free_size; }
static uint32_t peak_used;
static void run(int n = 12) { for (int i = 0; i < n; ++i) { fake::ms += 10; lv_tick_inc(10); lv_task_handler(); arq_machine_tick(); if (used() > peak_used) peak_used = used(); } }
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

// Texto visible en pantalla: se captura lo que LVGL DIBUJA (las tarjetas y dialogos ARQUIS pintan su texto con
// lv_draw_label desde su propia funcion de dibujo). Se enlaza con -Wl,--wrap=lv_draw_label (build_machine_sim.sh).
static std::string drawn;
extern "C" void __real_lv_draw_label(const lv_area_t *, const lv_area_t *, const lv_style_t *, lv_opa_t, const char *, lv_txt_flag_t,
                                     lv_point_t *, lv_draw_label_txt_sel_t *, lv_draw_label_hint_t *, lv_bidi_dir_t);
extern "C" void __wrap_lv_draw_label(const lv_area_t *c, const lv_area_t *m, const lv_style_t *st, lv_opa_t o, const char *t, lv_txt_flag_t f,
                                     lv_point_t *off, lv_draw_label_txt_sel_t *sel, lv_draw_label_hint_t *h, lv_bidi_dir_t b) {
  if (t) { drawn += t; drawn += '\n'; }
  __real_lv_draw_label(c, m, st, o, t, f, off, sel, h, b);
}
static bool shows(const char *needle) {
  drawn.clear(); lv_obj_invalidate(lv_scr_act()); lv_obj_invalidate(lv_layer_top()); run(3);
  return drawn.find(needle) != std::string::npos;
}

// Pasos 2 a 8 del asistente con el modelo ya elegido en el paso 1: comprueba finales, sentidos y home y guarda
static void wizard_rest() {
  next(); run();                                            // 1 -> 2
  next(); run(150);                                         // 2 -> 3 (finales en reposo >= 1 s)
  for (int a = 0; a < 3; ++a) press_es(a);
  run(20); next(); run();                                   // 3 -> 4
  for (int a = 0; a < 3; ++a) { tap(BOX2_X, row_y(a)); dlg_ok(); run(80); dlg_ok(); }
  next(); run(); dlg_ok(); run();                           // 4 -> 5: extrusor sin probar -> Continuar
  for (int a = 0; a < 3; ++a) { tap(BOX1_X, row_y(a)); dlg_ok(); run(80); dlg_ok(); }
  next(); run();                                            // 5 -> 6
  for (int i = 0; i < 6; ++i) next();                       // 6 -> 7
  for (int i = 0; i < 8; ++i) next();
  next(); dlg_ok(); run(); dlg_ok1(); run();                // 8: Guardar -> configuracion guardada -> Inicio
}

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
  machine_from(ARQ_MC_AQUILA_D1, false);
  CHECK(arq_mc_needs_wizard() && fake::gcode("G1 X1"), "sin registro: asistente obligatorio y G1 rechazado por el pegamento real");

  // ---- Paso 1: modelo ----
  fake::marlin_ee.valid = false;                            // EEPROM de Marlin no valida: no se detecta ni se preselecciona
  arq_machine_wizard_draw(); run(); dump("asistente_1_modelo_sin_eeprom");
  next(); CHECK(arq_dialog_is_open(), "Siguiente sin modelo elegido: aviso"); dlg_ok1();
  fake::marlin_ee.valid = true;                             // EEPROM valida con los pasos del Aquila: se detecta y se preselecciona
  arq_machine_wizard_draw(); run(); dump("asistente_1_modelo_detectado");
  tap(BOX1_X, row_y(0)); dump("asistente_1_xq1_elegida_aviso");   // elegir XQ1 sobre una EEPROM del Aquila: aviso en el chip
  CHECK(shows("otra m\xC3\xA1quina"), "P1: XQ1 sobre una EEPROM del Aquila: aviso de ajustes de otra maquina");
  tap(BOX1_X, row_y(2)); run();                            // intentar "Conservar"
  CHECK(arq_dialog_is_open() && shows("No se puede conservar"), "P1: Conservar con datos de otra maquina: rechazado con explicacion");
  dlg_ok1(); run();
  CHECK(shows("De f\xC3\xA1" "brica"), "P1: queda De fabrica");
  tap(BOX1_X, row_y(1)); dump("asistente_1_aquila_elegida");
  CHECK(shows("Conservar"), "P1: Aquila detectado = elegido: se puede conservar (por defecto)");
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
  CHECK(!strcmp(fake::last_inject, "G91\nG1 X10 F600\nG90") && fake::gcode_rejected == 1, "prueba de X: G91 / G1 X10 F600 / G90 aceptada por el pegamento (%s)", fake::last_inject);
  CHECK(stepperX.chip & ARQ_GCONF_SHAFT, "prueba de X: el driver X ya tiene el bit shaft del Aquila");
  dump("asistente_4_pregunta_x"); dlg_ok();                // si
  tap(BOX2_X, row_y(1)); dlg_ok(); run(80); dlg_no();      // Y: no -> se invierte
  dump("asistente_4_y_invertida"); dlg_ok1();
  tap(BOX2_X, row_y(1)); dlg_ok(); run(80);
  CHECK(!(stepperY.chip & ARQ_GCONF_SHAFT), "Y invertido tras responder No: la nueva prueba usa el otro sentido (shaft 0)");
  dlg_ok();                                                 // Y otra vez: si
  tap(BOX2_X, row_y(2)); dlg_ok(); run(80);
  CHECK(!strcmp(fake::last_inject, "G91\nG1 Z5 F300\nG90"), "prueba de Z: 5 mm a 5 mm/s");
  dlg_ok();
  tap(BOX2_X, row_y(3)); dump("asistente_4_calentar_e"); dlg_ok(); run();
  CHECK(hot_tgt == 200, "extrusor: calienta a 200");
  hot = 199; run(40); dump("asistente_4_e_caliente");
  tap(BOX2_X, row_y(3)); dlg_ok(); run(80);
  CHECK(!strcmp(fake::last_inject, "M83\nG1 E10 F120\nM82"), "prueba de E: 10 mm relativos");
  dlg_ok(); run();
  CHECK(hot_tgt == 0, "tras probar E se apaga el hotend");
  dump("asistente_4_direcciones_ok");
  next(); run();
  // ---- Paso 5: home ----
  dump("asistente_5_home");
  next(); CHECK(arq_dialog_is_open(), "Siguiente sin home: aviso"); dlg_ok1();
  for (int a = 0; a < 3; ++a) { tap(BOX1_X, row_y(a)); dlg_ok(); run(80); dlg_ok(); }
  CHECK(!strcmp(fake::last_inject, "G28 Z") && fake::homed == 7, "home X, Y y Z");
  tap(NAME_X, row_y(3)); dump("asistente_5_offsets");
  CHECK(shows("Babystep") && !shows("Ajuste Z"), "P2: el ajuste fino se llama Babystep (no 'Ajuste Z')");
  back();
  dump("asistente_5_home_ok");
  next(); run();
  // ---- Paso 6: mecanica ----
  const char *mech[] = { "pasos", "velocidad", "aceleracion_max", "aceleraciones", "corrientes", "shaping" };
  for (int i = 0; i < 6; ++i) { char n[48]; snprintf(n, sizeof n, "asistente_6_%s", mech[i]); dump(n); if (i == 4) { tap(BOX1_X, row_y(3)); type_num("1200"); dump("asistente_6_corriente_1200"); dlg_ok1(); key(2, 3); run(); } next(); }
  // ---- Paso 7: revision ----
  for (int i = 0; i < 9; ++i) { char n[48]; snprintf(n, sizeof n, "asistente_7_revision_%d", i + 1); dump(n); if (i < 8) next(); }
  // ---- Paso 8: guardar ----
  next(); dump("asistente_8_guardar"); dlg_ok(); run(); dump("asistente_8_guardado");
  CHECK(arq_mc_profile_id() == ARQ_MC_AQUILA_D1 && !arq_mc_needs_wizard(), "se guarda el perfil Aquila D1 (registro real en la EEPROM simulada)");
  CHECK((arq_mc_flags() & 0x0F) == 0x0F, "flags: asistente, finales, direcciones y extrusor comprobados");
  CHECK(arq_mc_max_pos(0) == 230 && arq_mc_max_pos(2) == 240, "medidas del Aquila aplicadas");
  CHECK(fabsf(planner.settings.axis_steps_per_mm[2] - 803.41f) < 1e-3, "pasos Z del Aquila conservados");
  CHECK(spi_saves == 1 && gCfgItems.trammingPos[2].x == 200 && gCfgItems.trammingPos[2].y == 210 && gCfgItems.trammingPos[4].x == 115, "tornillos de nivelacion recalculados para 230 x 240");
  dlg_ok1(); run();
  CHECK(ready_draws == 1 && !arq_mc_needs_wizard(), "termina en la pantalla de inicio y el asistente no vuelve a hacer falta");
  fake::boot();
  CHECK(!arq_mc_needs_wizard() && arq_mc_profile_id() == ARQ_MC_AQUILA_D1 && arq_mc_motion_ok(), "tras reiniciar: Aquila, sin asistente y se puede mover");

  // ---- Ajustes > Maquina ----
  arq_machine_menu_draw(); run(); dump("menu_1");
  next(); dump("menu_2"); next(); dump("menu_3"); next(); dump("menu_4"); next();
  tap(NAME_X, row_y(1)); dump("menu_dimensiones");           // Dimensiones
  tap(BOX1_X, row_y(2)); type_num("200"); run();            // Altura Z 200
  back(); run(); dump("menu_cambios_sin_guardar");
  back(); dump("menu_salir_con_cambios"); dlg_ok(); run(20);   // Guardar
  CHECK(arq_mc_max_pos(2) == 200 && !arq_mc_ram_unsaved(), "menu: Altura Z 200 guardada");
  arq_machine_menu_draw(); run();
  tap(NAME_X, row_y(2)); run();                             // Finales: cambiar la logica de X sin comprobar
  tap(BOX1_X, row_y(0)); run(); dump("menu_final_x_cambiado");
  back(); back(); dlg_ok(); run(20);
  CHECK(!(arq_mc_flags() & ARQ_MCF_ES_OK) && arq_mc_block_reason() == ARQ_BLK_UNVERIFIED, "logica cambiada sin comprobar: movimiento bloqueado");
  arq_machine_menu_draw(); run(); tap(BOX1_X, row_y(0)); dump("menu_cambiar_modelo");
  CHECK(shows("no se mezclan"), "P1: el aviso de cambiar de modelo dice que no se mezcla la mecanica");
  dlg_no();
  next(); next(); tap(NAME_X, row_y(1)); dump("menu_restaurar_mecanica");
  CHECK(shows("offsets M206") && !shows("ajuste Z"), "P2: Restaurar mecanica habla de offsets M206 (no de 'ajuste Z')");
  dlg_no();
  tap(NAME_X, row_y(2)); dump("menu_restaurar_todo");
  CHECK(shows("Se pierden malla, PID, offsets M206"), "P2: Restaurar todo dice lo que se pierde");
  dlg_no();
  back(); back(); back(); run();
  CHECK(set_draws >= 1, "Volver sale a Ajustes");

  // ===== P1: cambio de modelo desde el menu, sin mezclar la mecanica =====
  struct P1Case { uint8_t from, to; float e, z; const char *what; } p1[] = {
    { ARQ_MC_XQ1, ARQ_MC_AQUILA_D1, 760.5f, 0, "XQ1 (E calibrado) -> Aquila" },
    { ARQ_MC_AQUILA_D1, ARQ_MC_XQ1, 0, 810, "Aquila (Z modificado) -> XQ1" },
    { ARQ_MC_XQ1, ARQ_MC_AQUILA_D1, 0, 0, "XQ1 (de fabrica, detectado) -> Aquila" },
  };
  for (const P1Case &k : p1) {
    machine_from(k.from, true, k.e, k.z);
    home_offset.z = -1.5f; settings.save();                 // M206 Z del usuario en la maquina anterior
    arq_machine_menu_draw(); run(); tap(BOX1_X, row_y(0)); dlg_ok(); run();   // Modelo -> Cambiar
    tap(BOX1_X, row_y(k.to - 1)); run();
    CHECK(shows("De f\xC3\xA1" "brica"), "P1 %s: al elegir el otro modelo queda De fabrica", k.what);
    tap(BOX1_X, row_y(2)); run();                           // el usuario intenta Conservar
    CHECK(arq_dialog_is_open() && shows("No se puede conservar"), "P1 %s: Conservar rechazado", k.what);
    dlg_ok1(); run();
    for (int a = 0; a < 3; ++a) pin_raw[a] = (arq_mc_profile(k.to)->es_inv >> a) & 1;
    wizard_rest();
    CHECK(arq_mc_profile_id() == k.to && !arq_mc_needs_wizard(), "P1 %s: guardado con el modelo nuevo", k.what);
    CHECK(mech_is(k.to), "P1 %s: TODA la mecanica (pasos, velocidades, aceleraciones, corrientes, Shaping) es la del modelo nuevo", k.what);
    CHECK(arq_mc_max_pos(0) == arq_mc_profile(k.to)->v[ARQ_MP_BED_X] && arq_mc_max_pos(2) == arq_mc_profile(k.to)->v[ARQ_MP_Z_MAX], "P1 %s: medidas del modelo nuevo", k.what);
    CHECK(home_offset.z == 0, "P1 %s: el M206 de la maquina anterior no se arrastra", k.what);
    fake::boot();
    CHECK(mech_is(k.to) && arq_mc_profile_id() == k.to, "P1 %s: tras reiniciar sigue igual", k.what);
  }
  // Mismo modelo ("Repetir el asistente") con E calibrado: SI se puede conservar
  machine_from(ARQ_MC_XQ1, true, 752.3f);
  home_offset.z = -1.5f; settings.save();
  arq_machine_menu_draw(); run(); next(); next(); tap(NAME_X, row_y(3)); run();   // pagina 3: Repetir el asistente
  CHECK(shows("Conservar"), "P1 mismo modelo confirmado por el registro: Conservar por defecto");
  for (int a = 0; a < 3; ++a) pin_raw[a] = 1;
  wizard_rest();
  CHECK(fabsf(planner.settings.axis_steps_per_mm[3] - 752.3f) < 1e-3 && home_offset.z == -1.5f, "P1 mismo modelo: se conservan el E calibrado y el M206");
  // Primera instalacion con E calibrado (no se puede confirmar el modelo): fabrica aunque se pida Conservar
  machine_from(ARQ_MC_AQUILA_D1, false, 777);
  arq_machine_wizard_draw(); run(); tap(BOX1_X, row_y(1)); run();
  CHECK(shows("sin confirmar"), "P1 primera instalacion, pasos calibrados: 'sin confirmar' -> De fabrica");
  tap(BOX1_X, row_y(2)); run(); CHECK(arq_dialog_is_open(), "P1 primera instalacion: Conservar rechazado"); dlg_ok1(); run();
  wizard_rest();
  CHECK(mech_is(ARQ_MC_AQUILA_D1), "P1 primera instalacion sin confirmar: mecanica de fabrica del Aquila");

  // ===== P2: aviso antes de borrar una malla de otras medidas =====
  machine_from(ARQ_MC_AQUILA_D1, true);
  fake::mesh_valid = true; set_bed_leveling_enabled(true); settings.save();
  arq_machine_menu_draw(); run(); tap(NAME_X, row_y(1)); run();
  tap(BOX1_X, row_y(0)); type_num("220"); run();            // ancho 220
  back(); run(); back(); run();                             // salir: Guardar / Descartar
  CHECK(shows("Se borrar\xC3\xA1 la malla"), "P2: al salir con otro ancho, el dialogo avisa de que se borrara la malla");
  dlg_no(); run(20);                                        // Descartar
  CHECK(fake::mesh_valid && arq_mc_max_pos(0) == 230, "P2: Descartar: la malla y el ancho siguen");
  arq_machine_menu_draw(); run(); tap(NAME_X, row_y(1)); run();
  tap(BOX1_X, row_y(0)); type_num("220"); run(); back(); run();
  next(); next(); next(); tap(NAME_X, row_y(1)); run();     // pagina 4: Guardar cambios
  CHECK(arq_dialog_is_open() && shows("Se borrar\xC3\xA1 la malla"), "P2: Guardar cambios pide confirmacion antes de borrar la malla");
  dlg_no(); run();
  CHECK(fake::mesh_valid, "P2: Cancelar: la malla sigue");
  tap(NAME_X, row_y(1)); run(); dlg_ok(); run();
  CHECK(!fake::mesh_valid && arq_mc_max_pos(0) == 220, "P2: confirmado: se guarda el ancho 220 y se borra la malla");
  dlg_ok1(); back(); run();
  fake::mesh_valid = true; set_bed_leveling_enabled(true); settings.save();   // malla nueva para 220
  arq_machine_menu_draw(); run(); next(); next(); tap(NAME_X, row_y(1)); run();   // Restaurar mecanica (230 de fabrica)
  CHECK(shows("Se borrar\xC3\xA1 la malla"), "P2: Restaurar mecanica con otras medidas avisa de que se borra la malla");
  dlg_ok(); run();
  CHECK(!fake::mesh_valid && shows("la malla se borr"), "P2: y el resultado lo dice (no promete conservarla)");
  dlg_ok1(); run();
  fake::mesh_valid = true; set_bed_leveling_enabled(true); settings.save();
  tap(NAME_X, row_y(1)); run();
  CHECK(shows("Se conserva la malla"), "P2: con las mismas medidas, Restaurar mecanica conserva la malla");
  dlg_ok(); run(); CHECK(fake::mesh_valid, "P2: y la conserva"); dlg_ok1(); back(); run();

  // ===== P3: Ajustes > Maquina <-> Otros parametros (MKS) =====
  machine_from(ARQ_MC_AQUILA_D1, true);
  // (1) cambio sin guardar + Descartar -> MKS cambia pasos X y guarda -> volver: se ve lo de MKS, sin cambios pendientes
  arq_machine_menu_draw(); run(); tap(NAME_X, row_y(1)); run(); tap(BOX1_X, row_y(2)); type_num("200"); run(); back(); run();
  next(); next(); next(); mks_steps_x = 161; tap(NAME_X, row_y(0)); run();
  CHECK(arq_dialog_is_open() && shows("antes de abrir Otros"), "P3: con cambios sin guardar, Otros parametros pide Guardar o Descartar");
  const int pd0 = para_draws; dlg_no(); run();
  CHECK(para_draws == pd0 + 1 && arq_mc_max_pos(2) == 240, "P3 (1): Descartar abre MKS y no aplica la altura 200");
  mks_back(); run();
  CHECK(!shows("Guardar cambios"), "P3 (1): al volver no hay cambios pendientes (copia de trabajo releida)");
  back(); run();
  CHECK(fabsf(planner.settings.axis_steps_per_mm[0] - 161) < 1e-3 && fabsf(fake::marlin_ee.v[ARQ_MP_STEPS_X] - 161) < 1e-3, "P3 (1): el cambio hecho en MKS se conserva (RAM y EEPROM)");
  // (2) cambio + Guardar -> MKS cambia pasos X -> volver -> otro cambio + Guardar: no pisa lo de MKS
  arq_machine_menu_draw(); run(); tap(NAME_X, row_y(1)); run(); tap(BOX1_X, row_y(2)); type_num("210"); run(); back(); run();
  next(); next(); next(); mks_steps_x = 162; tap(NAME_X, row_y(0)); run(); dlg_ok(); run();
  CHECK(arq_mc_max_pos(2) == 210, "P3 (2): Guardar antes de abrir MKS guarda la altura 210");
  mks_back(); run();
  CHECK(shows("Otros par\xC3\xA1metros"), "P3 (2): al volver de MKS, Maquina sigue en la pagina donde estaba");
  next(); run();                                            // pagina 4 -> 1
  tap(NAME_X, row_y(1)); run(); tap(BOX1_X, row_y(2)); type_num("220"); run(); back(); run(); back(); run(); dlg_ok(); run(20);
  CHECK(arq_mc_max_pos(2) == 220 && fabsf(planner.settings.axis_steps_per_mm[0] - 162) < 1e-3 && fabsf(fake::marlin_ee.v[ARQ_MP_STEPS_X] - 162) < 1e-3,
        "P3 (2): guardar despues de volver de MKS no sobrescribe los pasos cambiados en MKS");
  // (3) prueba de sentido aplicada en RAM (sin guardar) -> Otros parametros -> Descartar: el driver vuelve a lo guardado
  arq_machine_menu_draw(); run(); tap(NAME_X, row_y(3)); run();
  tap(BOX1_X, row_y(0)); run();                             // invertir X en la copia de trabajo
  tap(BOX2_X, row_y(0)); dlg_ok(); run(80); dlg_ok(); run();   // probarlo (se aplica en RAM: shaft de X = 0)
  CHECK(!(stepperX.chip & ARQ_GCONF_SHAFT) && arq_mc_ram_unsaved(), "P3 (3): la prueba aplica el sentido nuevo solo en RAM");
  CHECK(!arq_mc_motion_ok(), "P3 (3): fuera de la prueba, con cambios en RAM, no se mueve");
  back(); run(); next(); next(); next(); mks_steps_x = 0; tap(NAME_X, row_y(0)); run(); dlg_no(); run();
  CHECK((stepperX.chip & ARQ_GCONF_SHAFT) && !arq_mc_ram_unsaved() && arq_mc_motion_ok(), "P3 (3): Descartar devuelve el driver X al sentido guardado");
  mks_back(); run(); back(); run();
  // (4) MKS cambia un valor SIN M500 -> volver -> salir: no se hace M501 (comportamiento original de Marlin)
  arq_machine_menu_draw(); run(); next(); next(); next(); mks_steps_x = 163; mks_m500 = false;
  const int ld = fake::loads; tap(NAME_X, row_y(0)); run(); mks_back(); run(); back(); run();
  CHECK(fabsf(planner.settings.axis_steps_per_mm[0] - 163) < 1e-3 && fake::loads == ld && fabsf(fake::marlin_ee.v[ARQ_MP_STEPS_X] - 162) < 1e-3,
        "P3 (4): un cambio de MKS sin M500 sigue en RAM; salir de Maquina no hace M501 ni lo borra");
  mks_m500 = true; mks_steps_x = 0;
  // (5) salir de la pantalla por otro camino (p. ej. empieza una impresion) con una prueba aplicada: se deshace
  arq_machine_menu_draw(); run(); tap(NAME_X, row_y(3)); run(); tap(BOX1_X, row_y(0)); run();
  tap(BOX2_X, row_y(0)); dlg_ok(); run(80); dlg_ok(); run();
  CHECK(arq_mc_ram_unsaved(), "P3 (5): prueba aplicada");
  arq_machine_ui_clear();                                   // clear_cur_ui() desde fuera
  CHECK(!arq_mc_ram_unsaved() && (stepperX.chip & ARQ_GCONF_SHAFT), "P3 (5): al salir de la pantalla por fuera, la prueba se deshace");

  // ---- Memoria (heap de LVGL de 16 KB) ----
  printf("[maquina] heap LVGL: base %u B, pico %u B (compilador del PC, 64 bits: en la placa es menor)\n", (unsigned)heap0, (unsigned)peak_used);
  CHECK(peak_used < 16384, "el pico cabe en el heap de 16 KB (incluso en 64 bits)");
  printf("[maquina] %d comprobaciones, %d fallos, %d capturas\n", checks, fails, shots);
  return fails ? 1 : 0;
}
