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
 * ARQUIS UI - pantalla "Rendimiento": CONTROLADOR (pegamento con Marlin; sin LVGL).
 * Cada pulsacion de - / + cambia UN valor y lo aplica al instante con la orden de Marlin correspondiente (M204, M201, M900, M205, M593)
 * mediante gcode.process_subcommands_now (como el PID al aplicar): Marlin valida y el controlador comprueba que lo tomo.
 * Los cambios viven en RAM hasta que el usuario pulsa Guardar (settings.save = M500). "Restaurar" vuelve a los valores guardados / del primer uso.
 * Solo se edita con la impresora quieta: sin impresion, sin movimientos pendientes, sin nivelacion ni autoajuste PID en curso.
 */

#include "arquis_perf_logic.h"

#include "../../../inc/MarlinConfigPre.h"

#if HAS_TFT_LVGL_UI && ENABLED(ARQUIS_UI)
  #define ARQ_HAS_PERF 1
#else
  #define ARQ_HAS_PERF 0
#endif

#if ARQ_HAS_PERF

typedef struct {
  bool    has[ARQ_PF_COUNT];       // valor disponible en este firmware
  float   val[ARQ_PF_COUNT];       // lo que Marlin usa ahora
  float   ref[ARQ_PF_COUNT];       // referencia: guardado en la EEPROM (o valor al abrir por primera vez)
  float   shaping_min;             // frecuencia minima de Input Shaping admitida
  uint8_t sel;                     // valor seleccionado
  bool    coarse;                  // paso grueso
  bool    dirty;                   // algun valor difiere de la referencia
  uint8_t why;                     // 0 = se puede editar; si no, arq_pf_err_t
} arq_perf_info_t;

void    arq_perf_ctl_info(arq_perf_info_t *out);
void    arq_perf_ctl_select(uint8_t id);
void    arq_perf_ctl_toggle_step();
uint8_t arq_perf_ctl_delta(int8_t dir);        // aplica un paso al valor seleccionado. ARQ_PF_OK, o el motivo (no se cambio nada)
uint8_t arq_perf_ctl_save();                   // M500 y comprobacion; en exito la referencia pasa a ser lo guardado
uint8_t arq_perf_ctl_restore();                // vuelve a la referencia (aplica los valores que difieren)

#endif
