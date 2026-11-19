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

#include <stdint.h>

// Arquis: capacidad de diseno (30 objetos, IDs 0..29) = UNICA fuente: shared/object_protocol/limits.json (ver gen_limits.py)
#include "../../../shared/object_protocol/generated/arquis_object_limits.h"

class CancelObject {
public:
  static bool skipping;
  static int8_t object_count, active_object;
  static uint32_t canceled;
  // Arquis: se recibio un indice (S/P/U) o un total (T) fuera de 0..ARQ_OBJ_MAX_ID: se ignoro de forma segura (no se cancela NINGUN objeto)
  static bool overflow;
  static int8_t declared_count;                              // Arquis: valor del ultimo M486 T<n> (acotado a 0..capacidad); -1 si no hubo
  // Arquis: bounding box XY (mm) de cada objeto, aprendida mientras se imprime (para el mapa de la pantalla)
  static uint32_t bbox_valid;
  static int16_t bbox_x0[ARQ_OBJ_DESIGN_CAPACITY], bbox_y0[ARQ_OBJ_DESIGN_CAPACITY], bbox_x1[ARQ_OBJ_DESIGN_CAPACITY], bbox_y1[ARQ_OBJ_DESIGN_CAPACITY];
  static uint16_t z_print;                                  // Arquis: Z (0,1 mm) de la ultima extrusion con XY (estados Pendiente/Imprimiendo/Completado)
  static void note_extrusion(const float x, const float y);
  static void note_z(const float z);
  // Los indices llegan como int32_t (parser.value_long()): estrechar a int8_t antes de validar haria que S256 activase el objeto 0
  static bool in_range(const int32_t obj) { return obj >= 0 && obj < ARQ_OBJ_DESIGN_CAPACITY; }
  static bool has_bbox(const int32_t obj) { return in_range(obj) && TEST(bbox_valid, obj); }
  static void set_object_count(const int32_t n);
  static void set_active_object(const int32_t obj);
  static void cancel_object(const int32_t obj);
  static void uncancel_object(const int32_t obj);
  static void report();
  static bool is_canceled(const int32_t obj) { return in_range(obj) && TEST(canceled, obj); }
  static void clear_active_object() { set_active_object(-1); }
  static void cancel_active_object() { if (active_object >= 0) cancel_object(active_object); }
  static void reset() { canceled = 0x0000; bbox_valid = 0x0000; object_count = 0; declared_count = -1; overflow = false; z_print = 0; clear_active_object(); }
};

extern CancelObject cancelable;
