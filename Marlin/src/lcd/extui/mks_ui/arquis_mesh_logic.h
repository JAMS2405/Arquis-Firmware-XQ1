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
 * ARQUIS UI - nivelacion manual 5x5: LOGICA PURA (sin LVGL ni Marlin; se prueba en el PC con los casos reales).
 *
 * CONVENCION DE SIGNOS (la de Marlin, comprobada en G29.cpp / bbl.cpp / planner.cpp):
 *   - Z absoluta  : posicion Z de la maquina (sin compensacion) cuando el papel roza. Mayor Z = boquilla mas arriba.
 *   - Correccion  : lo que Marlin guarda en la malla bilineal: z_values[x][y] = Z_absoluta(punto) - Z_absoluta(referencia).
 *                   Marlin la SUMA a la Z ordenada:  Z_fisica = Z_ordenada + correccion  (x fade).
 *                   Positiva = en ese punto la cama esta MAS ALTA que en la referencia (la boquilla debe subir).
 *                   Negativa = la cama esta MAS BAJA (la boquilla debe bajar).
 *   - Referencia  : el punto CENTRAL (indice 12, fila 2 / columna 2): su correccion es siempre 0,00. Asi la malla describe SOLO la
 *                   forma de la superficie y el Z-offset global (home offset / babystep) sigue siendo independiente; ademas la
 *                   altura de desvanecimiento (fade height) no estira la pieza por un offset global.
 *   - Desviacion relativa (la que se muestra): igual que la correccion.
 *
 * INDICES: p = iy * 5 + ix  (ix: columna, de izquierda a derecha; iy: fila, de delante hacia atras: Y crece hacia atras).
 * ORDEN DE MEDICION (seq 0..24): serpentina por filas empezando delante-izquierda; es el mismo orden que G29 manual de Marlin.
 */

#include <stdint.h>
#include <stddef.h>

#define ARQ_MESH_N        5
#define ARQ_MESH_PTS      25
#define ARQ_MESH_CENTER   12            // indice de la referencia (fila 2, columna 2)

// ---- Limites de seguridad (mm) ---------------------------------------------------------------------------------------------------
#define ARQ_MESH_Z_MIN          (-2.00f)   // la Z absoluta nunca baja de aqui durante la medicion
#define ARQ_MESH_Z_MAX          (10.00f)
#define ARQ_MESH_COARSE_FLOOR   ( 0.50f)   // pasos mayores de 0,10 mm no pueden BAJAR por debajo de esta Z (cerca del papel solo pasos finos)
#define ARQ_MESH_NEXT_MARGIN    ( 0.60f)   // el siguiente punto parte de (ultima medicion + margen), nunca mas alto que la altura inicial
#define ARQ_MESH_MAX_DEV        ( 2.00f)   // |correccion| mayor que esto = medicion sospechosa: la malla se rechaza
#define ARQ_MESH_LAYOUT_TOL     ( 0.05f)   // mm: tolerancia al comprobar que la malla guardada tiene la geometria esperada

#define ARQ_MESH_STEPS    4
extern const float arq_mesh_step_mm[ARQ_MESH_STEPS];     // 1.00, 0.10, 0.05, 0.01

// ---- Orden y coordenadas ---------------------------------------------------------------------------------------------------------
uint8_t arq_mesh_seq_to_p(uint8_t seq);                   // seq (orden de medicion) -> indice p. Fuera de rango: 0xFF
uint8_t arq_mesh_p_to_seq(uint8_t p);
inline uint8_t arq_mesh_ix(uint8_t p) { return p % ARQ_MESH_N; }
inline uint8_t arq_mesh_iy(uint8_t p) { return p / ARQ_MESH_N; }

