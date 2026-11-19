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

#include "../inc/MarlinConfig.h"

#if ENABLED(CANCEL_OBJECTS)

#include "cancel_object.h"
#include "../gcode/gcode.h"
#include "../lcd/marlinui.h"

CancelObject cancelable;

int8_t CancelObject::object_count, // = 0
       CancelObject::active_object = -1;
uint32_t CancelObject::canceled; // = 0x0000
bool CancelObject::skipping; // = false
bool CancelObject::overflow; // = false
uint32_t CancelObject::bbox_valid; // = 0x0000
int16_t CancelObject::bbox_x0[ARQ_OBJ_DESIGN_CAPACITY], CancelObject::bbox_y0[ARQ_OBJ_DESIGN_CAPACITY],
        CancelObject::bbox_x1[ARQ_OBJ_DESIGN_CAPACITY], CancelObject::bbox_y1[ARQ_OBJ_DESIGN_CAPACITY];
uint16_t CancelObject::z_print; // = 0

// Llamado desde G0/G1 con extrusion: ensancha el rectangulo del objeto activo
void CancelObject::note_extrusion(const float x, const float y) {
  const int8_t o = active_object;
  if (!in_range(o)) return;
  const int16_t xi = (int16_t)x, yi = (int16_t)y;
  if (!TEST(bbox_valid, o)) {
    SBI(bbox_valid, o);
    bbox_x0[o] = bbox_x1[o] = xi;
    bbox_y0[o] = bbox_y1[o] = yi;
    return;
  }
  if (xi < bbox_x0[o]) bbox_x0[o] = xi; else if (xi > bbox_x1[o]) bbox_x1[o] = xi;
  if (yi < bbox_y0[o]) bbox_y0[o] = yi; else if (yi > bbox_y1[o]) bbox_y1[o] = yi;
}

// Z (0,1 mm) de la ultima extrusion; satura en 0..65535 (6553,5 mm)
void CancelObject::note_z(const float z) {
  const int32_t zi = (int32_t)(z * 10.0f + 0.5f);
  z_print = zi < 0 ? 0 : (zi > 65535 ? 65535 : (uint16_t)zi);
}

// M486 T<n>: n fuera de 0..ARQ_OBJ_DESIGN_CAPACITY se acota y se marca 'overflow'; los indices >= capacidad no seran cancelables
void CancelObject::set_object_count(const int32_t n) {
  if (n < 0 || n > ARQ_OBJ_DESIGN_CAPACITY) overflow = true;
  object_count = n < 0 ? 0 : (n > ARQ_OBJ_DESIGN_CAPACITY ? ARQ_OBJ_DESIGN_CAPACITY : (int8_t)n);
}

// S-1 = no es un objeto (brim, torre...), siempre se imprime. Un indice fuera de rango NO se estrecha ni se enmascara: se trata como
// "no es un objeto" (se imprime) y se marca 'overflow'; asi nunca activa ni cancela OTRO objeto.
void CancelObject::set_active_object(const int32_t obj) {
  if (in_range(obj)) {
    active_object = (int8_t)obj;
    if (obj >= object_count) object_count = (int8_t)(obj + 1);
    skipping = TEST(canceled, obj);
  }
  else {
    if (obj != -1) overflow = true;
    active_object = -1;
    skipping = false;
  }

  #if ALL(HAS_STATUS_MESSAGE, CANCEL_OBJECTS_REPORTING)
    if (active_object >= 0)
      ui.status_printf(0, F(S_FMT " %i"), GET_TEXT(MSG_PRINTING_OBJECT), int(active_object));
    else
      ui.reset_status();
  #endif
}

void CancelObject::cancel_object(const int32_t obj) {
  if (in_range(obj)) {
    SBI(canceled, obj);
    if (obj == active_object) skipping = true;
  }
  else overflow = true;
}

void CancelObject::uncancel_object(const int32_t obj) {
  if (in_range(obj)) {
    CBI(canceled, obj);
    if (obj == active_object) skipping = false;
  }
  else overflow = true;
}

void CancelObject::report() {
  if (active_object >= 0)
    SERIAL_ECHO_MSG("Active Object: ", active_object);

  if (overflow) SERIAL_ECHO_MSG("Object index out of range ignored");

  if (canceled) {
    SERIAL_ECHO_START();
    SERIAL_ECHOPGM("Canceled:");
    for (int i = 0; i < object_count; i++)
      if (TEST(canceled, i)) { SERIAL_CHAR(' '); SERIAL_ECHO(i); }
    SERIAL_EOL();
  }
}

#endif // CANCEL_OBJECTS
