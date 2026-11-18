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

#include "arquis_controls.h"
#include "arquis_ctrl_view.h"
#include "arquis_data.h"
#include "draw_ui.h"

#include "../../../inc/MarlinConfig.h"
#include "../../../module/temperature.h"
#include "../../../module/motion.h"
#include "../../../gcode/queue.h"
#include "../../../sd/cardreader.h"
#if ENABLED(CANCEL_OBJECTS)
  #include "../../../feature/cancel_object.h"
#endif

#include <stdio.h>
#include <string.h>

extern lv_group_t *g;

#ifndef TEMP_HYSTERESIS
  #define TEMP_HYSTERESIS 3
#endif
#ifndef TEMP_BED_HYSTERESIS
  #define TEMP_BED_HYSTERESIS 3
#endif

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

// ==== TEMPERATURA (draw_preHeat.cpp) ============================================================================================
static lv_obj_t *t_scr;

static void temps_model(arq_temps_model_t *m) {
  memset(m, 0, sizeof(*m));
  #if HAS_HOTEND
    m->hot_cur = thermalManager.wholeDegHotend(uiCfg.extruderIndex); m->hot_tgt = thermalManager.degTargetHotend(uiCfg.extruderIndex); m->hot_tol = TEMP_HYSTERESIS;
  #endif
  #if HAS_HEATED_BED
    m->bed_cur = thermalManager.wholeDegBed(); m->bed_tgt = thermalManager.degTargetBed(); m->bed_tol = TEMP_BED_HYSTERESIS;
  #endif
  m->sel = uiCfg.curTempType ? 1 : 0;
  m->step = uiCfg.stepHeat;
  m->p1 = m->sel ? TERN(HAS_HEATED_BED, PREHEAT_1_TEMP_BED, 0) : TERN(HAS_HOTEND, PREHEAT_1_TEMP_HOTEND, 0);
  m->p2 = m->sel ? TERN(HAS_HEATED_BED, PREHEAT_2_TEMP_BED, 0) : TERN(HAS_HOTEND, PREHEAT_2_TEMP_HOTEND, 0);
}

void arq_temps_refresh() {
  if (!t_scr) return;
  arq_temps_model_t m;
  temps_model(&m);
  arq_temps_update(&m);
}

