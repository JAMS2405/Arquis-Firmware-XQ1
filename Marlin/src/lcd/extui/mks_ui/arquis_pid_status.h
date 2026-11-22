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
 * ARQUIS UI - estado del autoajuste PID (M303), sin dependencias (lo incluye module/temperature.cpp).
 *
 * Marlin ejecuta M303 en un bucle BLOQUEANTE (Temperature::PID_autotune) que no atiende la pantalla LVGL y solo imprime el
 * resultado por el puerto serie. El parche minimo de temperature.cpp (todo bajo ARQUIS_UI) hace tres cosas:
 *   1) anota aqui el progreso (ciclo) y el resultado (Kp/Ki/Kd) -> la pantalla los muestra y el usuario decide si aplicarlos;
 *   2) llama a arq_pid_idle() en cada vuelta del bucle (atiende LVGL/tactil: Cancelar funciona);
 *   3) nada mas: la cancelacion usa el mecanismo propio de Marlin (wait_for_heatup = false, como M108).
 */

#include <stdint.h>

enum { ARQ_PID_R_NONE = 0, ARQ_PID_R_RUNNING, ARQ_PID_R_DONE, ARQ_PID_R_TOO_HIGH, ARQ_PID_R_TIMEOUT, ARQ_PID_R_CANCEL };

typedef struct {
  volatile uint8_t cycle, total, result;
  float kp, ki, kd;
} arq_pid_status_t;

extern arq_pid_status_t arq_pid_status;
void arq_pid_idle();                                   // atiende LVGL (definida en arquis_pid.cpp)

static inline void arq_pid_begin(const int total)      { arq_pid_status.cycle = 0; arq_pid_status.total = (uint8_t)total; arq_pid_status.kp = arq_pid_status.ki = arq_pid_status.kd = 0; arq_pid_status.result = ARQ_PID_R_RUNNING; }
static inline void arq_pid_cycle(const int c)          { arq_pid_status.cycle = (uint8_t)c; }
static inline void arq_pid_end(const uint8_t r, const float kp, const float ki, const float kd) { arq_pid_status.kp = kp; arq_pid_status.ki = ki; arq_pid_status.kd = kd; arq_pid_status.result = r; }
static inline void arq_pid_finish()                    { if (arq_pid_status.result == ARQ_PID_R_RUNNING) arq_pid_status.result = ARQ_PID_R_CANCEL; }   // salio del bucle sin resultado = cancelado (M108 / wait_for_heatup)
