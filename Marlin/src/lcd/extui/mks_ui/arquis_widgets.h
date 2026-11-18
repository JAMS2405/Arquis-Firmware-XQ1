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
#include "arquis_logo.h"
#include "arquis_logic.h"

typedef enum {
  ARQ_LAYOUT_TILE,     // boton de navegacion: icono arriba, texto debajo (centrado)
  ARQ_LAYOUT_BUTTON,   // boton: icono y/o texto en una linea (centrado)
  ARQ_LAYOUT_STAT,     // tarjeta de dato: icono + titulo, punto de estado, valor grande (+ valor pequeno a la derecha), subtitulo
  ARQ_LAYOUT_CHIP,     // indicador: punto de color + texto
  ARQ_LAYOUT_HEADER,   // barra superior con logo y titulo
  ARQ_LAYOUT_BAR,      // barra de progreso
  ARQ_LAYOUT_FANS,     // lista de ventiladores (icono, nombre, porcentaje, mini barra)
  ARQ_LAYOUT_PRINT,    // tarjeta de impresion: archivo, barra, porcentaje, estado, tiempos
  ARQ_LAYOUT_DIALOG,   // cuadro de dialogo: titulo y mensaje con ajuste de linea
  ARQ_LAYOUT_JOB,      // supervision: archivo, barra, porcentaje grande y dos lineas de tiempo (title, value, value2, sub, pct)
  ARQ_LAYOUT_LIST      // lista de datos: punto de estado, nombre y valor (+ objetivo) alineados a la derecha
} arq_layout_t;

// Todas las funciones aceptan nullptr (si el heap de LVGL se agota, arq_card_create devuelve nullptr y nada falla).
lv_obj_t * arq_card_create(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                           arq_layout_t layout, arq_btn_kind_t kind, lv_event_cb_t cb, uint16_t id);
void arq_card_set_icon(lv_obj_t *c, const lv_img_dsc_t *icon, arq_tint_t tint);
void arq_card_set_title(lv_obj_t *c, const char *text, arq_text_t role);
void arq_card_set_value(lv_obj_t *c, const char *text, arq_text_t role);
void arq_card_set_value2(lv_obj_t *c, const char *text);      // texto pequeno a la derecha del valor (p. ej. objetivo)
void arq_card_set_sub(lv_obj_t *c, const char *text, arq_text_t role);
void arq_card_set_dot(lv_obj_t *c, arq_tint_t tint);          // ARQ_TINT_AUTO oculta el punto
void arq_card_set_percent(lv_obj_t *c, uint8_t percent);      // barra / tarjeta de impresion
void arq_card_set_bar_tint(lv_obj_t *c, arq_tint_t tint);     // color de relleno de la barra (ARQ_TINT_AUTO = azul del tema)
void arq_card_set_logo(lv_obj_t *c, bool show);               // solo cabecera: muestra la marca ARQUIS
void arq_card_set_disabled(lv_obj_t *c, bool disabled);       // deshabilitado: aspecto gris y no recibe toques ni foco
void arq_card_refresh(lv_obj_t *c);                           // redibuja (si el contenido de un buffer cambio)

void       arq_screen_apply(lv_obj_t *scr);                   // fondo de pantalla ARQUIS
void       arq_group_add_clickables(lv_group_t *g, lv_obj_t *scr);   // registra las tarjetas clicables de la pantalla en el grupo de la perilla

// Atajos ----------------------------------------------------------------------------------------------
lv_obj_t * arq_nav_tile_create(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h, const lv_img_dsc_t *icon,
                               arq_tint_t icon_tint, const char *text, arq_btn_kind_t kind, lv_event_cb_t cb, uint16_t id);
lv_obj_t * arq_button_create(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h, const lv_img_dsc_t *icon,
                             const char *text, arq_btn_kind_t kind, lv_event_cb_t cb, uint16_t id);
lv_obj_t * arq_chip_create(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, const char *text, arq_tint_t dot_tint);
lv_obj_t * arq_progress_create(lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h);
#define    arq_progress_set(bar, pct) arq_card_set_percent((bar), (pct))

// Cabecera de pantalla: barra con titulo (y logo) y, opcionalmente, boton Atras (1 o 2 objetos)
typedef struct { lv_obj_t *bar, *back; } arq_header_t;
void arq_header_create(arq_header_t *hd, lv_obj_t *parent, const char *static_title, bool logo, lv_event_cb_t back_cb, uint16_t back_id);

// Tarjeta de temperatura (hotend / cama): 1 objeto --------------------------------------------------------
typedef struct { lv_obj_t *card; int16_t cur, tgt; uint8_t state; char vbuf[10], tbuf[10]; } arq_temp_tile_t;
void arq_temp_tile_create(arq_temp_tile_t *t, lv_obj_t *parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                          const lv_img_dsc_t *icon, const char *static_name, lv_event_cb_t cb = nullptr, uint16_t id = 0);
void arq_temp_tile_update(arq_temp_tile_t *t, int16_t cur, int16_t target, int16_t tol);   // solo redibuja si cambia algo

// Lista de ventiladores: el llamador conserva el arreglo de filas ------------------------------------------
typedef struct { const lv_img_dsc_t *icon; const char *label; uint8_t pct; char txt[6]; } arq_fan_row_t;
#define ARQ_FAN_ROWS_MAX 3
void arq_card_set_fans(lv_obj_t *c, arq_fan_row_t *rows, uint8_t n);

// Lista de datos (ARQ_LAYOUT_LIST): el llamador conserva el arreglo de filas
typedef struct { const char *label; char v1[16]; char v2[10]; uint8_t dot; } arq_list_row_t;   // dot: arq_tint_t (ARQ_TINT_AUTO = sin punto)
#define ARQ_LIST_ROWS_MAX 4
#define ARQ_LIST_ROW_H    22
#define ARQ_LIST_PAD      6
void arq_card_set_rows(lv_obj_t *c, arq_list_row_t *rows, uint8_t n);

// Dialogo de confirmacion (modal, sobre la capa superior). Solo uno abierto a la vez: 4 objetos (3 si es informativo), se liberan al cerrar.
// cancel_text == nullptr -> dialogo informativo con un solo boton (ok_text).
lv_obj_t * arq_dialog_open(const char *title, const char *msg, const char *ok_text, const char *cancel_text, bool danger,
                           lv_event_cb_t cb, uint16_t ok_id, uint16_t cancel_id);
void       arq_dialog_close();
bool       arq_dialog_is_open();
