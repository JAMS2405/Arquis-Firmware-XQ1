// Tamano REAL de las estructuras del firmware para 30 objetos (Etapa 3): se incluye la cabecera de Marlin/src/lcd/extui/mks_ui/arquis_objects.h,
// no una copia. Los tipos son de ancho fijo: igual en la placa (ARM de 32 bits) salvo el relleno de alineacion, que aqui es el mismo (todo <= 4 B).
#include "arquis_objects.h"
#include <stdio.h>
static_assert(sizeof(ArqObjRec) == 16, "registro de 16 B (el numero de copia ocupa un byte propio para nombrar 'Llavero 3')");
int main() {
  printf("ArqObjRec=%zu B  ArqObjStore(%d objetos, hasta %d nombres de %d B)=%zu B\n", sizeof(ArqObjRec), ARQ_OBJ_DESIGN_CAPACITY, ARQ_OBJ_DESIGN_CAPACITY, ARQ_OBJ_NAME_MAX, sizeof(ArqObjStore));
  printf("RAM estatica nueva del lector (tabla + estado del lector) medida en ARM: ver tests/firmware/measure_size.sh\n");
  return 0;
}
