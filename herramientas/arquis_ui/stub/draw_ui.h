// STUB de draw_ui.h: solo lo que usan arquis_nav.cpp y arquis_dashboard.cpp (firmas reales de draw_ui.h / draw_*.h)
#pragma once
#include <lvgl.h>
#include "../../../inc/MarlinConfigPre.h"
typedef enum { MAIN_UI, PRINT_READY_UI } DISP_STATE;
typedef struct { DISP_STATE _disp_state[100]; int _disp_index; } DISP_STATE_STACK;
extern DISP_STATE_STACK disp_state_stack;
typedef struct { uint8_t curTempType; } UI_CFG; extern UI_CFG uiCfg;
typedef struct { bool from_flash_pic; bool encoder_enable; } GCFG; extern GCFG gCfgItems;
lv_obj_t* lv_screen_create(DISP_STATE newScreenType, const char *title = nullptr);
void lv_draw_print_file(); void lv_draw_printing(); void lv_draw_move_motor(); void lv_draw_preHeat(); void lv_draw_fan();
void lv_draw_tool(); void lv_draw_set();
#define HAS_ROTARY_ENCODER 1
