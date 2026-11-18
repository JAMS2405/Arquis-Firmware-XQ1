// STUB/arnes: declaraciones minimas para comprobar la sintaxis de los bloques ARQUIS de draw_print_file.cpp contra las firmas reales
#pragma once
#include <string.h>
#include "draw_ui.h"
#include "print_file_types.h"    // LIST_FILE, DIR_OFFSET y FILE_NUM extraidos de draw_print_file.h REAL
#include "../../../sd/cardreader.h"
#include "arquis_files_view.h"
#include "arquis_widgets.h"
extern int8_t curDirLever; extern uint8_t sel_id;
extern lv_group_t *g;
static lv_obj_t *scr;
static uint8_t search_file() { return 0; }
