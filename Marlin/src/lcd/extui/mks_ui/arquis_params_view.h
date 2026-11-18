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
 * ARQUIS UI - lista de parametros y teclado numerico (solo LVGL: no conoce Marlin).
 *
 * Sustituyen el aspecto MKS de las ~15 pantallas de "Ajustes > Parametros" (pasos, aceleracion, velocidades, TMC, nivelacion...).
 * Cada pantalla es un solo objeto de lista (las filas se dibujan, no son objetos) + cabecera: el uso de heap no depende del numero de filas.
 * El teclado numerico usa 4 objetos: cabecera (2), valor (1) y teclas (1, que dibuja y reconoce las 16 teclas).
 */

#include "arquis_widgets.h"

// ---- Lista de parametros ----------------------------------------------------------------------------------
#define ARQ_PL_ROWS 4
enum { ARQ_PL_NAV, ARQ_PL_EDIT1, ARQ_PL_EDIT2, ARQ_PL_ONOFF };

typedef struct {
  const char *name;          // puntero estable (tabla de idioma o literal)
  char        v1[14], v2[14];// valores ya formateados (se copian)
  uint8_t     kind;          // ARQ_PL_*
  bool        on;            // ONOFF: estado
  int16_t     id1, id2;      // ids que recibe el manejador original (-1 = fila no pulsable)
} arq_pl_row_t;

lv_obj_t * arq_pl_begin(lv_obj_t *scr, const char *title);                       // cabecera con titulo; olvida el estado anterior
void       arq_pl_back(lv_obj_t *scr, lv_event_cb_t cb, uint16_t id);            // boton Atras en la cabecera
void       arq_pl_page(lv_obj_t *scr, const char *text, lv_event_cb_t cb, uint16_t id);   // boton de pagina (Siguiente/Anterior) en la cabecera
void       arq_pl_row(lv_obj_t *scr, uint8_t row, const arq_pl_row_t *r, lv_event_cb_t cb); // fija la fila (la primera llamada crea el objeto de lista)
void       arq_pl_set_on(bool on);                                              // actualiza el interruptor de la ultima fila pulsada
const char*arq_pl_last_name();                                                  // nombre de la ultima fila pulsada (titulo del teclado)
void       arq_pl_forget();                                                     // al liberar la pantalla
int        arq_pl_pick(lv_obj_t *o, const lv_point_t *p, uint8_t *row_out);     // id bajo el punto (coordenadas de pantalla); -1 = ninguno (para pruebas)

// ---- Teclado numerico ---------------------------------------------------------------------------------------
// Ids: coinciden con los del teclado original (1..9 = digitos, 10 = 0) y se anade ARQ_NK_CANCEL.
enum { ARQ_NK_1 = 1, ARQ_NK_9 = 9, ARQ_NK_0 = 10, ARQ_NK_BKSP, ARQ_NK_CLEAR, ARQ_NK_OK, ARQ_NK_POINT, ARQ_NK_MINUS, ARQ_NK_CANCEL, ARQ_NK_BACK /* flecha de la cabecera */ };

void arq_numpad_build(lv_obj_t *scr, lv_event_cb_t cb, const char *title);
void arq_numpad_set(const char *text, bool can_ok);        // valor mostrado y si "Aceptar" esta habilitado
void arq_numpad_forget();
int  arq_numpad_pick(lv_obj_t *o, const lv_point_t *p);    // tecla bajo el punto; -1 = ninguna

// ---- Pantalla de mensaje / confirmacion (sustituye la pantalla de dialogo de MKS) ------------------------------------
// 0, 1 o 2 botones. ok_text / cancel_text == nullptr -> ese boton no se crea. Los textos se copian (buffers fijos). 3-5 objetos.
enum { ARQ_MSG_OK = 1, ARQ_MSG_CANCEL };
// compact: mensaje de 1-2 lineas arriba (deja libre el centro para un texto en vivo o una barra de progreso) y el boton abajo.
void arq_msg_build(lv_obj_t *scr, lv_event_cb_t cb, const char *title, const char *msg, const char *ok_text, const char *cancel_text, bool danger, bool compact = false);

// ---- Cambio de filamento (pantalla de entrada: temperatura + Cargar / Descargar) -----------------------------------------
// Los ids coinciden con los del manejador original (draw_filament_change.cpp): 1 = cargar, 2 = descargar, 3 = extrusor, 4 = volver.
enum { ARQ_FCID_LOAD = 1, ARQ_FCID_UNLOAD, ARQ_FCID_EXT, ARQ_FCID_BACK };
void arq_filchg_build(lv_obj_t *scr, lv_event_cb_t cb, bool multi_extruder);                 // 4-5 objetos
void arq_filchg_update(int16_t cur, int16_t target, uint8_t extruder, bool multi_extruder);  // solo redibuja si cambia algo
void arq_filchg_forget();

// ---- Consola de G-code (draw_gcode.cpp): boton "Escribir G-code" + resultado ----------------------------------------------
// Ids = los del manejador original: 1 = volver, 2 = escribir. 'result' es un buffer del llamador (se lee al dibujar).
enum { ARQ_GC_BACK = 1, ARQ_GC_WRITE };
void arq_gcode_build(lv_obj_t *scr, lv_event_cb_t cb, const char *result);   // 4 objetos

// ---- Estado del Wi-Fi (draw_wifi.cpp): panel de texto + Reconectar / Nube ----------------------------------------------
// Ids = los del manejador original: 1 = volver, 2 = nube, 3 = reconectar.
enum { ARQ_WF_BACK = 1, ARQ_WF_CLOUD, ARQ_WF_RECONNECT };
void arq_wifi_build(lv_obj_t *scr, lv_event_cb_t cb, bool sta_mode, bool cloud);   // 3-5 objetos
void arq_wifi_set(const char *text);                                              // texto del panel (lineas separadas por '\n'); se copia
