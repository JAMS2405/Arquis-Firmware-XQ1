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

#include "arquis_objects_sd.h"
#include "arquis_objects.h"
#include "draw_ui.h"
#include "tft_lvgl_configuration.h"      // public_buf
#include "../../../sd/cardreader.h"
#include "../../../MarlinCore.h"

#include <string.h>

extern uint8_t sel_id;      // la define draw_print_file.cpp (igual que en arquis_files.cpp)

enum : uint8_t { LD_IDLE = 0, LD_HEAD, LD_TAIL };
static uint8_t  stage;
static uint32_t tail_pos, tail_end, file_size, file_id;

#define BED_W_DMM  ((uint16_t)((X_BED_SIZE) * 10 + 0.5f))
#define BED_D_DMM  ((uint16_t)((Y_BED_SIZE) * 10 + 0.5f))

// Con un archivo abierto o una impresion en curso NO se abre nada: openFileRead() llama a abortFilePrintNow()
static bool sd_busy() { return !card.isMounted() || card.isFileOpen() || card.isPrinting() || printingIsActive() || printingIsPaused(); }

static void give_up() { stage = LD_IDLE; arq_man_reset(); }

// Un paso: abre, lee un trozo, cierra. true = hay mas pasos
static bool step() {
  if (sd_busy()) { give_up(); return false; }
  const char *cur = strrchr(list_file.file_name[sel_id], '/');
  if (!cur) { give_up(); return false; }
  card.openFileRead(cur);
  if (!card.isFileOpen()) { give_up(); return false; }
  bool more = true;
  if (stage == LD_HEAD) {
    file_size = card.getFileSize();
    file_id = arq_file_id(card.longest_filename(), file_size);
    const int br = card.read(public_buf, ARQ_MAN_HEAD_WINDOW);
    if (br < 0) { card.closefile(); give_up(); return false; }
    arq_man_head(public_buf, (uint16_t)br);
    arq_man_tail_begin();
    tail_end = file_size;
    tail_pos = file_size > ARQ_MAN_TAIL_WINDOW ? file_size - ARQ_MAN_TAIL_WINDOW : 0;
    stage = LD_TAIL;
  }
  else {
    if (card.getFileSize() != file_size) { card.closefile(); give_up(); return false; }    // el archivo cambio entre pasos: no se fia
    card.setIndex(tail_pos);
    const uint32_t want = tail_end - tail_pos < 512UL ? tail_end - tail_pos : 512UL;
    const int br = want ? card.read(public_buf, (uint16_t)want) : 0;
    if (br < 0 || (want && br == 0)) { card.closefile(); give_up(); return false; }
    arq_man_tail_feed(public_buf, (uint16_t)br);
    tail_pos += (uint32_t)br;
    if (tail_pos >= tail_end) {
      arq_man_tail_finish(BED_W_DMM, BED_D_DMM);
      if (arq_man_status() == ARQ_MAN_OK) arq_obj.file_id = file_id;
      stage = LD_IDLE;
      more = false;
    }
  }
  card.closefile();
  return more;
}

void arq_objload_start() {
  arq_man_reset();
  stage = sd_busy() ? LD_IDLE : LD_HEAD;
}

void arq_objload_poll() {
  if (stage == LD_IDLE) return;
  // Solo mientras esta en pantalla la confirmacion de "Imprimir archivo" (igual que la miniatura)
  if (disp_state != DIALOG_UI || uiCfg.dialogType != DIALOG_TYPE_PRINT_FILE) { arq_objload_stop(); return; }
  for (uint8_t n = 0; n < 2 && stage != LD_IDLE; ++n) if (!step()) break;
}

void arq_objload_finish() {
  for (uint8_t guard = 0; stage != LD_IDLE && guard < 40; ++guard) if (!step()) break;
  if (stage != LD_IDLE) give_up();          // no deberia pasar: sin manifiesto antes que un manifiesto a medias
}

void arq_objload_stop() { if (stage != LD_IDLE) give_up(); }
bool arq_objload_busy() { return stage != LD_IDLE; }

uint32_t arq_objload_printing_file_id() {
  return card.isFileOpen() ? arq_file_id(card.longest_filename(), card.getFileSize()) : 0;
}

#endif // HAS_TFT_LVGL_UI && ARQUIS_UI
