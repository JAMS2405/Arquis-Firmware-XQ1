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

#include "arquis_files.h"
#include "arquis_files_view.h"
#include "draw_ui.h"
#include "tft_lvgl_configuration.h"      // public_buf, bmp_public_buf
#include "SPI_TFT.h"
#include "../../../sd/cardreader.h"

extern lv_group_t *g;
bool have_pre_pic(char *path);      // draw_print_file.cpp (comprobacion original: el archivo tiene ";simage:" en su inicio)

static lv_event_cb_t ok_handler, cancel_handler;

// ---- Miniatura (solo lectura) ---------------------------------------------------------------------------------------------------
// Mismo formato y misma lectura que gcode_preview() de draw_ui.cpp (";gimage:", 200 filas de 809 bytes, RGB565 en hexadecimal), pero:
//  - NO escribe en la flash SPI ni inicia la impresion (eso lo hace la interfaz original al pulsar Imprimir);
//  - cierra el archivo despues de cada fila (un archivo abierto haria que Marlin lo trate como "impresion pausada").
#if HAS_GCODE_PREVIEW
  #define THUMB_ROWS     200
  #define THUMB_LINE     809UL      // bytes por fila en el G-code (el mismo valor fijo que usa la interfaz original)
  #define ROWS_PER_POLL  2

  static bool     thumb_active, thumb_found;
  static uint16_t thumb_row;
  static uint32_t thumb_start;

  static void thumb_stop() { thumb_active = false; }

  // Busca ";gimage:" avanzando 392 bytes por bloque de 400 (el solape evita perder la marca si cae entre dos bloques).
  static bool thumb_locate() {
    uint32_t pos = 0;
    for (uint16_t guard = 0; guard < 400; ++guard) {                  // 400 * 392 B = 156 KB como maximo
      card.setIndex(pos);
      const int br = card.read(public_buf, 400);
      if (br <= 0) return false;
      public_buf[br < 512 ? br : 512] = 0;
      const char *p1 = strstr((char *)public_buf, ";gimage:");
      if (p1) { thumb_start = pos + (uint32_t)(p1 - (const char *)public_buf); return true; }
      if (br < 400) return false;
      pos += 392;
    }
    return false;
  }

  static void thumb_draw_row() {
    char *cur_name = strrchr(list_file.file_name[sel_id], '/');
    card.openFileRead(cur_name);
    if (!card.isFileOpen()) { thumb_stop(); return; }
    if (!thumb_found) {
      thumb_found = thumb_locate();
      if (!thumb_found) { card.closefile(); thumb_stop(); return; }
    }
    card.setIndex(thumb_start + THUMB_LINE * thumb_row + 8);
    uint16_t j = 0;
    for (uint8_t k = 0; k < 2; ++k) {                                 // 2 lecturas de 400 caracteres = 400 bytes = 200 pixeles
      card.read(public_buf, 400);
      for (uint16_t i = 0; i < 400; i += 2, ++j)
        bmp_public_buf[j] = ascii2dec_test((char *)&public_buf[i]) << 4 | ascii2dec_test((char *)&public_buf[i + 1]);
    }
    card.closefile();
    uint16_t *px = (uint16_t *)bmp_public_buf;
    for (uint16_t i = 0; i < 200; ++i) if (px[i] == 0x0000) px[i] = ARQ_COL_BG.full;   // transparente -> fondo de la pantalla
    SPI_TFT.setWindow(ARQ_CONFIRM_THUMB_X, ARQ_CONFIRM_THUMB_Y + thumb_row, ARQ_CONFIRM_THUMB_S, 1);
    SPI_TFT.tftio.WriteSequence(px, 200);
    if (++thumb_row >= THUMB_ROWS) thumb_stop();
  }

  void arq_files_poll() {
    if (!thumb_active) return;
    // Solo mientras esta en pantalla la confirmacion: al pulsar Imprimir o Cancelar el estado cambia y se detiene
    if (disp_state != DIALOG_UI || uiCfg.dialogType != DIALOG_TYPE_PRINT_FILE) { thumb_stop(); return; }
    for (uint8_t n = 0; n < ROWS_PER_POLL && thumb_active; ++n) thumb_draw_row();
  }
#else
  void arq_files_poll() {}
#endif

static void confirm_cb(lv_obj_t *obj, lv_event_t event) {
  if (event != LV_EVENT_RELEASED) return;
  if (obj->mks_obj_id == ARQ_PC_OK) { if (ok_handler) ok_handler(obj, event); }
  else if (cancel_handler) cancel_handler(obj, event);
}

void arq_print_confirm_draw(lv_obj_t *scr, const char *file_name, lv_event_cb_t ok_cb, lv_event_cb_t cancel_cb) {
  ok_handler = ok_cb; cancel_handler = cancel_cb;
  arq_screen_apply(scr);
  bool has_thumb = false;
  #if HAS_GCODE_PREVIEW
    thumb_stop();
    has_thumb = have_pre_pic(list_file.file_name[sel_id]);          // comprobacion original del explorador de MKS (lee 1 KB del archivo)
  #endif
  arq_confirm_build(scr, confirm_cb, file_name, has_thumb);
  #if HAS_ROTARY_ENCODER
    if (gCfgItems.encoder_enable) arq_group_add_clickables(g, scr);
  #endif
  #if HAS_GCODE_PREVIEW
    thumb_row = 0; thumb_found = false; thumb_active = has_thumb;
  #endif
}

#endif // HAS_TFT_LVGL_UI && ARQUIS_UI
