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

#include "../../../inc/MarlinConfigPre.h"

#if HAS_TFT_LVGL_UI && ENABLED(ARQUIS_UI)

#include "arquis_tools.h"
#include "arquis_ctrl_view.h"
#include "arquis_tools_view.h"
#include "arquis_mesh_ui.h"
#include "arquis_pid_ui.h"
#include "arquis_perf_ui.h"
#include "draw_ui.h"

#include "../../../inc/MarlinConfig.h"
#include "../../../inc/Version.h"
#include "../../../MarlinCore.h"
#include "../../../module/temperature.h"
#include "../../../module/motion.h"
#include "../../../module/planner.h"
#include "../../../module/printcounter.h"
#include "../../../gcode/queue.h"
#include "../../../gcode/gcode.h"
#include "../../../sd/cardreader.h"
#if ENABLED(EEPROM_SETTINGS)
  #include "../../../module/settings.h"
#endif
#if HAS_BED_PROBE
  #include "../../../module/probe.h"
#endif

#include <stdio.h>
#include <string.h>

extern lv_group_t *g;
extern const char G28_STR[];
extern char public_buf_m[100];

static void add_to_group(lv_obj_t *scr) {
  #if HAS_ROTARY_ENCODER
    if (gCfgItems.encoder_enable) arq_group_add_clickables(g, scr);
  #endif
}
static void remove_group() {
  #if HAS_ROTARY_ENCODER
    if (gCfgItems.encoder_enable) lv_group_remove_all_objs(g);
  #endif
}

// Rejilla de accesos generica (Herramientas, Home, Ajustes)
struct Grid {
  lv_obj_t *scr = nullptr;
  uint8_t   act[ARQ_OPT_MAX];
  uint8_t   n = 0;
  arq_opt_item_t it[ARQ_OPT_MAX];
  void begin() { n = 0; }
  void add(uint8_t a, const lv_img_dsc_t *icon, const char *title, const char *sub = nullptr, uint8_t kind = ARQ_BTN_SECONDARY) {
    if (n >= ARQ_OPT_MAX) return;
    act[n] = a; it[n] = { icon, title, sub, kind }; ++n;
  }
  void show(DISP_STATE st, const char *title, lv_event_cb_t cb) {
    scr = lv_screen_create(st, "");
    arq_screen_apply(scr);
    arq_opts_build(scr, cb, title, it, n);
    add_to_group(scr);
  }
  void clear() {
    remove_group();
    arq_dialog_close();
    arq_opts_forget();
    if (scr) lv_obj_del(scr);
    scr = nullptr;
  }
  int action(lv_obj_t *obj) const { const int i = obj->mks_obj_id - ARQ_XID_ITEM0; return (i >= 0 && i < n) ? act[i] : -1; }
};

// ==== HERRAMIENTAS (draw_tool.cpp) ===============================================================================================
static Grid grid_tools;
enum { T_TEMP, T_EXTRUDE, T_MOVE, T_HOME, T_LEVEL_AUTO, T_LEVEL_MANUAL, T_FILAMENT, T_MORE };
enum { DLG_LVL_OK = 501, DLG_LVL_NO };

static void tools_dialog_cb(lv_obj_t *obj, lv_event_t event) {
  if (event != LV_EVENT_RELEASED) return;
  const int id = obj->mks_obj_id;
  arq_dialog_close();
  #if ENABLED(AUTO_BED_LEVELING_BILINEAR) && !ARQ_HAS_MESH    // con malla manual 5x5 no se inyecta nunca el G29 guardado (no hay sonda)
    if (id == DLG_LVL_OK) {             // misma secuencia que ID_T_LEVELING de draw_tool.cpp
      get_gcode_command(AUTO_LEVELING_COMMAND_ADDR, (uint8_t *)public_buf_m);
      public_buf_m[sizeof(public_buf_m) - 1] = 0;
      queue.inject(public_buf_m);
    }
  #else
    (void)id;
  #endif
}

