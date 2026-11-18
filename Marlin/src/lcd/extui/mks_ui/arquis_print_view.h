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
 * ARQUIS UI - vista de la supervision de impresion (solo LVGL: recibe un snapshot, no conoce Marlin).
 *
 * Distribucion (480x320). La zona x 2..202, y 36..236 queda LIBRE: ahi la interfaz original de MKS pinta la miniatura del G-code
 * directamente en la pantalla (y la "llena" segun el avance). Ningun objeto LVGL debe cubrirla.
 *   cabecera 36 px: marca + nombre | estado | boton Inicio
 *   derecha: tarjeta de avance (archivo, barra, %, tiempos) y lista de datos (hotend, cama, ventilador, Z)
 *   abajo: Pausar/Reanudar | Cancelar | Objetos | Opciones
 * 9 objetos LVGL como maximo (~3,4 KB de heap).
 */

#include "arquis_widgets.h"
#include "arquis_data.h"

enum { ARQ_PID_HOME = 1, ARQ_PID_PAUSE, ARQ_PID_STOP, ARQ_PID_OBJECTS, ARQ_PID_OPTIONS };

// Estado de los controles (lo decide arquis_print.cpp a partir de la maquina de estados original de MKS)
typedef enum {
  ARQ_PM_PREPARING,   // cargando la miniatura: la interfaz original ignora todos los toques
  ARQ_PM_RUNNING,     // imprimiendo: se puede pausar
  ARQ_PM_PAUSING,     // pausa en curso: boton deshabilitado
  ARQ_PM_PAUSED,      // pausada: se puede reanudar
  ARQ_PM_RESUMING,    // reanudando: boton deshabilitado
  ARQ_PM_RECOVER,     // impresion interrumpida (recuperacion tras un corte de energia): se puede reanudar
  ARQ_PM_NOCTRL       // trabajo que la interfaz no controla (p. ej. iniciado desde un host): sin boton de pausa
} arq_pmode_t;

#define ARQ_PRINT_HEADER_H 36

void arq_print_build(lv_obj_t *scr, lv_event_cb_t cb, const char *machine_name, const arq_snapshot_t *first, arq_pmode_t mode, bool with_objects);
void arq_print_update(const arq_snapshot_t *s, arq_pmode_t mode);   // no hace nada si nada cambio
void arq_print_forget();                                            // al liberar la pantalla
