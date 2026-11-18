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
 * ARQUIS UI - pantallas de control (union con Marlin): Temperatura, Ventiladores, Mover, Extrusion y Opciones.
 *
 * Las llamadas que actuan sobre la maquina son EXACTAMENTE las de las pantallas originales de MKS (draw_preHeat.cpp, draw_fan.cpp,
 * draw_move_motor.cpp, draw_extrusion.cpp, draw_operation.cpp), con los mismos limites; solo cambia como se dibuja.
 */

void arq_temps_draw();   void arq_temps_clear();   void arq_temps_refresh();
void arq_fan_draw();     void arq_fan_clear();     void arq_fan_refresh();
void arq_move_draw();    void arq_move_clear();
void arq_extrude_draw(); void arq_extrude_clear(); void arq_extrude_refresh();
void arq_options_draw(); void arq_options_clear();