static void tools_cb(lv_obj_t *obj, lv_event_t event) {
  if (event != LV_EVENT_RELEASED) return;
  if (arq_dialog_is_open()) return;
  if (obj->mks_obj_id == ARQ_XID_BACK) { arq_tools_clear(); lv_draw_ready_print(); return; }
  switch (grid_tools.action(obj)) {
    case T_TEMP:         arq_tools_clear(); lv_draw_preHeat(); break;
    case T_EXTRUDE:      arq_tools_clear(); lv_draw_extrusion(); break;
    case T_MOVE:         arq_tools_clear(); lv_draw_move_motor(); break;
    case T_HOME:         arq_tools_clear(); lv_draw_home(); break;
    #if ARQ_HAS_MESH
    case T_LEVEL_AUTO:   arq_tools_clear(); arq_mesh_draw(); break;     // asistente de malla manual 5x5 (arquis_mesh_ui.cpp)
    #else
    case T_LEVEL_AUTO:   // mueve la maquina de forma amplia (el comando guardado en la placa): se pide confirmacion
      arq_dialog_open("Nivelaci\xC3\xB3n autom\xC3\xA1tica", "El cabezal recorrer\xC3\xA1 la cama sondeando puntos. Retira cualquier objeto de la cama y comprueba que el hotend est\xC3\xA1 limpio.",
                      "Iniciar", "Volver", false, tools_dialog_cb, DLG_LVL_OK, DLG_LVL_NO);
      break;
    #endif
    case T_LEVEL_MANUAL: uiCfg.leveling_first_time = true; arq_tools_clear(); lv_draw_manualLevel(); break;
    case T_FILAMENT:
      uiCfg.hotendTargetTempBak = thermalManager.degTargetHotend(uiCfg.extruderIndex);
      arq_tools_clear(); lv_draw_filament_change();
      break;
    case T_MORE:         arq_tools_clear(); lv_draw_more(); break;
  }
}

void arq_tools_draw() {
  grid_tools.begin();
  grid_tools.add(T_TEMP, &arq_icon_temp, "Temperatura");
  grid_tools.add(T_EXTRUDE, &arq_icon_down, "Extruir");
  grid_tools.add(T_MOVE, &arq_icon_move, "Mover");
  grid_tools.add(T_HOME, &arq_icon_home, "Home");
  #if ENABLED(AUTO_BED_LEVELING_BILINEAR)
    #if ARQ_HAS_MESH
      grid_tools.add(T_LEVEL_AUTO, &arq_icon_level, "Malla 5x5");     // nivelacion manual con papel (sin sonda)
    #else
      grid_tools.add(T_LEVEL_AUTO, &arq_icon_level, "Nivel. auto");
    #endif
  #endif
  grid_tools.add(T_LEVEL_MANUAL, &arq_icon_level, "Nivelar cama");
  grid_tools.add(T_FILAMENT, &arq_icon_filament, "Filamento");
  grid_tools.add(T_MORE, &arq_icon_tools, "M\xC3\xA1s");
  grid_tools.show(TOOL_UI, "Herramientas", tools_cb);
}
void arq_tools_clear() { grid_tools.clear(); }

// ==== HOME (draw_home.cpp) =======================================================================================================
static Grid grid_home;
enum { H_ALL, H_X, H_Y, H_Z, H_OFF_ALL, H_OFF_XY };

static void home_cb(lv_obj_t *obj, lv_event_t event) {
  if (event != LV_EVENT_RELEASED) return;
  if (obj->mks_obj_id == ARQ_XID_BACK) { goto_previous_ui(); return; }
  switch (grid_home.action(obj)) {
    case H_ALL:     queue.inject_P(G28_STR); break;
    case H_X:       queue.inject(F("G28X")); break;
    case H_Y:       queue.inject(F("G28Y")); break;
    case H_Z:       queue.inject(F("G28Z")); break;
    case H_OFF_ALL: queue.inject(F("M84")); break;
    case H_OFF_XY:  queue.inject(F("M84XY")); break;
  }
}

