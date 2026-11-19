// Sustituto minimo de gcode/gcode.h: SOLO el analizador de parametros que usa M486.cpp, con la misma semantica y tipos que Marlin
// (value_int() devuelve int16_t, como parser.h; seen/seenval/intval).
#pragma once
#include <stdint.h>
#include <string.h>
struct Parser {
  bool has[26]; bool hasv[26]; long val[26]; char last;
  void clear() { memset(has, 0, sizeof has); memset(hasv, 0, sizeof hasv); memset(val, 0, sizeof val); last = 0; }
  bool seen(char c) const { return has[c - 'A']; }
  bool seenval(char c) { if (has[c - 'A'] && hasv[c - 'A']) { last = c; return true; } return false; }
  int16_t value_int() const { return int16_t(val[last - 'A']); }
  int32_t value_long() const { const long v = val[last - 'A']; return v > 2147483647L ? 2147483647 : (v < -2147483647L - 1 ? int32_t(-2147483647 - 1) : int32_t(v)); }   // strtol de 32 bits satura
  int32_t longval(char c, int32_t dval = 0) { return seenval(c) ? value_long() : dval; }
  int16_t intval(char c, int16_t dval = 0) { return seenval(c) ? value_int() : dval; }
};
extern Parser parser;
struct GcodeSuite { static void M486(); };
