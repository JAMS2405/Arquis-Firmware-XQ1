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

#include "../../../inc/MarlinConfig.h"
#include "arquis_print.h"
#include "arquis_print_view.h"
#include "arquis_widgets.h"
#include "arquis_data.h"
#include "draw_ui.h"

#include "../../../MarlinCore.h"
#include "../../../module/printcounter.h"
#include "../../../sd/cardreader.h"
#include "../../../gcode/queue.h"
#include "../../../gcode/gcode.h"

#if ENABLED(POWER_LOSS_RECOVERY)
  #include "../../../feature/powerloss.h"
#endif

#include <stdio.h>
#include <string.h>

extern lv_group_t *g;
extern bool once_flag, flash_preview_begin, default_preview_flg, gcode_preview_over;

static lv_obj_t *scr;
static bool      finish_latched;
static char      last_file[44], fin_msg[96];

enum { DLG_STOP_OK = 201, DLG_STOP_NO, DLG_FINISH_OK };

static arq_pmode_t mode_now() {
  if (gcode_preview_over) return ARQ_PM_PREPARING;           // la interfaz original ignora todos los toques mientras carga la miniatura
  switch (uiCfg.print_state) {
    case WORKING:    return ARQ_PM_RUNNING;
    case PAUSING:    return ARQ_PM_PAUSING;
    case PAUSED:     return ARQ_PM_PAUSED;
    case RESUMING:
    case REPRINTED:  return ARQ_PM_RESUMING;
    case REPRINTING: return ARQ_PM_RECOVER;
    default:         return ARQ_PM_NOCTRL;                   // p. ej. impresion iniciada desde un host: la pausa no es de esta pantalla
  }
}

// La miniatura se pinta directamente en la pantalla (no con LVGL). Si un dialogo la tapa, hay que pedir que se repinte,
// igual que hace draw_return_ui() al volver a esta pantalla.
static void restore_preview() {
  if (gCfgItems.from_flash_pic) flash_preview_begin = true; else default_preview_flg = true;
}

static void dialog_cb(lv_obj_t *obj, lv_event_t event) {
  if (event != LV_EVENT_RELEASED) return;
  switch (obj->mks_obj_id) {
    case DLG_STOP_NO:
      arq_dialog_close();
      restore_preview();
      break;
    case DLG_STOP_OK:     // misma secuencia que el boton OK de DIALOG_TYPE_STOP (draw_dialog.cpp)
      wait_for_heatup = false;
      stop_print_time();
      arq_print_clear();
      lv_draw_ready_print();
      #if HAS_MEDIA
        uiCfg.print_state = IDLE;
        card.abortFilePrintSoon();
      #endif
      break;
    case DLG_FINISH_OK:   // igual que el OK de DIALOG_TYPE_FINISH_PRINT
      clear_cur_ui();
      lv_draw_ready_print();
      break;
  }
}

// Pausa / reanudar: las mismas llamadas que ID_PAUSE en draw_printing.cpp. printer_state_polling() hace el resto.
static void toggle_pause() {
  if (uiCfg.print_state == WORKING) {
    #if HAS_MEDIA
      card.pauseSDPrint();
      stop_print_time();
      uiCfg.print_state = PAUSING;
    #endif
  }
  else if (uiCfg.print_state == PAUSED)
    uiCfg.print_state = RESUMING;
  #if ENABLED(POWER_LOSS_RECOVERY)
    else if (uiCfg.print_state == REPRINTING) {
      uiCfg.print_state = REPRINTED;
      print_time.minutes = recovery.info.print_job_elapsed / 60;
      print_time.seconds = recovery.info.print_job_elapsed % 60;
      print_time.hours   = print_time.minutes / 60;
    }
  #endif
}

static void screen_cb(lv_obj_t *obj, lv_event_t event) {
  if (event != LV_EVENT_RELEASED) return;
  if (gcode_preview_over) return;                                    // como la pantalla original: sin toques mientras se carga la miniatura
  switch (obj->mks_obj_id) {
    case ARQ_PID_HOME:                                               // SOLO cambia de pantalla: la impresion sigue su curso
      arq_print_clear();
      lv_draw_ready_print();
      break;
    case ARQ_PID_PAUSE:
      toggle_pause();
      arq_print_refresh();
      break;
    case ARQ_PID_STOP:                                               // cancelar SIEMPRE pide confirmacion
      if (!arq_dialog_open("Cancelar impresi\xC3\xB3n", "Se detendr\xC3\xA1 la impresi\xC3\xB3n y se perder\xC3\xA1 el avance. Esta acci\xC3\xB3n no se puede deshacer.",
                           "Cancelar impresi\xC3\xB3n", "Seguir imprimiendo", true, dialog_cb, DLG_STOP_OK, DLG_STOP_NO)) {
        // sin memoria para el dialogo: no se cancela nada
      }
      break;
    #if ENABLED(CANCEL_OBJECTS)
      case ARQ_PID_OBJECTS: arq_print_clear(); lv_draw_cancel_object(); break;
    #endif
    case ARQ_PID_OPTIONS:
      arq_print_clear();
      lv_draw_operation();
      break;
  }
}