static void temps_cb(lv_obj_t *obj, lv_event_t event) {
  if (event != LV_EVENT_RELEASED) return;
  switch (obj->mks_obj_id) {
    case ARQ_TID_BACK: goto_previous_ui(); return;

    case ARQ_TID_HOT:
      #if HAS_MULTI_EXTRUDER
        if (uiCfg.curTempType == 0) uiCfg.extruderIndex = uiCfg.extruderIndex ? 0 : 1;   // tocar de nuevo el hotend alterna entre extrusores
      #endif
      uiCfg.curTempType = 0;
      break;
    case ARQ_TID_BED: uiCfg.curTempType = TERN(HAS_HEATED_BED, 1, 0); break;

    case ARQ_TID_S1:  uiCfg.stepHeat =  1; break;
    case ARQ_TID_S5:  uiCfg.stepHeat =  5; break;
    case ARQ_TID_S10: uiCfg.stepHeat = 10; break;

    // Las ramas siguientes son las de draw_preHeat.cpp (ID_P_ADD, ID_P_DEC, ID_P_OFF, ID_P_PLA, ID_P_ABS), con los mismos limites.
    case ARQ_TID_INC:
      if (uiCfg.curTempType == 0) {
        #if HAS_HOTEND
          int16_t max_target;
          thermalManager.temp_hotend[uiCfg.extruderIndex].target += uiCfg.stepHeat;
          if (uiCfg.extruderIndex == 0)
            max_target = HEATER_0_MAXTEMP - (WATCH_TEMP_INCREASE + TEMP_HYSTERESIS + 1);
          else {
            #if HAS_MULTI_HOTEND
              max_target = HEATER_1_MAXTEMP - (WATCH_TEMP_INCREASE + TEMP_HYSTERESIS + 1);
            #endif
          }
          if (thermalManager.degTargetHotend(uiCfg.extruderIndex) > max_target)
            thermalManager.setTargetHotend(max_target, uiCfg.extruderIndex);
          thermalManager.start_watching_hotend(uiCfg.extruderIndex);
        #endif
      }
      else {
        #if HAS_HEATED_BED
          constexpr int16_t max_target = BED_MAXTEMP - (WATCH_BED_TEMP_INCREASE + TEMP_BED_HYSTERESIS + 1);
          thermalManager.temp_bed.target += uiCfg.stepHeat;
          if (thermalManager.degTargetBed() > max_target)
            thermalManager.setTargetBed(max_target);
          thermalManager.start_watching_bed();
        #endif
      }
      break;

    case ARQ_TID_DEC:
      if (uiCfg.curTempType == 0) {
        #if HAS_HOTEND
          if (thermalManager.degTargetHotend(uiCfg.extruderIndex) > uiCfg.stepHeat)
            thermalManager.temp_hotend[uiCfg.extruderIndex].target -= uiCfg.stepHeat;
          else
            thermalManager.setTargetHotend(0, uiCfg.extruderIndex);
          thermalManager.start_watching_hotend(uiCfg.extruderIndex);
        #endif
      }
      else {
        #if HAS_HEATED_BED
          if (thermalManager.degTargetBed() > uiCfg.stepHeat)
            thermalManager.temp_bed.target -= uiCfg.stepHeat;
          else
            thermalManager.setTargetBed(0);
          thermalManager.start_watching_bed();
        #endif
      }
      break;

    case ARQ_TID_OFF:
      if (uiCfg.curTempType == 0) {
        #if HAS_HOTEND
          thermalManager.setTargetHotend(0, uiCfg.extruderIndex);
          thermalManager.start_watching_hotend(uiCfg.extruderIndex);
        #endif
      }
      else {
        #if HAS_HEATED_BED
          thermalManager.temp_bed.target = 0;
          thermalManager.start_watching_bed();
        #endif
      }
      break;

    case ARQ_TID_P2:   // ABS
      if (uiCfg.curTempType == 0) { TERN_(HAS_HOTEND, thermalManager.setTargetHotend(PREHEAT_2_TEMP_HOTEND, 0)); }
      else if (uiCfg.curTempType == 1) { TERN_(HAS_HEATED_BED, thermalManager.setTargetBed(PREHEAT_2_TEMP_BED)); }
      break;
    case ARQ_TID_P1:   // PLA
      if (uiCfg.curTempType == 0) { TERN_(HAS_HOTEND, thermalManager.setTargetHotend(PREHEAT_1_TEMP_HOTEND, 0)); }
      else if (uiCfg.curTempType == 1) { TERN_(HAS_HEATED_BED, thermalManager.setTargetBed(PREHEAT_1_TEMP_BED)); }
      break;
  }
  arq_temps_refresh();
}

void arq_temps_draw() {
  t_scr = lv_screen_create(PREHEAT_UI, "");
  arq_screen_apply(t_scr);
  if (uiCfg.stepHeat != 1 && uiCfg.stepHeat != 5 && uiCfg.stepHeat != 10) uiCfg.stepHeat = 10;
  #if !HAS_HEATED_BED
    uiCfg.curTempType = 0;
  #endif
  arq_temps_build(t_scr, temps_cb, ENABLED(HAS_HOTEND), ENABLED(HAS_HEATED_BED), PREHEAT_1_LABEL, PREHEAT_2_LABEL);
  arq_temps_refresh();
  add_to_group(t_scr);
}

void arq_temps_clear() {
  remove_group();
  arq_temps_forget();
  if (t_scr) lv_obj_del(t_scr);
  t_scr = nullptr;
}