void arq_home_draw() {
  grid_home.begin();
  grid_home.add(H_ALL, &arq_icon_home, "Home todo");
  grid_home.add(H_X, &arq_icon_right, "Home X");
  grid_home.add(H_Y, &arq_icon_up, "Home Y");
  grid_home.add(H_Z, &arq_icon_down, "Home Z");
  grid_home.add(H_OFF_ALL, &arq_icon_power, "Motores off");
  grid_home.add(H_OFF_XY, &arq_icon_power, "Motores XY");
  grid_home.show(ZERO_UI, "Home", home_cb);
}
void arq_home_clear() { grid_home.clear(); }

// ==== AJUSTES (draw_set.cpp) =====================================================================================================
static Grid grid_settings;
enum { S_PARA, S_EEPROM, S_FAN, S_SYSTEM, S_LANG, S_WIFI, S_POWER, S_PID, S_PERF };
enum { DLG_PWR_OK = 601, DLG_PWR_NO };

static void settings_dialog_cb(lv_obj_t *obj, lv_event_t event) {
  if (event != LV_EVENT_RELEASED) return;
  const int id = obj->mks_obj_id;
  arq_dialog_close();
  if (id == DLG_PWR_OK) TERN(HAS_SUICIDE, suicide(), queue.enqueue_now(F("M84")));      // misma llamada que ID_S_MOTOR_OFF de draw_set.cpp
}

static void settings_cb(lv_obj_t *obj, lv_event_t event) {
  if (event != LV_EVENT_RELEASED) return;
  if (arq_dialog_is_open()) return;
  if (obj->mks_obj_id == ARQ_XID_BACK) { arq_settings_clear(); lv_draw_ready_print(); return; }
  const int a = grid_settings.action(obj);
  if (a == S_POWER) {          // apaga la impresora (o los motores): se pide confirmacion
    arq_dialog_open(ENABLED(HAS_SUICIDE) ? "Apagar la impresora" : "Apagar motores",
                    ENABLED(HAS_SUICIDE) ? "Se cortar\xC3\xA1 la alimentaci\xC3\xB3n de la impresora." : "Se desactivar\xC3\xA1n los motores y se perder\xC3\xA1 la posici\xC3\xB3n de los ejes.",
                    ENABLED(HAS_SUICIDE) ? "Apagar" : "Apagar motores", "Volver", true, settings_dialog_cb, DLG_PWR_OK, DLG_PWR_NO);
    return;
  }
  if (a < 0) return;
  arq_settings_clear();
  switch (a) {
    case S_FAN:    lv_draw_fan(); break;
    case S_SYSTEM: lv_draw_about(); break;
    #if ARQ_HAS_PID
      case S_PID:  arq_pid_draw(); break;
    #endif
    #if ARQ_HAS_PERF
      case S_PERF: arq_perf_draw(); break;
    #endif
    case S_LANG:   lv_draw_language(); break;
    case S_PARA:   lv_draw_machine_para(); break;
    case S_EEPROM: lv_draw_eeprom_settings(); break;
    #if ENABLED(MKS_WIFI_MODULE)
      case S_WIFI:
        if (gCfgItems.wifi_mode_sel == STA_MODEL) {
          if (wifi_link_state == WIFI_CONNECTED) { last_disp_state = SET_UI; lv_draw_wifi(); }
          else if (uiCfg.command_send) {
            uint8_t cmd_wifi_list[] = { 0xA5, 0x07, 0x00, 0x00, 0xFC };
            raw_send_to_wifi(cmd_wifi_list, COUNT(cmd_wifi_list));
            last_disp_state = SET_UI;
            lv_draw_wifi_list();
          }
          else { last_disp_state = SET_UI; lv_draw_dialog(DIALOG_WIFI_ENABLE_TIPS); }
        }
        else { last_disp_state = SET_UI; lv_draw_wifi(); }
        break;
    #endif
  }
}

