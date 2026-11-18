// STUB de draw_ui.h: solo lo que usan arquis_nav/dashboard/data/print (firmas reales de draw_ui.h y draw_*.h de Marlin 2.1.2.10)
#pragma once
#include <lvgl.h>
#include "../../../inc/MarlinConfigPre.h"
typedef enum { MAIN_UI, PRINT_READY_UI, PRINTING_UI } DISP_STATE;
typedef struct { DISP_STATE _disp_state[100]; int _disp_index; } DISP_STATE_STACK;
extern DISP_STATE_STACK disp_state_stack;
extern DISP_STATE disp_state;
enum { IDLE, WORKING, PAUSING, PAUSED, REPRINTING, REPRINTED, RESUMING, STOP };   // draw_printing.h
typedef struct { uint8_t curTempType; uint8_t print_state; } UI_CFG; extern UI_CFG uiCfg;
typedef struct { bool from_flash_pic; bool encoder_enable; bool finish_power_off; uint32_t curFilesize; } GCFG; extern GCFG gCfgItems;
typedef struct { int16_t days; uint16_t hours; uint8_t minutes; volatile int8_t seconds; int8_t ms_10; int8_t start; } PRINT_TIME; extern PRINT_TIME print_time;
#define PREVIEW_SIZE 202720
lv_obj_t* lv_screen_create(DISP_STATE newScreenType, const char *title = nullptr);
void lv_draw_print_file(); void lv_draw_printing(); void lv_draw_move_motor(); void lv_draw_preHeat(); void lv_draw_fan();
void lv_draw_tool(); void lv_draw_set(); void lv_draw_ready_print(); void lv_draw_operation(); void clear_cur_ui();
void stop_print_time(); void start_print_time(); void reset_print_time();
#if ENABLED(CANCEL_OBJECTS)
  void lv_draw_cancel_object();
#endif
#define HAS_ROTARY_ENCODER 1