// ==== VENTILADORES (draw_fan.cpp) ================================================================================================
static lv_obj_t *f_scr;
static uint8_t   f_n, f_idx[ARQ_FANCTL_MAX], f_sel;
static const char *f_names[ARQ_FANCTL_MAX];
static const char *const aux_names[] = { "Aux", "Aux 1", "Aux 2", "Aux 3", "Aux 4" };

void arq_fan_refresh() {
  if (!f_scr) return;
  arq_fanctl_model_t m;
  memset(&m, 0, sizeof(m));
  m.n = f_n; m.sel = f_sel;
  #if HAS_FAN
    for (uint8_t i = 0; i < f_n; ++i) m.pct[i] = thermalManager.fanSpeedPercent(f_idx[i]);
  #endif
  arq_fanctl_update(&m);
}

static void fan_cb(lv_obj_t *obj, lv_event_t event) {
  if (event != LV_EVENT_RELEASED) return;
  const int id = obj->mks_obj_id;
  if (id == ARQ_FID_BACK) { goto_previous_ui(); return; }
  if (id == ARQ_FID_F0) f_sel = 0;
  else if (id == ARQ_FID_F1 && f_n > 1) f_sel = 1;
  #if HAS_FAN
    else {
      int pct = thermalManager.fanSpeedPercent(f_idx[f_sel]);
      switch (id) {
        case ARQ_FID_INC:  pct += 5; break;
        case ARQ_FID_DEC:  pct -= 5; break;
        case ARQ_FID_FULL: pct = 100; break;
        case ARQ_FID_HALF: pct =  50; break;
        case ARQ_FID_OFF:  pct =   0; break;
        default: break;
      }
      pct = pct < 0 ? 0 : pct > 100 ? 100 : pct;
      thermalManager.set_fan_speed(f_idx[f_sel], (uint16_t)((pct * 255 + 50) / 100));      // misma llamada que draw_fan.cpp
    }
  #endif
  arq_fan_refresh();
}

void arq_fan_draw() {
  f_scr = lv_screen_create(FAN_UI, "");
  arq_screen_apply(f_scr);
  f_n = 0; f_sel = 0;
  #if HAS_FAN0
    f_idx[f_n] = 0; f_names[f_n] = "Capa"; ++f_n;
  #endif
  #if HAS_FAN1
    for (uint8_t i = 1; i < FAN_COUNT && f_n < ARQ_FANCTL_MAX; ++i) { f_idx[f_n] = i; f_names[f_n] = aux_names[i < 5 ? i : 0]; ++f_n; }
  #endif
  arq_fanctl_build(f_scr, fan_cb, f_n, f_names);
  arq_fan_refresh();
  add_to_group(f_scr);
}

void arq_fan_clear() {
  remove_group();
  arq_fanctl_forget();
  if (f_scr) lv_obj_del(f_scr);
  f_scr = nullptr;
}

// ==== MOVER (draw_move_motor.cpp) ================================================================================================
static lv_obj_t  *m_scr;
static lv_task_t *m_task;

static int32_t to_cmm(float v) { return (int32_t)(v * 100.0f + (v >= 0 ? 0.5f : -0.5f)); }

static void move_refresh() {
  if (!m_scr) return;
  arq_move_model_t m;
  memset(&m, 0, sizeof(m));
  m.pos_cmm[0] = to_cmm(current_position.x); m.pos_cmm[1] = to_cmm(current_position.y); m.pos_cmm[2] = to_cmm(current_position.z);
  m.homed = (axis_was_homed(X_AXIS) ? 1 : 0) | (axis_was_homed(Y_AXIS) ? 2 : 0) | (axis_was_homed(Z_AXIS) ? 4 : 0);
  const int d = (int)(10 * uiCfg.move_dist);
  m.step = d == 1 ? 0 : d == 100 ? 2 : 1;
  arq_move_update(&m);
}

static void move_task(lv_task_t *) { move_refresh(); }

