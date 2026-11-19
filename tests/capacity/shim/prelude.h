// Prelude para compilar, sin modificarlos, feature/cancel_object.cpp y gcode/feature/cancel/M486.cpp REALES de Marlin 2.1.2.10.
// Las macros de bits son COPIA EXACTA de Marlin/src/core/macros.h (cura/tests/test_real_marlin_engine.py comprueba que siguen iguales).
#pragma once
#include <stdint.h>
#include <stdio.h>
#undef _BV
#define _BV(b) (1 << (b))
#define TEST(n,b) (!!((n)&_BV(b)))
#define SBI(A,B) (A |= _BV(B))
#define CBI(A,B) (A &= ~_BV(B))
#define WITHIN(N,L,H) ((N) >= (L) && (N) <= (H))
#define ENABLED(x) 1
#define ALL(...) 0
#define TERN_(a,b)
#define SERIAL_ECHO_MSG(...)
#define SERIAL_ECHO_START()
#define SERIAL_ECHOPGM(x)
#define SERIAL_ECHO(x)
#define SERIAL_CHAR(c)
#define SERIAL_EOL()