void arq_settings_draw() {
  grid_settings.begin();
  grid_settings.add(S_PARA, &arq_icon_gear, "Par\xC3\xA1metros");
  grid_settings.add(S_EEPROM, &arq_icon_file, "Memoria");
  grid_settings.add(S_FAN, &arq_icon_fan, "Ventiladores");
  grid_settings.add(S_SYSTEM, &arq_icon_tools, "Sistema");
  #if ARQ_HAS_PID
    grid_settings.add(S_PID, &arq_icon_temp, "PID");               // autoajuste de hotend y cama (M303)
  #endif
  #if ARQ_HAS_PERF
    grid_settings.add(S_PERF, &arq_icon_speed, "Rendimiento");     // aceleraciones, Linear Advance, Input Shaping
  #endif
  #if HAS_LANG_SELECT_SCREEN
    grid_settings.add(S_LANG, &arq_icon_file, "Idioma");
  #endif
  #if ENABLED(MKS_WIFI_MODULE)
    grid_settings.add(S_WIFI, &arq_icon_wifi, "Wi-Fi");
  #endif
  grid_settings.add(S_POWER, &arq_icon_power, ENABLED(HAS_SUICIDE) ? "Apagar" : "Motores off", nullptr, ARQ_BTN_SECONDARY);
  grid_settings.show(SET_UI, "Ajustes", settings_cb);
}
void arq_settings_clear() { grid_settings.clear(); }

// ==== SISTEMA Y DIAGNOSTICO (draw_about.cpp) =====================================================================================
static lv_obj_t *a_scr;
static lv_task_t *a_task;
static arq_info_list_t a_left, a_right;
static char a_up[24], a_cnt[20], a_tot[24];

static void fmt_hm(char *b, size_t n, uint32_t s) { snprintf(b, n, "%lu h %02lu min", (unsigned long)(s / 3600), (unsigned long)((s / 60) % 60)); }

static void about_fill() {
  arq_info_list_t &l = a_left, &r = a_right;
  uint8_t n = 0;
  auto add_l = [&](const char *label, const char *v) { l.rows[n].label = label; snprintf(l.rows[n].v1, sizeof(l.rows[n].v1), "%s", v); l.rows[n].v2[0] = 0; l.rows[n].dot = ARQ_TINT_AUTO; ++n; };
  l.title = "Sistema";
  add_l("M\xC3\xA1quina", MACHINE_NAME);
  add_l("Firmware", "Marlin " SHORT_BUILD_VERSION);
  add_l("Interfaz", "ARQUIS " ARQUIS_UI_VERSION);
  add_l("Placa", BOARD_INFO_NAME);
  add_l("Compilaci\xC3\xB3n", STRING_DISTRIBUTION_DATE);
  fmt_hm(a_up, sizeof(a_up), millis() / 1000UL);
  add_l("Encendida", a_up);
  #if ENABLED(PRINTCOUNTER)
    const printStatistics st = print_job_timer.getStats();
    snprintf(a_cnt, sizeof(a_cnt), "%u (%u ok)", (unsigned)st.totalPrints, (unsigned)st.finishedPrints);
    add_l("Impresiones", a_cnt);
    fmt_hm(a_tot, sizeof(a_tot), st.printTime);
    add_l("Tiempo total", a_tot);
  #endif
  l.n = n;

  n = 0;
  auto add_r = [&](const char *label, const char *v, arq_tint_t dot) { r.rows[n].label = label; snprintf(r.rows[n].v1, sizeof(r.rows[n].v1), "%s", v); r.rows[n].v2[0] = 0; r.rows[n].dot = dot; ++n; };
  r.title = "Diagn\xC3\xB3stico";
  #if HAS_HOTEND
    { const int t = thermalManager.wholeDegHotend(0); char v[12]; const bool ok = t > 0 && t < HEATER_0_MAXTEMP - 5;
      if (ok) snprintf(v, sizeof(v), "%d\xC2\xB0", t); else snprintf(v, sizeof(v), "Revisar");
      add_r("Sensor hotend", v, ok ? ARQ_TINT_OK : ARQ_TINT_DANGER); }
  #endif
  #if HAS_HEATED_BED
    { const int t = thermalManager.wholeDegBed(); char v[12]; const bool ok = t > 0 && t < BED_MAXTEMP - 5;
      if (ok) snprintf(v, sizeof(v), "%d\xC2\xB0", t); else snprintf(v, sizeof(v), "Revisar");
      add_r("Sensor cama", v, ok ? ARQ_TINT_OK : ARQ_TINT_DANGER); }
  #endif
  // Finales de carrera: lectura directa del pin con la misma regla que M119 (READ(pin) != *_ENDSTOP_INVERTING); activado = ambar
  #define ARQ_ES(LBL, S, INV) { const bool hit = READ(S##_PIN) != (INV); add_r(LBL, hit ? "Activado" : "Abierto", hit ? ARQ_TINT_WARN : ARQ_TINT_MUTED); }
  #if PIN_EXISTS(X_MIN) && ENABLED(USE_XMIN_PLUG)
    ARQ_ES("Final X", X_MIN, X_MIN_ENDSTOP_INVERTING)
  #endif
  #if PIN_EXISTS(Y_MIN) && ENABLED(USE_YMIN_PLUG)
    ARQ_ES("Final Y", Y_MIN, Y_MIN_ENDSTOP_INVERTING)
  #endif
  #if PIN_EXISTS(Z_MIN) && ENABLED(USE_ZMIN_PLUG)
    ARQ_ES("Final Z", Z_MIN, Z_MIN_ENDSTOP_INVERTING)
  #endif
  #if HAS_MEDIA
    add_r("Tarjeta SD", card.isMounted() ? "Montada" : "Sin tarjeta", card.isMounted() ? ARQ_TINT_OK : ARQ_TINT_MUTED);
  #endif
  #if ARQ_HAS_MESH
    add_r("Malla de cama", arq_mesh_is_leveling_on() ? "Activa" : (arq_mesh_is_valid() ? "Inactiva" : "Sin malla"), arq_mesh_is_leveling_on() ? ARQ_TINT_OK : (arq_mesh_is_valid() ? ARQ_TINT_WARN : ARQ_TINT_MUTED));
  #endif
  add_r("Marlin", (IsStopped() || marlin_state == MF_KILLED) ? "Detenido" : "Normal", (IsStopped() || marlin_state == MF_KILLED) ? ARQ_TINT_DANGER : ARQ_TINT_OK);
  r.n = n;
}