static void move_cb(lv_obj_t *obj, lv_event_t event) {
  if (event != LV_EVENT_RELEASED) return;
  const int id = obj->mks_obj_id;
  switch (id) {
    case ARQ_MID_BACK: goto_previous_ui(); return;
    case ARQ_MID_S01: uiCfg.move_dist = 0.1f; break;
    case ARQ_MID_S1:  uiCfg.move_dist = 1;    break;
    case ARQ_MID_S10: uiCfg.move_dist = 10;   break;
    default: {
      // Igual que draw_move_motor.cpp: movimiento relativo inyectado en la cola, solo si hay sitio.
      char axis = 'Z', str_1[16];
      float dist = uiCfg.move_dist;
      switch (id) {
        case ARQ_MID_XN: dist *= -1; case ARQ_MID_XP: axis = 'X'; break;
        case ARQ_MID_YN: dist *= -1; case ARQ_MID_YP: axis = 'Y'; break;
        case ARQ_MID_ZN: dist *= -1; case ARQ_MID_ZP: axis = 'Z'; break;
        default: return;
      }
      if (!queue.ring_buffer.full(3)) {
        sprintf_P(public_buf_l, PSTR("G91\nG1 %c%s F%d\nG90"), axis, dtostrf(dist, 1, 3, str_1), uiCfg.moveSpeed);
        queue.inject(public_buf_l);
      }
    } break;
  }
  move_refresh();
}

void arq_move_draw() {
  m_scr = lv_screen_create(MOVE_MOTOR_UI, "");
  arq_screen_apply(m_scr);
  arq_move_build(m_scr, move_cb);
  move_refresh();
  m_task = lv_task_create(move_task, 300, LV_TASK_PRIO_LOWEST, 0);
  add_to_group(m_scr);
}

void arq_move_clear() {
  remove_group();
  if (m_task) lv_task_del(m_task);
  m_task = nullptr;
  arq_move_forget();
  if (m_scr) lv_obj_del(m_scr);
  m_scr = nullptr;
}

// ==== EXTRUSION (draw_extrusion.cpp) =============================================================================================
static lv_obj_t *e_scr;
static int32_t   e_amount;

void arq_extrude_refresh() {
  if (!e_scr) return;
  arq_extrude_model_t m;
  memset(&m, 0, sizeof(m));
  #if HAS_HOTEND
    m.cur = thermalManager.wholeDegHotend(uiCfg.extruderIndex); m.tgt = thermalManager.degTargetHotend(uiCfg.extruderIndex);
    m.hot_enough = thermalManager.hotEnoughToExtrude(uiCfg.extruderIndex);
    m.min_temp = thermalManager.extrude_min_temp;
  #endif
  m.tol = TEMP_HYSTERESIS;
  m.ext_idx = uiCfg.extruderIndex ? 1 : 0;
  m.amount = e_amount;
  m.step_mm = uiCfg.extruStep;
  m.speed = uiCfg.extruSpeed == uiCfg.eSpeedL ? 0 : uiCfg.extruSpeed == uiCfg.eSpeedH ? 2 : 1;
  arq_extrude_update(&m);
}

