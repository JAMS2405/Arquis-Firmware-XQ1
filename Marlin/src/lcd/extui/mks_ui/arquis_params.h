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
 * ARQUIS UI - pantallas de parametros de Marlin/MKS con aspecto ARQUIS (glue).
 *
 * draw_ui.cpp llama a estas funciones desde los ayudantes de pantalla originales (lv_screen_create, lv_big_button_create,
 * lv_screen_menu_item*): cada pantalla de parametros conserva su logica y sus manejadores; solo cambia como se dibuja.
 */

#include <lvgl.h>
#include <stdint.h>

const char * arq_es(const char *t);                       // traduce al espanol un texto de las tablas de MKS (sin traduccion: lo devuelve igual)
bool arq_params_state(int disp_state);                    // true si esa pantalla se dibuja con la lista de parametros
void arq_params_begin(lv_obj_t *scr, int disp_state, const char *title);
bool arq_params_button(lv_obj_t *par, const char *text, lv_event_cb_t cb, int id);   // Volver / Siguiente / Anterior (true = creado)
// kind: ARQ_PL_NAV / EDIT1 / EDIT2 / ONOFF (arquis_params_view.h)
void arq_params_row(lv_obj_t *par, const char *text, lv_coord_t y, lv_event_cb_t cb, int id, uint8_t kind, const char *v1, const char *v2, int id2, bool on);
void arq_params_onoff_update(bool on);
bool arq_numpad_value_ok(const char *s);                  // el texto del teclado es un numero valido (al menos un digito)

// Dialogos de MKS (draw_dialog.cpp): el codigo original decide el tipo, los textos y los manejadores; esto solo cambia el aspecto.
// label/btnOk/btnCancel son los objetos originales (se leen sus textos y se eliminan). Cualquiera puede ser nullptr.
void arq_dialog_restyle(lv_obj_t *scr, lv_obj_t *label, lv_obj_t *btnOk, lv_obj_t *btnCancel, lv_event_cb_t ok_cb, lv_event_cb_t cancel_cb, bool danger, bool compact = false);
