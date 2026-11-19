// Tamano de las estructuras propuestas para 30 objetos (Etapa 3). Los tipos son de ancho fijo: igual en la placa (ARM de 32 bits).
#include <stdint.h>
#include <stdio.h>
#include "../../shared/object_protocol/generated/arquis_object_limits.h"
// Registro de un objeto (unidades de 0,1 mm). La rotacion del manifiesto NO se guarda: el marcador es un rectangulo alineado con los ejes.
struct ArqObjRec { int16_t cx, cy; uint16_t w, d, z0, ztop; uint8_t name_idx, type_order; };   // type (2 bits) + orden (6 bits) en un byte
struct ArqObjStore { uint8_t n, n_names; uint16_t flags; ArqObjRec rec[ARQ_OBJ_DESIGN_CAPACITY]; char names[ARQ_OBJ_DESIGN_CAPACITY][ARQ_OBJ_NAME_BUF]; };
// Estado del motor: se conserva la mascara uint32_t y las cajas aprendidas [32] que YA existen (indices <= 29 caben).
struct EngineToday { uint32_t canceled; int16_t bx0[32], by0[32], bx1[32], by1[32]; };
static_assert(sizeof(ArqObjRec) == 14, "registro de 14 B");
int main() {
  printf("ArqObjRec=%zu B  ArqObjStore(%d objetos, hasta %d nombres de %d B)=%zu B\n", sizeof(ArqObjRec), ARQ_OBJ_DESIGN_CAPACITY, ARQ_OBJ_DESIGN_CAPACITY, ARQ_OBJ_NAME_MAX, sizeof(ArqObjStore));
  printf("motor actual (mascara + cajas aprendidas [32]) = %zu B: NO cambia\n", sizeof(EngineToday));
  printf("RAM nueva neta (peor caso, 30 nombres distintos) = %zu B\n", sizeof(ArqObjStore));
  return 0;
}
