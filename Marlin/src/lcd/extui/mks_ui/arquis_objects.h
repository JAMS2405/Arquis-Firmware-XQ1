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
 * ARQUIS_OBJECTS_V1 en el firmware: lector del manifiesto, mapa de la cama y estados. LOGICA PURA: no incluye Marlin, ni LVGL, ni la SD
 * (solo <stdint.h> y <string.h>), de modo que se compila y se prueba en el PC contra los vectores compartidos de
 * shared/object_protocol/test_vectors/ (tests/firmware/). La lectura del archivo en la SD esta en arquis_objects_sd.cpp.
 *
 * Especificacion: shared/object_protocol/ARQUIS_OBJECTS_V1.md. El lector reproduce el parser de referencia de Cura
 * (cura/plugins/ARQUISPlugin/engines/manifest_engine.py): mismo "ok" y mismo conjunto de errores para cada vector.
 * Memoria: sin asignacion dinamica; una tabla estatica (arq_obj) y un buffer de linea de 96 B.
 */

#include <stdint.h>
#include "../../../../../shared/object_protocol/generated/arquis_object_limits.h"   // capacidad y limites: UNICA fuente (limits.json)

#define ARQ_MAN_TAIL_WINDOW    16384UL   // el bloque se busca en los ultimos 16 KB (detras va el ;SETTING_ de Cura)
#define ARQ_MAN_HEAD_WINDOW    512U      // la marca de cabecera, en los primeros 512 B
#define ARQ_MAN_LINE_MAX       95        // las lineas del manifiesto miden <= 80 B; las mas largas se tratan como invalidas
#define ARQ_MAN_BED_TOL_DMM    10        // tolerancia de cama: 1,0 mm

enum : uint8_t { ARQ_MAN_NONE = 0, ARQ_MAN_OK = 1, ARQ_MAN_BAD = 2 };   // sin manifiesto (G-code normal) / verificado / presente pero invalido

// Errores: conjunto de bits con los MISMOS nombres que el parser de referencia (E_MANIFEST_CRC -> ARQ_E_CRC...)
enum : uint32_t {
  ARQ_E_TRUNCATED = 1UL << 0,  ARQ_E_FORMAT     = 1UL << 1,  ARQ_E_CRC         = 1UL << 2,  ARQ_E_TRAILER   = 1UL << 3,
  ARQ_E_VERSION   = 1UL << 4,  ARQ_E_NAME_DUP   = 1UL << 5,  ARQ_E_NAME_LONG   = 1UL << 6,  ARQ_E_NAME_INDEX = 1UL << 7,
  ARQ_E_COUNT     = 1UL << 8,  ARQ_E_OVER_CAP   = 1UL << 9,  ARQ_E_CAP         = 1UL << 10, ARQ_E_ID_DUP     = 1UL << 11,
  ARQ_E_ID_RANGE  = 1UL << 12, ARQ_E_SIZE       = 1UL << 13, ARQ_E_Z           = 1UL << 14, ARQ_E_ROT        = 1UL << 15,
  ARQ_E_OUTSIDE   = 1UL << 16, ARQ_E_ORDER      = 1UL << 17, ARQ_E_TRAILER_MM  = 1UL << 18, ARQ_E_HEADER_MM  = 1UL << 19,
  ARQ_E_BED       = 1UL << 20
};

// Un objeto del manifiesto (0,1 mm). La rotacion del manifiesto NO se guarda (el marcador es un rectangulo alineado con los ejes).
struct ArqObjRec {
  int16_t  cx, cy;            // centro
  uint16_t w, d;              // ancho (X) y fondo (Y)
  uint16_t z0, ztop;          // Z de la primera y de la ultima capa con extrusion
  uint8_t  name_idx;          // indice en names[]
  uint8_t  inst;              // numero de copia (0...) dentro del nombre
  uint8_t  order;             // orden de impresion 0..N-1
  uint8_t  shape;             // 'C' circulo, 'S' cuadrado, 'R' rectangulo
};                            // 16 B

