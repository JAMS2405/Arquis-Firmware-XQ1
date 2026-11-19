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

#include <stdint.h>

/**
 * Carga del manifiesto ARQUIS_OBJECTS_V1 desde la SD (solo LECTURA) al elegir un archivo para imprimir.
 *
 * No interfiere con la lectura normal del G-code, las miniaturas ni la impresion:
 *  - Solo trabaja mientras esta en pantalla la confirmacion de "Imprimir archivo" y NO hay ningun archivo abierto ni impresion en
 *    curso (CardReader::openFileRead() llama a abortFilePrintNow(): abrir un archivo durante una impresion la abortaria).
 *  - Como la miniatura: abre el archivo, lee UN trozo de 512 B y lo cierra en cada paso (un archivo abierto haria que Marlin lo trate
 *    como "impresion pausada"). Cabecera: 1 paso; cola (ultimos 16 KB): 32 pasos. Comparte public_buf solo cuando la miniatura ya termino.
 *  - Si el usuario pulsa Imprimir antes de que termine, se completa de forma sincrona (<= 33 lecturas pequenas) ANTES de iniciar la impresion.
 *  - Cualquier fallo de lectura => sin manifiesto (la impresion sigue exactamente igual).
 */
void     arq_objload_start();               // el archivo elegido es list_file.file_name[sel_id] (el mismo que usa la miniatura)
void     arq_objload_poll();                // un paso por llamada (desde GUI_RefreshPage); no hace nada si no hay carga pendiente
void     arq_objload_finish();              // termina de forma sincrona si hay una carga a medias
void     arq_objload_stop();                // abandona la carga (el usuario salio de la confirmacion): deja "sin manifiesto"
bool     arq_objload_busy();
uint32_t arq_objload_printing_file_id();    // identificador del archivo que se imprime ahora (0 si no hay archivo abierto)
