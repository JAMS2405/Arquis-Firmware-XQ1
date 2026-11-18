// STUB de draw_ui.h: solo lo que usan arquis_nav/dashboard/data/print (firmas reales de draw_ui.h y draw_*.h de Marlin 2.1.2.10)
#pragma once
#include <lvgl.h>
#include "../../../inc/MarlinConfigPre.h"
typedef enum { MAIN_UI, PRINT_READY_UI, PRINTING_UI, CANCEL_OBJECT_UI, PREHEAT_UI, FAN_UI, MOVE_MOTOR_UI, EXTRUSION_UI, OPERATE_UI } DISP_STATE;
typedef struct { DISP_STATE _disp_state[100]; int _disp_index; } DISP_STATE_STACK;
extern DISP_STATE_STACK disp_state_stack;
extern DISP_STATE disp_state;
enum { IDLE, WORKING, PAUSING, PAUSED, REPRINTING, REPRINTED, RESUMING, STOP };   // draw_printing.h
// UI_CFG: campos y anchos de bit IGUALES a los de draw_ui.h real (Marlin 2.1.2.10 MKS)
typedef int16_t celsius_t_ui;
typedef struct UI_Config_Struct {
  uint8_t curTempType:1, extruderIndex:3, stepHeat:4, extruderIndexBak:4;
  uint8_t extruStep;
  static constexpr uint8_t eStepMin = 1, eStepMed = 5, eStepMax = 10;
  uint8_t extruSpeed;
  static constexpr uint8_t eSpeedH = 20, eSpeedN = 10, eSpeedL = 1;
  uint8_t print_state;
  uint16_t moveSpeed, moveSpeed_bak;
  float move_dist;
  int16_t hotendTargetTempBak;
} UI_CFG; extern UI_CFG uiCfg;
typedef struct { bool from_flash_pic; bool encoder_enable; bool finish_power_off; uint32_t curFilesize; } GCFG; extern GCFG gCfgItems;   // (real: mas campos)
typedef struct { int16_t days; uint16_t hours; uint8_t minutes; volatile int8_t seconds; int8_t ms_10; int8_t start; } PRINT_TIME; extern PRINT_TIME print_time;
#define PREVIEW_SIZE 202720
lv_obj_t* lv_screen_create(DISP_STATE newScreenType, const char *title = nullptr);
void lv_draw_print_file(); void lv_draw_printing(); void lv_draw_move_motor(); void lv_draw_preHeat(); void lv_draw_fan();
void lv_draw_tool(); void lv_draw_set(); void lv_draw_ready_print(); void lv_draw_operation(); void clear_cur_ui();
void stop_print_time(); void start_print_time(); void reset_print_time();
#if ENABLED(CANCEL_OBJECTS)
  void lv_draw_cancel_object(); void lv_clear_cancel_object(); void lv_refresh_cancel_object();
  lv_obj_t* lv_cancel_object_button_create(lv_obj_t *par, lv_coord_t x, lv_coord_t y, lv_event_cb_t cb, const int id);
#endif
// firmas reales de draw_ui.h (Marlin 2.1.2.10 MKS)
void goto_previous_ui();
#define BTN_X_PIXEL 117
#define BTN_Y_PIXEL 92
extern lv_style_t tft_style_label_rel;
lv_obj_t* lv_label_create_empty(lv_obj_t *par);
lv_obj_t* lv_btn_create(lv_obj_t *par, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h, lv_event_cb_t cb, const int id, lv_style_t *style);
#define HAS_ROTARY_ENCODER 1

extern char public_buf_l[30];
void lv_draw_preHeat(); void lv_draw_extrusion(); void lv_draw_filament_change(); void lv_draw_change_speed(); void lv_draw_baby_stepping(); void update_spi_flash();