static void extrude_cb(lv_obj_t *obj, lv_event_t event) {
  if (event != LV_EVENT_RELEASED) return;
  switch (obj->mks_obj_id) {
    case ARQ_EID_BACK: goto_previous_ui(); return;
    case ARQ_EID_TEMP: uiCfg.curTempType = 0; arq_extrude_clear(); lv_draw_preHeat(); return;
    // IN / OUT: draw_extrusion.cpp (ID_E_ADD / ID_E_DEC); solo con el hotend a temperatura suficiente
    case ARQ_EID_IN:
      if (thermalManager.hotEnoughToExtrude(uiCfg.extruderIndex)) {
        sprintf_P((char *)public_buf_l, PSTR("G91\nG1 E%d F%d\nG90"), uiCfg.extruStep, 60 * uiCfg.extruSpeed);
        queue.inject(public_buf_l);
        e_amount += uiCfg.extruStep;
      }
      break;
    case ARQ_EID_OUT:
      if (thermalManager.hotEnoughToExtrude(uiCfg.extruderIndex)) {
        sprintf_P((char *)public_buf_l, PSTR("G91\nG1 E%d F%d\nG90"), 0 - uiCfg.extruStep, 60 * uiCfg.extruSpeed);
        queue.inject(public_buf_l);
        e_amount -= uiCfg.extruStep;
      }
      break;
    case ARQ_EID_TYPE:
      if (ENABLED(HAS_MULTI_EXTRUDER)) {
        if (uiCfg.extruderIndex == 0) { uiCfg.extruderIndex = 1; queue.inject(F("T1")); }
        else                          { uiCfg.extruderIndex = 0; queue.inject(F("T0")); }
      }
      else uiCfg.extruderIndex = 0;
      e_amount = 0;
      break;
    case ARQ_EID_STEP:
      switch (uiCfg.extruStep) {
        case uiCfg.eStepMin: uiCfg.extruStep = uiCfg.eStepMed; break;
        case uiCfg.eStepMed: uiCfg.extruStep = uiCfg.eStepMax; break;
        case uiCfg.eStepMax: uiCfg.extruStep = uiCfg.eStepMin; break;
      }
      break;
    case ARQ_EID_SPEED:
      switch (uiCfg.extruSpeed) {
        case uiCfg.eSpeedL: uiCfg.extruSpeed = uiCfg.eSpeedN; break;
        case uiCfg.eSpeedN: uiCfg.extruSpeed = uiCfg.eSpeedH; break;
        case uiCfg.eSpeedH: uiCfg.extruSpeed = uiCfg.eSpeedL; break;
      }
      break;
  }
  arq_extrude_refresh();
}

void arq_extrude_draw() {
  e_scr = lv_screen_create(EXTRUSION_UI, "");
  arq_screen_apply(e_scr);
  e_amount = 0;
  arq_extrude_build(e_scr, extrude_cb, ENABLED(HAS_MULTI_EXTRUDER));
  arq_extrude_refresh();
  add_to_group(e_scr);
}

void arq_extrude_clear() {
  remove_group();
  arq_extrude_forget();
  if (e_scr) lv_obj_del(e_scr);
  e_scr = nullptr;
}

// ==== OPCIONES (draw_operation.cpp) ==============================================================================================
enum { ACT_TEMP, ACT_FILAMENT, ACT_FAN, ACT_POWEROFF, ACT_SPEED, ACT_BABY, ACT_EXTRUDE, ACT_MOVE, ACT_OBJECTS };
enum { DLG_FIL_OK = 401, DLG_FIL_NO };

static lv_obj_t *o_scr;
static uint8_t   o_act[ARQ_OPT_MAX], o_n, o_power_idx;

static const char *power_sub() { return gCfgItems.finish_power_off ? "Activado" : "Desactivado"; }

// Cambio de filamento: mismas llamadas que ID_O_FILAMENT de draw_operation.cpp
static void filament_change() {
  #if HAS_MULTI_EXTRUDER
    uiCfg.extruderIndexBak = active_extruder;
  #endif
  if (uiCfg.print_state == WORKING) {
    #if HAS_MEDIA
      card.pauseSDPrint();
      stop_print_time();
      uiCfg.print_state = PAUSING;
    #endif
  }
  uiCfg.moveSpeed_bak = (uint16_t)feedrate_mm_s;
  uiCfg.hotendTargetTempBak = thermalManager.degTargetHotend(active_extruder);
  arq_options_clear();
  lv_draw_filament_change();
}

static void options_dialog_cb(lv_obj_t *obj, lv_event_t event) {
  if (event != LV_EVENT_RELEASED) return;
  const int id = obj->mks_obj_id;
  arq_dialog_close();
  if (id == DLG_FIL_OK) filament_change();
}

