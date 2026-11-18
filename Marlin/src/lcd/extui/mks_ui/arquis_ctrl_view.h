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

#pragma once

/**
 * ARQUIS UI - vistas de las pantallas de CONTROL (solo LVGL: reciben un modelo, no conocen Marlin):
 * Temperatura, Ventiladores, Mover, Extrusion y Opciones. Todas con cabecera + boton Atras y entre 9 y 12 objetos LVGL (heap de 16 KB).
 * Disposicion comun (480x320): cabecera 44 px | contenido y 52..310.
 */

#include "arquis_widgets.h"

// ---- Temperatura ----------------------------------------------------------------------------------------------------------------
enum { ARQ_TID_BACK = 1, ARQ_TID_HOT, ARQ_TID_BED, ARQ_TID_S1, ARQ_TID_S5, ARQ_TID_S10, ARQ_TID_DEC, ARQ_TID_INC, ARQ_TID_P1, ARQ_TID_P2, ARQ_TID_OFF };
typedef struct {
  int16_t hot_cur, hot_tgt, hot_tol, bed_cur, bed_tgt, bed_tol;   // grados C
  uint8_t sel;                      // 0 = hotend, 1 = cama
  uint8_t step;                     // 1, 5 o 10
  int16_t p1, p2;                   // temperaturas de los preajustes para la seleccion actual
} arq_temps_model_t;
void arq_temps_build(lv_obj_t *scr, lv_event_cb_t cb, bool has_hot, bool has_bed, const char *p1_name, const char *p2_name);
void arq_temps_update(const arq_temps_model_t *m);
void arq_temps_forget();

// ---- Ventiladores ---------------------------------------------------------------------------------------------------------------
enum { ARQ_FID_BACK = 1, ARQ_FID_F0, ARQ_FID_F1, ARQ_FID_DEC, ARQ_FID_INC, ARQ_FID_OFF, ARQ_FID_HALF, ARQ_FID_FULL };
#define ARQ_FANCTL_MAX 2
typedef struct { uint8_t n; uint8_t sel; uint8_t pct[ARQ_FANCTL_MAX]; } arq_fanctl_model_t;
void arq_fanctl_build(lv_obj_t *scr, lv_event_cb_t cb, uint8_t n, const char *const *names);
void arq_fanctl_update(const arq_fanctl_model_t *m);
void arq_fanctl_forget();

// ---- Mover ------------------------------------------------------------------------------------------------------------------------
enum { ARQ_MID_BACK = 1, ARQ_MID_XP, ARQ_MID_XN, ARQ_MID_YP, ARQ_MID_YN, ARQ_MID_ZP, ARQ_MID_ZN, ARQ_MID_S01, ARQ_MID_S1, ARQ_MID_S10 };
typedef struct {
  int32_t pos_cmm[3];               // X, Y, Z en centesimas de mm
  uint8_t homed;                    // bit0 X, bit1 Y, bit2 Z referenciados
  uint8_t step;                     // 0 = 0,1 mm; 1 = 1 mm; 2 = 10 mm   (babystep: 0,01 / 0,05 / 0,1 mm)
  int32_t z_off_um;                 // babystep: offset Z de la sonda en micras (mm * 1000)
} arq_move_model_t;
// baby = true: pantalla de babystep (mismos botones; muestra el offset Z y los pasos finos)
void arq_move_build(lv_obj_t *scr, lv_event_cb_t cb, bool baby = false);
void arq_move_update(const arq_move_model_t *m);
void arq_move_forget();

// ---- Extrusion --------------------------------------------------------------------------------------------------------------------
enum { ARQ_EID_BACK = 1, ARQ_EID_TYPE, ARQ_EID_TEMP, ARQ_EID_IN, ARQ_EID_OUT, ARQ_EID_STEP, ARQ_EID_SPEED };
typedef struct {
  int16_t cur, tgt, tol, min_temp;  // grados C
  bool    hot_enough;               // Marlin permite extruir (hotEnoughToExtrude)
  uint8_t ext_idx;                  // extrusor activo (0 o 1)
  int32_t amount;                   // mm extruidos en esta pantalla
  uint8_t step_mm;                  // paso (mm)
  uint8_t speed;                    // 0 = lenta, 1 = normal, 2 = rapida
} arq_extrude_model_t;
void arq_extrude_build(lv_obj_t *scr, lv_event_cb_t cb, bool multi_extruder);
void arq_extrude_update(const arq_extrude_model_t *m);
void arq_extrude_forget();

// ---- Opciones (rejilla de accesos 4 x 2) -----------------------------------------------------------------------------------------
enum { ARQ_XID_BACK = 1, ARQ_XID_ITEM0 = 10 };                 // los accesos usan ARQ_XID_ITEM0 + indice
#define ARQ_OPT_MAX 8
typedef struct { const lv_img_dsc_t *icon; const char *title; const char *sub; uint8_t kind; } arq_opt_item_t;   // sub: texto fijo del llamador o nullptr
void arq_opts_build(lv_obj_t *scr, lv_event_cb_t cb, const char *title, const arq_opt_item_t *items, uint8_t n);
void arq_opts_set(uint8_t index, const char *sub, uint8_t kind);   // cambia el subtitulo y el tipo (p. ej. interruptor activado)
void arq_opts_forget();
