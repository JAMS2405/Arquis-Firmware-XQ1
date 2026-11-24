// STUB de draw_ui.h para machine_sim: firmas reales de draw_ui.h (Marlin 2.1.2.10 MKS) que usa arquis_machine_ui.cpp
#pragma once
#include <lvgl.h>
#include "../../../inc/MarlinConfigPre.h"
#define HAS_ROTARY_ENCODER 1
typedef enum { PRINT_READY_UI, SET_UI, MACHINE_PARA_UI, ARQ_MACHINE_UI } DISP_STATE;
typedef struct { DISP_STATE _disp_state[100]; int _disp_index; } DISP_STATE_STACK;
extern DISP_STATE_STACK disp_state_stack;
struct xy_int_stub { int16_t x, y; };
typedef struct { xy_int_stub trammingPos[5]; bool encoder_enable; } GCFG; extern GCFG gCfgItems;
lv_obj_t* lv_screen_create(DISP_STATE newScreenType, const char *title = nullptr);
void lv_draw_set(); void lv_draw_ready_print(); void lv_draw_machine_para(); void update_spi_flash();
