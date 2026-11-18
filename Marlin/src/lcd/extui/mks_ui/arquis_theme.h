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
 * ARQUIS UI - tema visual (colores, tipografia, estilos).
 * Identidad corporativa ARQUIS. Todos los estilos son estaticos (sin asignacion dinamica de memoria).
 * Pantalla: MKS TS35 480x320, LVGL 6.1.1-MKS (16 bits, sin intercambio de bytes).
 */

#include <lvgl.h>
#include "arquis_icons.h"

#ifdef __cplusplus
extern "C" {
#endif
LV_FONT_DECLARE(arq_font_text16)   // Inter Medium 16, Latin-1 (espanol)
LV_FONT_DECLARE(arq_font_num36)    // Inter Display SemiBold 36, solo cifras y simbolos de medida
#ifdef __cplusplus
}
#endif

// Version de ARQUIS UI (se muestra en Ajustes > Sistema y se usa para nombrar el paquete de entrega)
#define ARQUIS_UI_VERSION "1.0.0"

// ---- Paleta ARQUIS --------------------------------------------------------------------------
#define ARQ_COL_BG        LV_COLOR_MAKE(0x15, 0x1D, 0x26)  // Grafito: fondo de pantalla
#define ARQ_COL_PANEL     LV_COLOR_MAKE(0x22, 0x2D, 0x38)  // Paneles y tarjetas
#define ARQ_COL_PANEL_HI  LV_COLOR_MAKE(0x2D, 0x3B, 0x49)  // Panel pulsado
#define ARQ_COL_LINE      LV_COLOR_MAKE(0x35, 0x44, 0x53)  // Bordes, divisores, fondo de barras
#define ARQ_COL_TEXT      LV_COLOR_MAKE(0xF1, 0xF5, 0xF9)  // Texto principal
#define ARQ_COL_MUTED     LV_COLOR_MAKE(0x9B, 0xAA, 0xB8)  // Texto secundario
#define ARQ_COL_BLUE      LV_COLOR_MAKE(0x30, 0xAC, 0xE3)  // Azul tecnologico: acento principal
#define ARQ_COL_BLUE_DK   LV_COLOR_MAKE(0x1F, 0x86, 0xB5)  // Azul pulsado
#define ARQ_COL_GREEN     LV_COLOR_MAKE(0x4C, 0xAF, 0x53)  // Verde corporativo: listo / correcto
#define ARQ_COL_GREEN_DK  LV_COLOR_MAKE(0x39, 0x8C, 0x3F)  // Verde pulsado
// Colores de estado (propuestos, fuera de la paleta base: calentando y error)
#define ARQ_COL_WARN      LV_COLOR_MAKE(0xF5, 0xA5, 0x24)  // Ambar: calentando / atencion
#define ARQ_COL_DANGER    LV_COLOR_MAKE(0xE5, 0x48, 0x4D)  // Rojo: error / accion peligrosa

// ---- Geometria (480x320) -----------------------------------------------------------------------
#define ARQ_W            480
#define ARQ_H            320
#define ARQ_MARGIN       10
#define ARQ_GAP          8
#define ARQ_HEADER_H     44
#define ARQ_RADIUS       10
#define ARQ_TOUCH_MIN    48      // lado minimo de un objetivo tactil

typedef enum { ARQ_TINT_ACCENT, ARQ_TINT_TEXT, ARQ_TINT_MUTED, ARQ_TINT_DARK, ARQ_TINT_OK, ARQ_TINT_WARN, ARQ_TINT_DANGER, ARQ_TINT_COUNT,
               ARQ_TINT_AUTO = ARQ_TINT_COUNT } arq_tint_t;   // AUTO: el icono toma el color del texto del boton
typedef enum { ARQ_TEXT_BODY, ARQ_TEXT_MUTED, ARQ_TEXT_ACCENT, ARQ_TEXT_OK, ARQ_TEXT_WARN, ARQ_TEXT_DARK,
               ARQ_TEXT_NUM_L, ARQ_TEXT_NUM_L_OK, ARQ_TEXT_NUM_L_WARN, ARQ_TEXT_COUNT } arq_text_t;
typedef enum { ARQ_BTN_PRIMARY, ARQ_BTN_SECONDARY, ARQ_BTN_SUCCESS, ARQ_BTN_DANGER, ARQ_BTN_GHOST, ARQ_BTN_COUNT } arq_btn_kind_t;

// Estados visuales de un icono (misma geometria, distinto color): reglas de color ARQUIS
//  AZUL = navegacion/seleccion, VERDE = correcto/activo, AMBAR = calentando/advertencia, ROJO = error/peligro, GRIS = deshabilitado
typedef enum { ARQ_ICON_NORMAL, ARQ_ICON_SELECTED, ARQ_ICON_ACTIVE, ARQ_ICON_DISABLED, ARQ_ICON_WARNING, ARQ_ICON_ERROR } arq_icon_state_t;
arq_tint_t arq_icon_state_tint(arq_icon_state_t s);

void arq_theme_init();   // Idempotente. Llamar una vez antes de crear widgets.

lv_color_t arq_tint_color(arq_tint_t t);

const lv_style_t* arq_style_screen();
const lv_style_t* arq_style_panel();
const lv_style_t* arq_style_panel_pressed();
const lv_style_t* arq_style_chip();
const lv_style_t* arq_style_focus();      // anillo azul de foco (navegacion con perilla)
const lv_style_t* arq_style_scrim();      // velo oscuro tras un dialogo
const lv_style_t* arq_style_header();
const lv_style_t* arq_style_text(arq_text_t role);
const lv_style_t* arq_style_icon(arq_tint_t tint);
const lv_style_t* arq_style_dot(arq_tint_t tint);
const lv_style_t* arq_style_btn(arq_btn_kind_t kind, lv_btn_state_t state);
const lv_style_t* arq_style_bar_bg();
const lv_style_t* arq_style_bar_indic();
