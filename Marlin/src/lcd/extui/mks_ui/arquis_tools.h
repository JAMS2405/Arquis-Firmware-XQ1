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
 * ARQUIS UI - pantallas de Etapa 5 (union con Marlin): Herramientas, Home, Ajustes, Sistema y diagnostico, Babystep, Velocidad y Nivelar cama.
 * Las llamadas que actuan sobre la maquina son EXACTAMENTE las de las pantallas originales de MKS (draw_tool/home/set/baby_stepping/
 * change_speed/manuaLevel/about.cpp). Solo se anaden confirmaciones en los accesos que mueven la maquina de forma amplia o la apagan.
 */

void arq_tools_draw();    void arq_tools_clear();
void arq_home_draw();     void arq_home_clear();
void arq_settings_draw(); void arq_settings_clear();
void arq_about_draw();    void arq_about_clear();
void arq_baby_draw();     void arq_baby_clear();    void arq_baby_refresh();
void arq_speed_draw();    void arq_speed_clear();
void arq_level_draw();    void arq_level_clear();
void arq_more_draw();     void arq_more_clear();
void arq_language_draw(); void arq_language_clear();
