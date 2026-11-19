// Prototipo (NO es el firmware) de la capacidad 64 sobre la misma semantica que feature/cancel_object.cpp REAL.
// Cambios respecto al real: mascara de bits de 64, rango validado ANTES de estrechar a int8_t, fuera de rango => no cancelable + bandera.
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define CAP 64
struct CO64 {
  bool skipping=false, overflow=false; int8_t object_count=0, active_object=-1; uint64_t canceled=0;
  void reset(){ canceled=0; object_count=0; overflow=false; set_active(-1); }
  void set_T(int16_t v){ reset(); if (v<0) v=0; if (v>CAP){ overflow=true; v=CAP; } object_count=(int8_t)v; }
  void set_active(int16_t v){ if (v>=0 && v<CAP){ active_object=(int8_t)v; if(v>=object_count) object_count=(int8_t)(v+1); skipping=(canceled>>v)&1; }
                              else { active_object=-1; skipping=false; if (v>=CAP) overflow=true; } }
  void cancel(int16_t v){ if(v>=0&&v<CAP){ canceled|=1ULL<<v; if(v==active_object) skipping=true; } }
  void uncancel(int16_t v){ if(v>=0&&v<CAP){ canceled&=~(1ULL<<v); if(v==active_object) skipping=false; } }
};
int main(){
  long bad=0, ok=0;
  for (int v=-32768; v<=32767; ++v) {                       // TODO el rango de parser.value_int() (int16_t)
    CO64 c; c.set_T(64); c.set_active((int16_t)v);
    int exp = (v>=0&&v<CAP)? v : -1;
    if (c.active_object!=exp) bad++; else ok++;
    CO64 d; d.set_T(64); d.cancel((int16_t)v);
    uint64_t want = (v>=0&&v<CAP)? (1ULL<<v) : 0; if (d.canceled!=want) bad++; else ok++;
  }
  printf("CAP=64 prototipo: S y P sobre los 65536 valores de int16_t -> correctos=%ld, errores=%ld (ningun indice >=64 coincide con otro objeto)\n", ok, bad);
  CO64 c; c.set_T(64); for(int i=0;i<64;i++){ c.cancel(i); c.set_active(i); if(!c.skipping){bad++;} c.uncancel(i); c.set_active(i); if(c.skipping) bad++; }
  printf("cancelar/restaurar cada uno de 0..63: errores=%ld\n", bad);
  CO64 e; e.set_T(200); printf("M486 T200 -> object_count=%d overflow=%d\n", e.object_count, e.overflow);
  printf("sizeof(estado CO64)=%zu B ; mascara=%zu B\n", sizeof(CO64), sizeof(uint64_t));
  return bad!=0;
}