static void about_task(lv_task_t *) { about_fill(); arq_info_refresh(); }

static void about_cb(lv_obj_t *obj, lv_event_t event) {
  if (event == LV_EVENT_RELEASED && obj->mks_obj_id == ARQ_IID_BACK) goto_previous_ui();
}

void arq_about_draw() {
  a_scr = lv_screen_create(ABOUT_UI, "");
  arq_screen_apply(a_scr);
  memset(&a_left, 0, sizeof(a_left)); memset(&a_right, 0, sizeof(a_right));
  about_fill();
  arq_info_build(a_scr, about_cb, "Sistema y diagn\xC3\xB3stico", &a_left, &a_right);
  a_task = lv_task_create(about_task, 500, LV_TASK_PRIO_LOWEST, 0);        // valores en vivo (tiempo encendida, finales de carrera, temperaturas)
  add_to_group(a_scr);
}

void arq_about_clear() {
  remove_group();
  if (a_task) lv_task_del(a_task);
  a_task = nullptr;
  arq_info_forget();
  if (a_scr) lv_obj_del(a_scr);
  a_scr = nullptr;
}

// ==== BABYSTEP (draw_baby_stepping.cpp) ==========================================================================================
static lv_obj_t *b_scr;
static float     babystep_dist = 0.01f;
static uint8_t   has_adjust_z;

void arq_baby_refresh() {
  if (!b_scr) return;
  arq_move_model_t m;
  memset(&m, 0, sizeof(m));
  const int d = (int)(100 * babystep_dist);
  m.step = d == 1 ? 0 : d == 5 ? 1 : 2;
  #if HAS_BED_PROBE
    m.z_off_um = (int32_t)(probe.offset.z * 1000.0f + (probe.offset.z >= 0 ? 0.5f : -0.5f));
  #endif
  arq_move_update(&m);
}

