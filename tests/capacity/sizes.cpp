#include <stdint.h>
#include <stdio.h>
struct ArqObjRec { int16_t cx, cy; uint16_t w, d, ztop, rot; uint8_t name_idx, type; };   // cx,cy,w,d,ztop en 0.1 mm
struct ArqObjStore { uint8_t n, n_names; uint16_t flags; ArqObjRec rec[64]; char names[64][24]; };
struct ArquisCap64 { uint64_t canceled; int16_t bx0[64], by0[64], bx1[64], by1[64]; };
int main(){ printf("ArqObjRec=%zu B  ArqObjStore(64 obj, 64 nombres)=%zu B  mascara+cajas aprendidas(64)=%zu B\n", sizeof(ArqObjRec), sizeof(ArqObjStore), sizeof(ArquisCap64)); }