struct ArqObjStore {
  uint32_t pid;               // identificador de impresion del manifiesto
  uint32_t file_id;           // a que archivo pertenece (lo fija el cargador; 0 = ninguno)
  uint16_t bed_w, bed_d;      // cama del manifiesto (0,1 mm)
  int16_t  min_x, min_y;      // esquina minima de la cama (0,1 mm)
  uint8_t  n;                 // objetos publicados (= M486 T<n>), 0..ARQ_OBJ_DESIGN_CAPACITY
  uint8_t  n_names;           // nombres base distintos
  uint8_t  seq;               // 1 = un objeto cada vez
  uint8_t  cap;               // CAP declarado (saturado a 255)
  char     org[3];            // "FL" = el origen X0 Y0 es la esquina frontal izquierda
  ArqObjRec rec[ARQ_OBJ_DESIGN_CAPACITY];                     // rec[id]: los ids son contiguos 0..n-1 en un manifiesto valido
  char     names[ARQ_OBJ_DESIGN_CAPACITY][ARQ_OBJ_NAME_BUF];  // nombres base (los de las copias se guardan una vez)
};

extern ArqObjStore arq_obj;   // vacio salvo que arq_man_status() == ARQ_MAN_OK

// ---- Lector (streaming; no depende de como se parta el archivo en trozos) -----------------------------------------------------------
void     arq_man_reset();                                       // sin manifiesto (borra la tabla)
void     arq_man_head(const uint8_t *buf, uint16_t n);          // los primeros min(tamano, 512) B del archivo (marca de cabecera)
void     arq_man_tail_begin();                                  // empieza la lectura de los ultimos 16 KB (o de todo el archivo si es menor)
void     arq_man_tail_feed(const uint8_t *buf, uint16_t n);     // bytes consecutivos, en cualquier trozo
uint8_t  arq_man_tail_finish(uint16_t bed_w_dmm, uint16_t bed_d_dmm);   // valida con la cama de la maquina (0,1 mm); 0 = sin comprobar
uint8_t  arq_man_status();                                      // ARQ_MAN_NONE / OK / BAD
uint32_t arq_man_errors();                                      // bits ARQ_E_* (si BAD)
uint32_t arq_crc32_update(uint32_t raw, const uint8_t *p, uint16_t n);   // CRC-32 IEEE: estado en bruto (empezar con 0xFFFFFFFF; resultado = ~estado)

// ---- Nombres y estados -----------------------------------------------------------------------------------------------------------------
// "Llavero 3" (nombre + copia, solo si hay mas de una con ese nombre) en out[n]; devuelve la longitud
uint8_t  arq_obj_label(char *out, uint8_t n, uint8_t id);

enum : uint8_t { ARQ_ST_PENDING = 0, ARQ_ST_PRINTING = 1, ARQ_ST_DONE = 2, ARQ_ST_CANCELED = 3, ARQ_ST_UNAVAILABLE = 4 };
// Mismos criterios que bed_map_engine.object_state (vectores en bed_map_v1.json); z en 0,1 mm
uint8_t  arq_obj_state(bool available, bool trusted, bool canceled, bool seq, uint16_t z0, uint16_t ztop, uint8_t order, uint16_t z_print, int16_t active_order);

// ---- Mapa de la cama (aritmetica entera; mismos resultados que bed_map_engine.py) --------------------------------------------------------
#define ARQ_MAP_MIN_MARKER_PX  22
#define ARQ_MAP_PAD_PX         8
struct ArqBed    { int32_t min_x, min_y, w, d; };                          // 0,1 mm
struct ArqView   { int16_t x, y, w, h; uint8_t zoom; bool has_center; int32_t cx, cy; };   // zoom 1, 2 o 4; centro en 0,1 mm
struct ArqMarker { int16_t x0, y0, x1, y1, cx, cy; };                      // pixeles; x1/y1 inclusive

int32_t  arq_div_round(int32_t a, int32_t b);                              // floor((2a+b)/(2b)), b > 0
void     arq_to_px(int32_t x, int32_t y, const ArqBed &bed, const ArqView &v, int32_t *px, int32_t *py);
void     arq_marker(int32_t cx, int32_t cy, int32_t w, int32_t d, char shape, const ArqBed &bed, const ArqView &v, ArqMarker *m);
bool     arq_marker_hit(const ArqMarker &m, int32_t px, int32_t py);
bool     arq_marker_beats(const ArqMarker &m, uint8_t id, const ArqMarker &best, int16_t best_id);   // el de menor area; empate: el id mayor

// ---- Comprobacion en ejecucion ------------------------------------------------------------------------------------------------------------
// Caja XY aprendida mientras se imprime (mm enteros) dentro de la caja del manifiesto ampliada en margin_dmm (0,1 mm)
bool     arq_box_within(const ArqObjRec &r, int32_t x0_mm, int32_t y0_mm, int32_t x1_mm, int32_t y1_mm, int32_t margin_dmm);