static void baby_cb(lv_obj_t *obj, lv_event_t event) {
  if (event != LV_EVENT_RELEASED) return;
  const int id = obj->mks_obj_id;
  if (id == ARQ_MID_BACK) {
    if (has_adjust_z == 1) { TERN_(EEPROM_SETTINGS, (void)settings.save()); has_adjust_z = 0; }     // como ID_BABYSTEP_RETURN
    goto_previous_ui();
    return;
  }
  if (id == ARQ_MID_S01) babystep_dist = 0.01f;
  else if (id == ARQ_MID_S1) babystep_dist = 0.05f;
  else if (id == ARQ_MID_S10) babystep_dist = 0.1f;
  else {
    char axis = 'Z', baby_buf[30] = { 0 }, str_1[16];
    float d = babystep_dist;
    switch (id) {
      case ARQ_MID_XN: d = -d; case ARQ_MID_XP: axis = 'X'; break;
      case ARQ_MID_YN: d = -d; case ARQ_MID_YP: axis = 'Y'; break;
      case ARQ_MID_ZN: d = -d; case ARQ_MID_ZP: axis = 'Z'; break;
      default: return;
    }
    sprintf_P(baby_buf, PSTR("M290 %c%s"), axis, dtostrf(d, 1, 3, str_1));
    gcode.process_subcommands_now(F(baby_buf));                      // misma llamada que draw_baby_stepping.cpp
    has_adjust_z = 1;
  }
  arq_baby_refresh();
}

void arq_baby_draw() {
  b_scr = lv_screen_create(BABYSTEP_UI, "");
  arq_screen_apply(b_scr);
  arq_move_build(b_scr, baby_cb, true);
  arq_baby_refresh();
  add_to_group(b_scr);
}

void arq_baby_clear() {
  remove_group();
  arq_move_forget();
  if (b_scr) lv_obj_del(b_scr);
  b_scr = nullptr;
}

// ==== VELOCIDAD (draw_change_speed.cpp) ==========================================================================================
static lv_obj_t *s_scr;
static bool      editingFlowrate;

static void speed_refresh() {
  if (!s_scr) return;
  arq_speed_model_t m;
  m.feed = feedrate_percentage; m.flow = planner.flow_percentage[0]; m.sel = editingFlowrate ? 1 : 0; m.step = uiCfg.stepPrintSpeed;
  arq_speed_update(&m);
}

static void speed_cb(lv_obj_t *obj, lv_event_t event) {
  if (event != LV_EVENT_RELEASED) return;
  switch (obj->mks_obj_id) {
    case ARQ_SID_BACK: goto_previous_ui(); return;
    case ARQ_SID_FEED: editingFlowrate = false; break;
    case ARQ_SID_FLOW: editingFlowrate = true; break;
    case ARQ_SID_S1:  uiCfg.stepPrintSpeed =  1; break;
    case ARQ_SID_S5:  uiCfg.stepPrintSpeed =  5; break;
    case ARQ_SID_S10: uiCfg.stepPrintSpeed = 10; break;
    // INC / DEC: draw_change_speed.cpp (ID_C_ADD / ID_C_DEC), con los mismos limites
    case ARQ_SID_INC:
      if (!editingFlowrate) {
        if (feedrate_percentage < MAX_EXT_SPEED_PERCENT - uiCfg.stepPrintSpeed) feedrate_percentage += uiCfg.stepPrintSpeed;
        else feedrate_percentage = MAX_EXT_SPEED_PERCENT;
      }
      else {
        if (planner.flow_percentage[0] < MAX_EXT_SPEED_PERCENT - uiCfg.stepPrintSpeed) planner.flow_percentage[0] += uiCfg.stepPrintSpeed;
        else planner.flow_percentage[0] = MAX_EXT_SPEED_PERCENT;
        planner.refresh_e_factor(0);
        #if HAS_MULTI_EXTRUDER
          planner.flow_percentage[1] = planner.flow_percentage[0];
          planner.refresh_e_factor(1);
        #endif
      }
      break;
    case ARQ_SID_DEC:
      if (!editingFlowrate) {
        if (feedrate_percentage > MIN_EXT_SPEED_PERCENT + uiCfg.stepPrintSpeed) feedrate_percentage -= uiCfg.stepPrintSpeed;
        else feedrate_percentage = MIN_EXT_SPEED_PERCENT;
      }
      else {
        if (planner.flow_percentage[0] > MIN_EXT_SPEED_PERCENT + uiCfg.stepPrintSpeed) planner.flow_percentage[0] -= uiCfg.stepPrintSpeed;
        else planner.flow_percentage[0] = MIN_EXT_SPEED_PERCENT;
        planner.refresh_e_factor(0);
        #if HAS_MULTI_EXTRUDER
          planner.flow_percentage[1] = planner.flow_percentage[0];
          planner.refresh_e_factor(1);
        #endif
      }
      break;
  }
  speed_refresh();
}