typedef struct { float x_min, x_max, y_min, y_max; } arq_mesh_area_t;     // area de la malla en coordenadas de Marlin (mm)
bool arq_mesh_area_ok(const arq_mesh_area_t *a);          // finita, ordenada y con paso > 1 mm
float arq_mesh_spacing_x(const arq_mesh_area_t *a);
float arq_mesh_spacing_y(const arq_mesh_area_t *a);
void arq_mesh_point_xy(const arq_mesh_area_t *a, uint8_t p, float *x, float *y);

// ---- Movimiento de Z con proteccion ----------------------------------------------------------------------------------------------
// Z nueva tras pulsar subir (dir=+1) o bajar (dir=-1) con el paso step_mm. Aplica: limites absolutos y suelo de pasos gruesos.
float arq_mesh_step_z(float z, int8_t dir, float step_mm);
// Z a la que se deja la boquilla al llegar a un punto: el primero usa start_z; los siguientes min(start_z, ultima + margen).
float arq_mesh_start_z(bool have_last, float last_z, float start_z);

// ---- Sesion de medicion ----------------------------------------------------------------------------------------------------------
typedef enum {
  ARQ_MESH_IDLE,       // sin sesion
  ARQ_MESH_PREP,       // calentando cama, homing y elevando Z
  ARQ_MESH_MOVING,     // la boquilla va al punto 'cur'
  ARQ_MESH_ADJUST,     // el usuario ajusta Z con el papel
  ARQ_MESH_REVIEW,     // los 25 puntos medidos: revisar, repetir o guardar
  ARQ_MESH_DONE,       // aplicada (y guardada si se pidio)
  ARQ_MESH_ABORTED,    // cancelada: no se toca la malla anterior
  ARQ_MESH_ERROR       // error (ver err)
} arq_mesh_state_t;

typedef enum {
  ARQ_MESH_OK = 0,
  ARQ_MESH_E_STATE,        // operacion no valida en el estado actual
  ARQ_MESH_E_Z_RANGE,      // Z fuera de limites o no finita
  ARQ_MESH_E_INCOMPLETE,   // faltan puntos
  ARQ_MESH_E_RANGE,        // desviacion mayor que ARQ_MESH_MAX_DEV
  ARQ_MESH_E_NAN,          // valor no finito
  ARQ_MESH_E_NO_MESH,      // Marlin no tiene malla (rejilla sin definir)
  ARQ_MESH_E_LAYOUT,       // la malla de Marlin no tiene la geometria 5x5 esperada
  ARQ_MESH_E_BAD_POINT,    // indice de punto invalido o punto sin medir
  ARQ_MESH_E_PRINTING,     // hay una impresion en curso
  ARQ_MESH_E_NOT_HOMED,    // sin homing valido
  ARQ_MESH_E_TEMP,         // temperatura de cama fuera de limites
  ARQ_MESH_E_G29_BUSY,     // un G29 manual de Marlin esta en curso
  ARQ_MESH_E_EEPROM,       // la EEPROM no se pudo escribir/leer
  ARQ_MESH_E_APPLY,        // Marlin no activo la malla
  ARQ_MESH_E_Z_SYNC,       // la Z real de Marlin no coincide con la mostrada (orden perdida): hay que confirmar de nuevo
  ARQ_MESH_E_COUNT
} arq_mesh_err_t;
const char * arq_mesh_err_text(uint8_t err);               // texto corto en espanol

typedef struct {
  uint8_t  st;                       // arq_mesh_state_t
  uint8_t  err;                      // arq_mesh_err_t (ultimo error)
  uint8_t  cur;                      // indice p del punto en curso
  uint8_t  step_idx;                 // 0..3 en arq_mesh_step_mm
  uint8_t  redo;                     // 1: se esta repitiendo un punto ya medido (al confirmar se vuelve a REVIEW si estaban todos)
  uint32_t done;                     // bit p = punto confirmado por el usuario
  float    z_now;                    // Z absoluta ordenada para el punto en curso
  float    z[ARQ_MESH_PTS];          // Z absoluta confirmada (valida solo si el bit esta puesto)
  float    last_z;                   // ultima Z confirmada (para el arranque del siguiente punto)
  bool     have_last;
} arq_mesh_sess_t;

