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
 * ARQUIS UI - pantalla "Rendimiento": LOGICA PURA (sin LVGL ni Marlin; se prueba en el PC).
 * Ocho valores de movimiento que se ajustan desde la TS35 sin recompilar: aceleraciones, Linear Advance, Junction Deviation e Input Shaping.
 * Cada uno tiene limites propios, dos tamanos de paso (fino / grueso) y la orden de Marlin que lo aplica (M204, M201, M900, M205, M593).
 * Los limites replican las comprobaciones de Marlin 2.1.2.10 (ver los comentarios) para no enviarle nunca un valor que rechace.
 */

#include <stdint.h>
#include <stddef.h>

typedef enum {
  ARQ_PF_ACC_P,      // M204 P   aceleracion al imprimir
  ARQ_PF_ACC_T,      // M204 T   aceleracion en vacio (movimientos sin extruir)
  ARQ_PF_AMAX_X,     // M201 X   aceleracion maxima del eje X (techo de las dos anteriores)
  ARQ_PF_AMAX_Y,     // M201 Y
  ARQ_PF_K,          // M900 K   Linear Advance (0 = desactivado)
  ARQ_PF_JD,         // M205 J   Junction Deviation
  ARQ_PF_FX,         // M593 X F frecuencia de Input Shaping en X (0 = desactivado)
  ARQ_PF_FY,         // M593 Y F
  ARQ_PF_COUNT
} arq_pf_id_t;

typedef struct {
  const char *label;       // nombre corto (UTF-8) para la tarjeta
  const char *unit;        // unidad (UTF-8)
  float       min, max;    // limites de la pantalla (dentro de los de Marlin)
  float       fine, coarse;// paso fino y grueso
  uint8_t     dec;         // decimales al mostrar y al enviar
  bool        zero_off;    // 0 es valido y significa "desactivado" (K y frecuencias de shaping); fuera de [min,max]
} arq_pf_param_t;

// shaping_min = frecuencia minima que admite el buffer de Input Shaping compilado (Marlin: shaping_min_freq; por debajo se desborda).
// Solo afecta a ARQ_PF_FX / ARQ_PF_FY.
arq_pf_param_t arq_pf_param(uint8_t id, float shaping_min);

// Ajusta v a un valor valido: dentro de limites y en la rejilla de decimales (sin deriva de coma flotante). Un NaN/infinito devuelve el minimo.
float arq_pf_clamp(uint8_t id, float v, float shaping_min);

// Un paso arriba (dir > 0) o abajo (dir < 0), fino o grueso. En los parametros con "cero = desactivado": bajar del minimo da 0 y subir desde 0 da el minimo.
float arq_pf_step(uint8_t id, float v, int8_t dir, bool coarse, float shaping_min);

// Orden de Marlin que aplica el valor ("M204 P3000", "M593 X F35.0"...). Devuelve 0 si id/valor no son validos o no cabe; si no, la longitud.
size_t arq_pf_fmt_cmd(char *buf, size_t n, uint8_t id, float v, float shaping_min);

// Numero con 'dec' decimales sin printf de coma flotante (la libc reducida de la placa no lo trae siempre). "OFF" si zero_off y v == 0.
size_t arq_pf_fmt_value(char *buf, size_t n, uint8_t id, float v);

// Aviso informativo sobre un valor (nunca bloquea): devuelve 0 (nada) o un codigo ARQ_PF_HINT_*.
typedef enum { ARQ_PF_HINT_NONE = 0, ARQ_PF_HINT_CAPPED, ARQ_PF_HINT_HIGH_ACC, ARQ_PF_HINT_NO_SHAPING, ARQ_PF_HINT_K_ZERO } arq_pf_hint_t;
// acc_effective: aceleracion que Marlin aplicara de verdad = min(valor, aceleracion maxima de X e Y). Se avisa si el valor queda limitado.
uint8_t arq_pf_hint(uint8_t id, float v, float amax_x, float amax_y, float ref_value);
float   arq_pf_effective_acc(float acc, float amax_x, float amax_y);

bool arq_pf_equal(uint8_t id, float a, float b);    // iguales a la precision que se muestra (para saber si hay cambios sin guardar)

typedef enum {
  ARQ_PF_OK = 0,
  ARQ_PF_E_UNAVAILABLE,  // este firmware no tiene ese valor (p. ej. sin Linear Advance)
  ARQ_PF_E_PARAM,        // valor fuera de limites
  ARQ_PF_E_PRINTING,     // impresion en curso
  ARQ_PF_E_BUSY,         // hay movimientos o ordenes pendientes, o la nivelacion / el autoajuste PID estan en marcha
  ARQ_PF_E_STOPPED,      // Marlin detenido
  ARQ_PF_E_REJECT,       // Marlin no tomo el valor
  ARQ_PF_E_SAVE,         // aplicado pero no se pudo guardar en la EEPROM
  ARQ_PF_E_COUNT
} arq_pf_err_t;

const char * arq_pf_err_text(uint8_t e);
const char * arq_pf_hint_text(uint8_t hint);     // texto de un arq_pf_hint_t (nullptr si ARQ_PF_HINT_NONE)