static void options_cb(lv_obj_t *obj, lv_event_t event) {
  if (event != LV_EVENT_RELEASED) return;
  if (arq_dialog_is_open()) return;
  const int id = obj->mks_obj_id;
  if (id == ARQ_XID_BACK) { goto_previous_ui(); return; }
  const int i = id - ARQ_XID_ITEM0;
  if (i < 0 || i >= o_n) return;
  switch (o_act[i]) {
    case ACT_TEMP:     arq_options_clear(); lv_draw_preHeat(); break;
    case ACT_FAN:      arq_options_clear(); lv_draw_fan(); break;
    case ACT_SPEED:    arq_options_clear(); lv_draw_change_speed(); break;
    case ACT_BABY:     arq_options_clear(); lv_draw_baby_stepping(); break;
    case ACT_EXTRUDE:  arq_options_clear(); lv_draw_extrusion(); break;
    case ACT_MOVE:     arq_options_clear(); lv_draw_move_motor(); break;
    #if ENABLED(CANCEL_OBJECTS)
      case ACT_OBJECTS: arq_options_clear(); lv_draw_cancel_object(); break;
    #endif
    case ACT_FILAMENT:
      if (uiCfg.print_state == WORKING) {      // pausa la impresion y mueve el cabezal: se pide confirmacion
        arq_dialog_open("Cambiar filamento", "Se pausar\xC3\xA1 la impresi\xC3\xB3n y el cabezal se mover\xC3\xA1 a la posici\xC3\xB3n de pausa. Aseg\xC3\xBArate de que la cama est\xC3\xA1 libre.",
                        "Pausar y cambiar", "Volver", false, options_dialog_cb, DLG_FIL_OK, DLG_FIL_NO);
      }
      else filament_change();
      break;
    case ACT_POWEROFF:
      gCfgItems.finish_power_off = !gCfgItems.finish_power_off;
      update_spi_flash();
      arq_opts_set(o_power_idx, power_sub(), gCfgItems.finish_power_off ? ARQ_BTN_SUCCESS : ARQ_BTN_SECONDARY);
      break;
  }
}

void arq_options_draw() {
  o_scr = lv_screen_create(OPERATE_UI, "");
  arq_screen_apply(o_scr);
  arq_opt_item_t it[ARQ_OPT_MAX];
  o_n = 0;
  auto add = [&](uint8_t act, const lv_img_dsc_t *icon, const char *title, const char *sub = nullptr, uint8_t kind = ARQ_BTN_SECONDARY) {
    if (o_n >= ARQ_OPT_MAX) return;
    o_act[o_n] = act; it[o_n] = { icon, title, sub, kind }; ++o_n;
  };
  add(ACT_TEMP, &arq_icon_temp, "Temperatura");
  add(ACT_FILAMENT, &arq_icon_filament, "Filamento");
  add(ACT_FAN, &arq_icon_fan, "Ventiladores");
  #if HAS_SUICIDE     // sin control de alimentacion (pin SUICIDE) el autoapagado no hace nada al terminar: no se muestra un interruptor que no actua
    o_power_idx = o_n;
    add(ACT_POWEROFF, &arq_icon_power, "Autoapagado", power_sub(), gCfgItems.finish_power_off ? ARQ_BTN_SUCCESS : ARQ_BTN_SECONDARY);
  #endif
  if (uiCfg.print_state != WORKING) {                      // como la original: mover y extruir solo sin una impresion en marcha
    add(ACT_EXTRUDE, &arq_icon_down, "Extruir");
    add(ACT_MOVE, &arq_icon_move, "Mover");
  }
  else {
    add(ACT_SPEED, &arq_icon_speed, "Velocidad");
    add(ACT_BABY, &arq_icon_level, "Babystep");
  }
  #if ENABLED(CANCEL_OBJECTS)
    if (cancelable.object_count > 0) add(ACT_OBJECTS, &arq_icon_objects, "Objetos");
  #endif
  arq_opts_build(o_scr, options_cb, "Opciones", it, o_n);
  add_to_group(o_scr);
}

void arq_options_clear() {
  remove_group();
  arq_dialog_close();
  arq_opts_forget();
  if (o_scr) lv_obj_del(o_scr);
  o_scr = nullptr;
}

#endif // HAS_TFT_LVGL_UI && ARQUIS_UI