void    arq_mesh_sess_reset(arq_mesh_sess_t *s);           // IDLE y sin datos
bool    arq_mesh_begin(arq_mesh_sess_t *s);                // IDLE/DONE/ABORTED/ERROR -> PREP
bool    arq_mesh_prep_done(arq_mesh_sess_t *s);            // PREP -> MOVING hacia el primer punto en orden de medicion
bool    arq_mesh_arrived(arq_mesh_sess_t *s, float start_z);   // MOVING -> ADJUST (z_now = start_z)
bool    arq_mesh_select_step(arq_mesh_sess_t *s, uint8_t idx);
bool    arq_mesh_jog(arq_mesh_sess_t *s, int8_t dir);      // ADJUST: mueve z_now un paso (con proteccion). true si cambio
uint8_t arq_mesh_confirm(arq_mesh_sess_t *s, float z_abs); // ADJUST: registra el punto en curso; devuelve arq_mesh_err_t
uint8_t arq_mesh_repeat(arq_mesh_sess_t *s, uint8_t p);    // ADJUST/REVIEW: re-mide p (queda PENDIENTE hasta confirmarlo)
void    arq_mesh_cancel(arq_mesh_sess_t *s);               // PREP/MOVING/ADJUST/REVIEW -> ABORTED y borra los datos
void    arq_mesh_fail(arq_mesh_sess_t *s, uint8_t err);    // -> ERROR (borra los datos)
bool    arq_mesh_finish(arq_mesh_sess_t *s);               // REVIEW -> DONE
uint8_t arq_mesh_count(const arq_mesh_sess_t *s);          // puntos confirmados
uint8_t arq_mesh_percent(const arq_mesh_sess_t *s);        // 0..100
int8_t  arq_mesh_next_pending(const arq_mesh_sess_t *s);   // primer p pendiente en orden de medicion, -1 si no queda
bool    arq_mesh_is_active(const arq_mesh_sess_t *s);      // PREP, MOVING, ADJUST o REVIEW (hay una calibracion sin terminar)

// ---- Calculo, validacion y estadisticas ------------------------------------------------------------------------------------------
// Correcciones (relativas al centro) a partir de las Z absolutas. Devuelve arq_mesh_err_t; out[p] solo se escribe si OK.
uint8_t arq_mesh_corrections(const arq_mesh_sess_t *s, float out[ARQ_MESH_PTS]);

typedef struct {
  float min, max, range, mean, sigma;    // sigma: desviacion tipica poblacional
  uint8_t imin, imax;                    // indices p del punto mas bajo / mas alto
  uint8_t n;                             // valores usados
} arq_mesh_stats_t;
// Estadisticas de los valores v[p] cuyo bit esta en 'mask'. Con n == 0 todo queda a cero.
void arq_mesh_stats(const float v[ARQ_MESH_PTS], uint32_t mask, arq_mesh_stats_t *out);

// Valida una malla YA guardada en Marlin (z[p], rejilla): no definida, no finita, geometria distinta o desviacion excesiva.
// grid_x/grid_y = inicio de la rejilla; sp_x/sp_y = paso (0 = sin malla).
uint8_t arq_mesh_check_stored(const float z[ARQ_MESH_PTS], float start_x, float start_y, float sp_x, float sp_y, const arq_mesh_area_t *expected);

// Escala de color: valor normalizado -1..+1 (0 = referencia) usando la mayor desviacion absoluta; limit<=0 da 0.
float arq_mesh_norm(float v, float limit);

// ---- Textos ----------------------------------------------------------------------------------------------------------------------
// "+0.12", "-0.05", "0.00" (nunca "-0.00"). Devuelve la longitud escrita.
size_t arq_mesh_fmt_dev(char *buf, size_t n, float v);
// "1.00" (sin signo; para Z absoluta; negativa con "-").
size_t arq_mesh_fmt_z(char *buf, size_t n, float v);
