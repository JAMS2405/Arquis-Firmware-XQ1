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
 * ARQUIS UI - componentes reutilizables.
 *
 * Diseno orientado a memoria: en esta version de LVGL (6.1.1-MKS) cada objeto cuesta ~340 bytes del heap de 16 KB.
 * Por eso cada componente es UN SOLO objeto ("tarjeta") que dibuja su fondo, icono, textos y punto de estado
 * con lv_draw_*() en su callback de diseno, en lugar de un panel + imagenes + etiquetas separadas.
 *  - Sin asignacion dinamica propia: los textos son punteros a cadenas estaticas o a buffers del llamador.
 *  - Los "set" solo invalidan (redibujan) cuando algo cambio.
 *  - Los botones se conectan con lv_obj_set_event_cb_mks() como el resto de pantallas MKS: el callback recibe
 *    los eventos normales de LVGL y obj->mks_obj_id contiene el id indicado al crearlo.
 */

#include "arquis_theme.h"

typedef enum {
  ARQ_LAYOUT_TILE,     // boton de navegacion: icono arriba, texto debajo (centrado)
  ARQ_LAYOUT_BUTTON,   // boton: icono y/o texto en una linea (centrado)
  ARQ_LAYOUT_STAT,     // tarjeta de dato: icono + titulo, punto de estado, valor grande, subtitulo
  ARQ_LAYOUT_CHIP,     // indicador: punto de color + texto
  ARQ_LAYOUT_HEADER,   // barra superior con titulo
  ARQ_LAYOUT_BAR       // barra de progreso
} arq_layout_t;

lv_obj_t * arq_card_create(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                           arq_layout_t layout, arq_btn_kind_t kind, lv_event_cb_t cb, uint16_t id);
void arq_card_set_icon(lv_obj_t *c, const lv_img_dsc_t *icon, arq_tint_t tint);
void arq_card_set_title(lv_obj_t *c, const char *text, arq_text_t role);
void arq_card_set_value(lv_obj_t *c, const char *text, arq_text_t role);
void arq_card_set_sub(lv_obj_t *c, const char *text, arq_text_t role);
void arq_card_set_dot(lv_obj_t *c, arq_tint_t tint);          // ARQ_TINT_AUTO oculta el punto
void arq_card_set_percent(lv_obj_t *c, uint8_t percent);      // solo ARQ_LAYOUT_BAR
void arq_card_refresh(lv_obj_t *c);                           // redibuja (si el contenido de un buffer cambio)

void       arq_screen_apply(lv_obj_t *scr);                   // fondo de pantalla ARQUIS

// Atajos ----------------------------------------------------------------------------------------------
lv_obj_t * arq_nav_tile_create(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h, const lv_img_dsc_t *icon,
                               arq_tint_t icon_tint, const char *text, arq_btn_kind_t kind, lv_event_cb_t cb, uint16_t id);
lv_obj_t * arq_button_create(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h, const lv_img_dsc_t *icon,
                             const char *text, arq_btn_kind_t kind, lv_event_cb_t cb, uint16_t id);
lv_obj_t * arq_chip_create(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, const char *text, arq_tint_t dot_tint);
lv_obj_t * arq_progress_create(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h);
#define    arq_progress_set(bar, pct) arq_card_set_percent((bar), (pct))

// Cabecera de pantalla: barra con titulo y, opcionalmente, boton Atras (2 objetos)
typedef struct { lv_obj_t *bar, *back; } arq_header_t;
void arq_header_create(arq_header_t *hd, lv_obj_t *parent, const char *static_title, lv_event_cb_t back_cb, uint16_t back_id);

// Tarjeta de temperatura (hotend / cama): 1 objeto -----------------------------------------------------
typedef enum { ARQ_TEMP_OFF, ARQ_TEMP_HEATING, ARQ_TEMP_READY, ARQ_TEMP_COOLING } arq_temp_state_t;
typedef struct { lv_obj_t *card; int16_t cur, tgt; uint8_t state; char vbuf[10], tbuf[16]; } arq_temp_tile_t;
void arq_temp_tile_create(arq_temp_tile_t *t, lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                          const lv_img_dsc_t *icon, const char *static_name);
void arq_temp_tile_update(arq_temp_tile_t *t, int16_t cur, int16_t target);   // solo redibuja si cambia algo
arq_temp_state_t arq_temp_state(int16_t cur, int16_t target);