void arq_speed_draw() {
  s_scr = lv_screen_create(CHANGE_SPEED_UI, "");
  arq_screen_apply(s_scr);
  if (uiCfg.stepPrintSpeed != 1 && uiCfg.stepPrintSpeed != 5 && uiCfg.stepPrintSpeed != 10) uiCfg.stepPrintSpeed = 10;
  arq_speed_build(s_scr, speed_cb);
  speed_refresh();
  add_to_group(s_scr);
}

void arq_speed_clear() {
  remove_group();
  arq_speed_forget();
  if (s_scr) lv_obj_del(s_scr);
  s_scr = nullptr;
}

// ==== NIVELAR CAMA (draw_manuaLevel.cpp) =========================================================================================
static lv_obj_t *l_scr;

static void level_cb(lv_obj_t *obj, lv_event_t event) {
  if (event != LV_EVENT_RELEASED) return;
  const int id = obj->mks_obj_id;
  if (id == ARQ_LID_BACK) { arq_level_clear(); lv_draw_tool(); return; }          // como ID_MANUAL_RETURN
  if (id >= ARQ_LID_P1 && id <= ARQ_LID_P5 && queue.ring_buffer.empty()) {         // como ID_M_POINT1..5: solo con la cola vacia
    if (uiCfg.leveling_first_time) {
      uiCfg.leveling_first_time = false;
      #if ANY(ENABLE_LEVELING_AFTER_G28, RESTORE_LEVELING_AFTER_G28)
        queue.inject(F("G28L0"));       // Arquis: ENABLE_LEVELING_AFTER_G28 activaria la malla; los tornillos se ajustan SIN compensacion
      #else
        queue.inject_P(G28_STR);
      #endif
    }
    const int ind = id - ARQ_LID_P1;
    sprintf_P(public_buf_l, PSTR("G1Z10\nG1X%dY%d\nG1Z0"), gCfgItems.trammingPos[ind].x, gCfgItems.trammingPos[ind].y);
    queue.inject(public_buf_l);
  }
}

void arq_level_draw() {
  l_scr = lv_screen_create(LEVELING_UI, "");
  arq_screen_apply(l_scr);
  arq_level_model_t m;
  memset(&m, 0, sizeof(m));
  m.bed_x = (int16_t)(X_MAX_POS - X_MIN_POS); m.bed_y = (int16_t)(Y_MAX_POS - Y_MIN_POS);
  for (uint8_t i = 0; i < 5; ++i) { m.x[i] = (int16_t)(gCfgItems.trammingPos[i].x - X_MIN_POS); m.y[i] = (int16_t)(gCfgItems.trammingPos[i].y - Y_MIN_POS); }
  arq_level_build(l_scr, level_cb, &m);
  add_to_group(l_scr);
}

void arq_level_clear() {
  remove_group();
  arq_level_forget();
  if (l_scr) lv_obj_del(l_scr);
  l_scr = nullptr;
}

// ==== MAS (draw_more.cpp) ========================================================================================================
// G-code + los accesos personalizados de la configuracion (MAIN_MENU_ITEM_n_GCODE; solo si estan definidos). Mismas acciones que la original.
static Grid grid_more;
enum { MR_GCODE, MR_C1, MR_C2, MR_C3, MR_C4, MR_C5, MR_C6 };

