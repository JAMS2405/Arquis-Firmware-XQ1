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

#include "../../../inc/MarlinConfig.h"

#if ENABLED(CANCEL_OBJECTS)

#include "../../gcode.h"
#include "../../../feature/cancel_object.h"

/**
 * M486: A simple interface to cancel objects
 *
 *   T[count] : Reset objects and/or set the count
 *   S<index> : Start an object with the given index
 *   P<index> : Cancel the object with the given index
 *   U<index> : Un-cancel object with the given index
 *   C        : Cancel the current object (the last index given by S<index>)
 *   S-1      : Start a non-object like a brim or purge tower that should always print
 */
void GcodeSuite::M486() {

  // Arquis: los indices se leen como int32_t (value_long) y se validan ANTES de estrechar: con value_int()/int8_t, S256 activaba el
  // objeto 0 y P256 lo cancelaba. Fuera de 0..29 no se hace nada salvo marcar 'overflow' (ver cancel_object.cpp).
  if (parser.seen('T')) {
    cancelable.reset();
    cancelable.set_object_count(parser.longval('T', 1));
  }

  if (parser.seenval('S'))
    cancelable.set_active_object(parser.value_long());

  if (parser.seen('C')) cancelable.cancel_active_object();

  if (parser.seenval('P')) cancelable.cancel_object(parser.value_long());

  if (parser.seenval('U')) cancelable.uncancel_object(parser.value_long());
}

#endif // CANCEL_OBJECTS
