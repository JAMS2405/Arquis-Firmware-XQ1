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

/** ARQUIS UI - pantalla "Autoajuste PID" (Ajustes > PID). Pegamento entre arquis_pid_view y arquis_pid. */

#include "arquis_pid.h"

#if ARQ_HAS_PID
  void arq_pid_draw();      // crea la pantalla (PID_UI); si hay un ajuste en curso o un resultado pendiente, recupera su estado
  void arq_pid_clear();     // la libera (el ajuste sigue en el controlador)
  void arq_pid_poll();      // desde GUI_RefreshPage (tambien durante M303)
#endif
