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
 * ARQUIS UI - nivelacion manual 5x5: CONTROLADOR (pegamento con Marlin; sin LVGL).
 *
 * Marlin hace TODO el calculo: la malla vive en bedlevel (AUTO_BED_LEVELING_BILINEAR) y la compensacion en el planificador.
 * Este modulo solo (1) guia la medicion con G-code por la cola normal, (2) recoge las 25 alturas que el usuario CONFIRMA y
 * (3) las entrega a Marlin en un unico paso atomico y validado (arq_mesh_commit). Nada toca la malla de Marlin hasta ese paso,
 * asi que cancelar o fallar en mitad de la medicion deja la ultima malla valida intacta.
 *
 * Por que no se usa el G29 manual de Marlin para medir: (a) solo avanza (no permite repetir un punto), (b) al arrancar con otra
 * rejilla (p. ej. 3x3 -> 5x5) borra la malla anterior (reset_bed_level) y si se cancela no la recupera, (c) la primera orden baja a Z=0
 * en el primer punto sin comprobar nada. G29 sigue disponible desde la consola; si esta en curso, el asistente se niega a empezar.
 */

#include "arquis_mesh_logic.h"

#define ARQ_MESH_BED_DEFAULT   60      // °C de calibracion por defecto
#define ARQ_MESH_BED_STEP       5
#define ARQ_MESH_SOAK_S        30      // s de espera con la cama a temperatura antes de medir (dilatacion estable)
#define ARQ_MESH_HEAT_TIMEOUT_S 1500   // s: si la cama no llega a temperatura, se cancela

typedef enum { ARQ_PP_NONE, ARQ_PP_HEAT, ARQ_PP_SOAK, ARQ_PP_HOME, ARQ_PP_END } arq_prep_t;   // fases de PREP (END: terminando/cancelando: Z a salvo y estado anterior)

typedef struct {
  uint8_t  st, err, cur, step_idx, redo;
  uint8_t  prep, soak_s, count, percent;
  int16_t  bed_cur, bed_set;
  float    z_now;                 // Z ordenada actual (mm, absoluta)
  bool     busy;                  // hay movimientos u ordenes pendientes
  bool     leveling_on;           // compensacion activa en Marlin
  bool     mesh_ok;               // Marlin tiene una malla 5x5 valida
  uint8_t  mesh_err;              // si !mesh_ok: arq_mesh_err_t
  bool     rel;                   // val[] son DESVIACIONES relativas al centro (REVIEW / malla guardada); si no, son Z absolutas medidas
  uint32_t mask;                  // puntos con valor en val[]
  float    val[ARQ_MESH_PTS];
  float    fade_mm;               // altura de desvanecimiento (0 = sin desvanecimiento)
} arq_mesh_info_t;

void    arq_mesh_ctl_info(arq_mesh_info_t *out);               // instantanea para la pantalla
const arq_mesh_area_t * arq_mesh_ctl_area();                    // area de la malla (coordenadas de Marlin)

uint8_t arq_mesh_ctl_can_start();                               // arq_mesh_err_t: impresion en curso, G29 en curso, Marlin detenido...
uint8_t arq_mesh_ctl_start(int16_t bed_temp);                   // empieza: calienta, homing, primer punto. ARQ_MESH_OK o el motivo
void    arq_mesh_ctl_poll();                                    // avanza la maquina de estados (llamar ~4 veces por segundo)
void    arq_mesh_ctl_bed_delta(int8_t dir);                     // ajusta la temperatura de calibracion (solo fuera de sesion)
int16_t arq_mesh_ctl_bed_max();
void    arq_mesh_ctl_select_step(uint8_t idx);
bool    arq_mesh_ctl_jog(int8_t dir);                           // +1 sube / -1 baja un paso. false si no se pudo (ocupado o limite)
uint8_t arq_mesh_ctl_confirm();                                 // confirma el punto: lee la Z REAL de Marlin y la registra
uint8_t arq_mesh_ctl_repeat(uint8_t p);                         // vuelve a medir el punto p
void    arq_mesh_ctl_cancel();                                  // cancela con seguridad (conserva la malla anterior)
uint8_t arq_mesh_ctl_commit();                                  // REVIEW: valida y entrega la malla a Marlin y la activa (atomico)
uint8_t arq_mesh_ctl_save();                                    // M500 (guarda TODOS los ajustes, incluida la malla) y comprueba el resultado
uint8_t arq_mesh_ctl_load();                                    // M501 y activa la malla si es valida
uint8_t arq_mesh_ctl_set_leveling(bool on);                     // activa/desactiva la compensacion (no durante una sesion)
bool    arq_mesh_ctl_session_active();
void    arq_mesh_ctl_ack();                                     // el usuario vio el resultado: DONE/ABORTED/ERROR -> IDLE