static void more_cb(lv_obj_t *obj, lv_event_t event) {
  if (event != LV_EVENT_RELEASED) return;
  if (obj->mks_obj_id == ARQ_XID_BACK) { arq_more_clear(); lv_draw_tool(); return; }
  switch (grid_more.action(obj)) {
    case MR_GCODE: arq_more_clear(); lv_draw_gcode(true); break;
    #if HAS_USER_ITEM(1)
      case MR_C1: queue.inject(F(MAIN_MENU_ITEM_1_GCODE)); break;
    #endif
    #if HAS_USER_ITEM(2)
      case MR_C2: queue.inject(F(MAIN_MENU_ITEM_2_GCODE)); break;
    #endif
    #if HAS_USER_ITEM(3)
      case MR_C3: queue.inject(F(MAIN_MENU_ITEM_3_GCODE)); break;
    #endif
    #if HAS_USER_ITEM(4)
      case MR_C4: queue.inject(F(MAIN_MENU_ITEM_4_GCODE)); break;
    #endif
    #if HAS_USER_ITEM(5)
      case MR_C5: queue.inject(F(MAIN_MENU_ITEM_5_GCODE)); break;
    #endif
    #if HAS_USER_ITEM(6)
      case MR_C6: queue.inject(F(MAIN_MENU_ITEM_6_GCODE)); break;
    #endif
    default: break;
  }
}

void arq_more_draw() {
  grid_more.begin();
  grid_more.add(MR_GCODE, &arq_icon_tools, "G-code");
  #if HAS_USER_ITEM(1)
    grid_more.add(MR_C1, &arq_icon_gear, more_menu.custom1);
  #endif
  #if HAS_USER_ITEM(2)
    grid_more.add(MR_C2, &arq_icon_gear, more_menu.custom2);
  #endif
  #if HAS_USER_ITEM(3)
    grid_more.add(MR_C3, &arq_icon_gear, more_menu.custom3);
  #endif
  #if HAS_USER_ITEM(4)
    grid_more.add(MR_C4, &arq_icon_gear, more_menu.custom4);
  #endif
  #if HAS_USER_ITEM(5)
    grid_more.add(MR_C5, &arq_icon_gear, more_menu.custom5);
  #endif
  #if HAS_USER_ITEM(6)
    grid_more.add(MR_C6, &arq_icon_gear, more_menu.custom6);
  #endif
  grid_more.show(MORE_UI, "M\xC3\xA1s", more_cb);
}
void arq_more_clear() { grid_more.clear(); }

// ==== IDIOMA (draw_language.cpp) =================================================================================================
// Mismas acciones que la original: guarda el idioma en la flash SPI y recarga las tablas de textos de MKS. ARQUIS UI conserva solo
// espanol e ingles. Las pantallas ARQUIS estan en espanol; el idioma afecta a los textos de MKS (con ingles se muestran sin traducir).
static Grid grid_lang;
static const uint8_t lang_code[2] = { LANG_SPANISH, LANG_ENGLISH };

static void lang_mark() {
  for (uint8_t i = 0; i < 2; ++i) {
    const bool on = (gCfgItems.language == lang_code[i]);
    arq_opts_set(i, on ? "Activo" : nullptr, on ? ARQ_BTN_SUCCESS : ARQ_BTN_SECONDARY);
  }
}

static void lang_cb(lv_obj_t *obj, lv_event_t event) {
  if (event != LV_EVENT_RELEASED) return;
  if (obj->mks_obj_id == ARQ_XID_BACK) { arq_language_clear(); lv_draw_set(); return; }
  const int i = grid_lang.action(obj);
  if (i < 0 || i > 1) return;
  gCfgItems.language = lang_code[i];
  update_spi_flash();
  disp_language_init();
  lang_mark();
}

void arq_language_draw() {
  grid_lang.begin();
  grid_lang.add(0, &arq_icon_gear, "Espa\xC3\xB1ol");
  grid_lang.add(1, &arq_icon_gear, "Ingl\xC3\xA9s");
  grid_lang.show(LANGUAGE_UI, "Idioma", lang_cb);
  lang_mark();
}
void arq_language_clear() { grid_lang.clear(); }

#endif // HAS_TFT_LVGL_UI && ARQUIS_UI