void arq_print_draw() {
  disp_state_stack._disp_index = 0;
  ZERO(disp_state_stack._disp_state);
  scr = lv_screen_create(PRINTING_UI, "");
  arq_screen_apply(scr);
  arq_snapshot_t s;
  arq_data_read(&s);
  arq_print_build(scr, screen_cb, MACHINE_NAME, &s, mode_now(), ENABLED(CANCEL_OBJECTS));
  arq_print_update(&s, mode_now());
  #if HAS_ROTARY_ENCODER
    if (gCfgItems.encoder_enable) arq_group_add_clickables(g, scr);
  #endif
}

void arq_print_refresh() {
  if (!scr) return;
  arq_snapshot_t s;
  arq_data_read(&s);
  arq_print_update(&s, mode_now());
}

void arq_print_clear() {
  #if HAS_ROTARY_ENCODER
    if (gCfgItems.encoder_enable) lv_group_remove_all_objs(g);
  #endif
  arq_dialog_close();                                // un dialogo en la capa superior sobreviviria al cambio de pantalla
  arq_print_forget();
  if (scr) { lv_obj_del(scr); scr = nullptr; }
}

// ---- Fin de impresion (C3 de la auditoria) -------------------------------------------------------------------------------------
// La interfaz original solo detecta el fin de impresion si la pantalla activa es la de supervision. Con el boton Inicio esa
// suposicion deja de cumplirse: aqui se detecta desde cualquier pantalla y, si no es una pantalla segura, se aplaza hasta volver
// al Dashboard o a la supervision. Las acciones son las mismas que las originales (setProBarRate en draw_printing.cpp).
static void show_finish() {
  stop_print_time();
  flash_preview_begin = default_preview_flg = false;
  once_flag = true;
  char t[12];
  arq_fmt_hm(t, sizeof(t), print_job_timer.duration());
  if (last_file[0]) snprintf(fin_msg, sizeof(fin_msg), "%s\nTiempo total: %s", last_file, t);
  else              snprintf(fin_msg, sizeof(fin_msg), "Tiempo total: %s", t);
  if (!arq_dialog_open("Impresi\xC3\xB3n finalizada", fin_msg, "Aceptar", nullptr, false, dialog_cb, DLG_FINISH_OK, 0)) {
    clear_cur_ui();                                  // sin memoria para el dialogo: ir directamente al Dashboard
    lv_draw_ready_print();
  }
  #if HAS_SUICIDE
    if (gCfgItems.finish_power_off) {
      gcode.process_subcommands_now(F("M1001"));
      queue.inject(F("M81"));
      marlin_state = MF_RUNNING;
    }
  #endif
}

bool arq_print_dialog_open() { return arq_dialog_is_open(); }

void arq_print_poll() {
  #if HAS_MEDIA
    if (card.isFileOpen()) {                          // recordar el nombre: al terminar el archivo ya esta cerrado
      const char *n = card.longest_filename();
      if (n && strncmp(last_file, n, sizeof(last_file) - 1)) arq_strlcpy(last_file, n, sizeof(last_file));
    }
    // marlin_state == MF_SD_COMPLETE dura solo hasta que Marlin encola M1001: hay que anotarlo en cuanto aparece, en cualquier pantalla.
    // Si esa ventana se pierde, el respaldo de arq_finish_detect() lo detecta igualmente (ver arquis_logic.h).
    static arq_finish_state_t fin;
    const arq_finish_in_t in = { uiCfg.print_state == WORKING, gcode_preview_over, once_flag,
                                 marlin_state == MF_SD_COMPLETE || card.flag.sdprintdone, arq_data_job_active(), card.isFileOpen(), (uint32_t)millis() };
    if (!finish_latched && arq_finish_detect(&fin, &in)) {
      finish_latched = true;
      stop_print_time();
    }
  #endif
  if (!finish_latched) return;
  // Descartar solo si ya se atendio o si EMPEZO otra impresion. (Antes se descartaba con arq_data_job_active(): al terminar el archivo
  // el trabajo sigue "activo" hasta que Marlin ejecuta M1001, asi que el aviso de fin se perdia siempre que los movimientos finales tardaban.)
  #if HAS_MEDIA
    if (once_flag || arq_finish_new_job(card.isFileOpen(), card.flag.sdprintdone)) { finish_latched = false; return; }
  #else
    if (once_flag) { finish_latched = false; return; }
  #endif
  if (gcode_preview_over || arq_dialog_is_open()) return;
  if (disp_state == PRINTING_UI || disp_state == PRINT_READY_UI) { finish_latched = false; show_finish(); }
}

#endif // HAS_TFT_LVGL_UI && ARQUIS_UI
