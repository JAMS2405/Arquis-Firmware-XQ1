// Prototipo (NO es el firmware) del motor endurecido para 30 objetos: MANTIENE la mascara uint32_t y las macros TEST/SBI/CBI de Marlin
// (indices <= 29, asi que 1<<29 cabe en un int de 32 bits sin desbordar) y valida el rango ANTES de estrechar a int8_t.
#include <stdint.h>
#include <stdio.h>
#define _BV(b) (1 << (b))
#define TEST(n,b) (!!((n)&_BV(b)))
#define SBI(A,B) (A |= _BV(B))
#define CBI(A,B) (A &= ~_BV(B))
#define CAP 30
struct CO {
  bool skipping = false, overflow = false; int8_t object_count = 0, active_object = -1; uint32_t canceled = 0;
  void reset() { canceled = 0; object_count = 0; overflow = false; set_active(-1); }
  void set_T(int16_t v) { reset(); if (v < 0) v = 0; if (v > CAP) { overflow = true; v = CAP; } object_count = (int8_t)v; }
  void set_active(int16_t v) { if (v >= 0 && v < CAP) { active_object = (int8_t)v; if (v >= object_count) object_count = (int8_t)(v + 1); skipping = TEST(canceled, v); }
                               else { active_object = -1; skipping = false; if (v >= CAP) overflow = true; } }
  void cancel(int16_t v) { if (v >= 0 && v < CAP) { SBI(canceled, v); if (v == active_object) skipping = true; } }
  void uncancel(int16_t v) { if (v >= 0 && v < CAP) { CBI(canceled, v); if (v == active_object) skipping = false; } }
};
int main() {
  long bad = 0, ok = 0;
  for (int v = -32768; v <= 32767; ++v) {                         // TODO el rango de parser.value_int() (int16_t)
    const bool in = v >= 0 && v < CAP;
    CO a; a.set_T(CAP); a.set_active((int16_t)v);
    (a.active_object == (in ? v : -1) && !a.skipping ? ok : bad)++;
    CO b; b.set_T(CAP); b.cancel((int16_t)v);
    (b.canceled == (in ? (1u << v) : 0u) ? ok : bad)++;
    CO c; c.set_T(CAP); c.cancel(5); c.uncancel((int16_t)v);
    (c.canceled == ((in && v == 5) ? 0u : (1u << 5)) ? ok : bad)++;
  }
  CO e; e.set_T(CAP);
  for (int i = 0; i < CAP; ++i) { e.cancel(i); e.set_active(i); if (!e.skipping) ++bad; e.uncancel(i); e.set_active(i); if (e.skipping) ++bad; }
  printf("prototipo CAP=30 con mascara uint32_t: %ld comprobaciones correctas, %ld errores (S/P/U sobre los 65 536 valores de int16_t; cancelar/restaurar 0..29)\n", ok, bad);
  CO t; t.set_T(200); printf("M486 T200 -> object_count=%d overflow=%d ; sizeof(estado)=%zu B (mascara %zu B, la misma que hoy)\n", t.object_count, t.overflow, sizeof(CO), sizeof(uint32_t));
  return bad != 0;
}
