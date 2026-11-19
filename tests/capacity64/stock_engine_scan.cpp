#include <initializer_list>
#include <initializer_list>
#include "prelude.h"
#include "co.cpp"
int main(){ long alias_active=0, alias_cancel=0, dead_cancel=0;
  for (int v=32; v<=32767; ++v){ cancelable.reset(); cancelable.object_count=50; cancelable.set_active_object(v);
     if (cancelable.active_object>=0 && cancelable.active_object<32 && cancelable.active_object!=v) alias_active++;
     cancelable.reset(); cancelable.cancel_object(v); if (cancelable.canceled) alias_cancel++; if(!cancelable.canceled) dead_cancel++; }
  printf("MOTOR REAL: de los valores M486 32..32767: S<v> acaba activando OTRO objeto 0..31: %ld ; P<v> cancela un objeto equivocado: %ld ; P<v> sin efecto: %ld\n", alias_active, alias_cancel, dead_cancel); }
