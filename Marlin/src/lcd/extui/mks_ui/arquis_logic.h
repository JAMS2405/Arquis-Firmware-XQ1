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
 * ARQUIS UI - logica pura (sin LVGL ni Marlin): estados y formatos.
 * Separada a proposito para poder probarla en el PC con todos los casos.
 */

#include <stdint.h>
#include <stddef.h>

#define ARQ_SAFE_TEMP     50    // °C: por encima, un calentador apagado se considera "enfriando"
#define ARQ_PROGRESS_MIN   2    // %  : minimo de avance para estimar el tiempo restante
#define ARQ_ELAPSED_MIN   60    // s  : minimo de tiempo para estimar el tiempo restante

typedef enum { ARQ_TEMP_OFF, ARQ_TEMP_HEATING, ARQ_TEMP_READY, ARQ_TEMP_COOLING } arq_temp_state_t;

typedef enum { ARQ_PS_IDLE, ARQ_PS_HEATING, ARQ_PS_AT_TEMP, ARQ_PS_COOLING, ARQ_PS_PRINTING, ARQ_PS_PAUSED, ARQ_PS_ERROR } arq_pstate_t;

// Estado de un calentador. tol = histeresis de Marlin (TEMP_HYSTERESIS / TEMP_BED_HYSTERESIS).
arq_temp_state_t arq_temp_state(int16_t cur, int16_t target, int16_t tol);

// Estado global de la impresora. Prioridad: error > pausada > imprimiendo > calentando > temperatura alcanzada > enfriando > lista.
// n_heaters/cur/tgt/tol: calentadores presentes (hotend, cama...).
arq_pstate_t arq_printer_state(bool stopped, bool paused, bool printing, uint8_t n_heaters, const int16_t *cur, const int16_t *tgt, const int16_t *tol);

// Tiempo restante estimado con la MISMA formula de Marlin (_calculated_remaining_time): elapsed * (100 - p) / p.
// Devuelve 0 (= desconocido) si el avance o el tiempo son insuficientes. Nunca inventa un valor.
uint32_t arq_remaining_s(uint32_t elapsed_s, uint8_t percent);

// "H:MM" (las horas pueden pasar de 9). Devuelve la longitud escrita.
size_t arq_fmt_hm(char *buf, size_t n, uint32_t seconds);

// Copia segura y siempre terminada en NUL (no depende de que la libc traiga strlcpy).
size_t arq_strlcpy(char *dst, const char *src, size_t n);
