// STUB de draw_ui.h: solo lo que usan arquis_nav/dashboard/data/print (firmas reales de draw_ui.h y draw_*.h de Marlin 2.1.2.10)
#pragma once
#include <lvgl.h>
#include "../../../inc/MarlinConfigPre.h"
typedef enum { TOOL_UI, ZERO_UI, SET_UI, ABOUT_UI, BABYSTEP_UI, CHANGE_SPEED_UI, LEVELING_UI, MAIN_UI, PRINT_READY_UI, PRINTING_UI, DIALOG_UI, PRINT_FILE_UI, CANCEL_OBJECT_UI, PREHEAT_UI, FAN_UI, MOVE_MOTOR_UI, EXTRUSION_UI, OPERATE_UI } DISP_STATE;
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
  uint8_t dialogType;
  uint8_t stepPrintSpeed;
  bool leveling_first_time;
  uint16_t moveSpeed, moveSpeed_bak;
  float move_dist;
  int16_t hotendTargetTempBak;
} UI_CFG; extern UI_CFG uiCfg;
struct xy_int_stub { int16_t x, y; };
typedef struct { xy_int_stub trammingPos[5]; bool from_flash_pic; bool encoder_enable; bool finish_power_off; uint32_t curFilesize; } GCFG; extern GCFG gCfgItems;   // (real: mas campos)
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
enum { DIALOG_TYPE_PRINT_FILE = 1 };
#include "print_file_types.h"    // LIST_FILE/DIR_OFFSET REALES (extraidos de draw_print_file.h por check_marlin_api.sh)
extern uint8_t sel_id; bool have_pre_pic(char *path); int ascii2dec_test(char *ascii);
#define HAS_GCODE_PREVIEW 1
void lv_draw_dialog(uint8_t type); void lv_draw_media_select(); void lv_clear_print_file(); typedef struct { uint8_t dummy; } disp_stub_t;

// --- Etapa 5 (firmas de draw_ui.h y draw_*.h reales) ---
void lv_draw_home(); void lv_draw_manualLevel(); void lv_draw_more(); void lv_draw_language(); void lv_draw_machine_para(); void lv_draw_eeprom_settings(); void lv_draw_about(); void lv_draw_tool(); void lv_draw_set();
void get_gcode_command(int addr, uint8_t *d);
#define AUTO_LEVELING_COMMAND_ADDR 1
