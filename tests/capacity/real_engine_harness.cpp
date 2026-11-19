// Ejecuta un G-code con el motor de cancelacion REAL de Marlin (cancel_object.cpp + M486.cpp, compilados sin modificar).
// Uso: harness <archivo.gcode> <ids a cancelar tras M486 T, separados por comas, o -> [legacy-int]
// Salida: una linea por cada G0/G1/G2/G3: "<n linea 0-based> <active_object> <skipping> <mascara canceled>"
#include "prelude.h"
#include "gcode/gcode.h"
#include "feature/cancel_object.h"
#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cstdlib>
Parser parser;
static void load(const std::string &code) {       // "M486 S3 P12 C" -> parser
  parser.clear();
  for (size_t i = 0; i < code.size();) {
    char c = code[i];
    if (c >= 'A' && c <= 'Z' && i > 0) {
      parser.has[c - 'A'] = true; ++i;
      size_t j = i; while (j < code.size() && (code[j] == ' ')) ++j;
      size_t k = j; if (k < code.size() && (code[k] == '-' || code[k] == '+')) ++k;
      while (k < code.size() && ((code[k] >= '0' && code[k] <= '9') || code[k] == '.')) ++k;
      if (k > j && (isdigit((unsigned char)code[k - 1]) || code[k - 1] == '.')) { parser.hasv[c - 'A'] = true; parser.val[c - 'A'] = strtol(code.c_str() + j, nullptr, 10); i = k; } 
      continue;
    }
    ++i;
  }
}
int main(int argc, char **argv) {
  std::ifstream f(argv[1]);
  std::vector<int> cancel;
  if (argc > 2 && std::string(argv[2]) != "-") { std::stringstream ss(argv[2]); std::string t; while (std::getline(ss, t, ',')) cancel.push_back(atoi(t.c_str())); }
  std::string line; long n = -1;
  while (std::getline(f, line)) {
    ++n;
    size_t sc = line.find(';'); std::string code = sc == std::string::npos ? line : line.substr(0, sc);
    size_t a = code.find_first_not_of(" \t"); if (a == std::string::npos) continue; code = code.substr(a);
    for (auto &ch : code) ch = toupper(ch);
    if (code.rfind("M486", 0) == 0 && (code.size() == 4 || code[4] == ' ')) {
      load(code.substr(4));
      GcodeSuite::M486();                                         // <- M486.cpp REAL
      if (parser.seen('T')) for (int c : cancel) cancelable.cancel_object((int8_t)c);   // como "M486 P<c>" justo tras M486 T (llamada directa para fijar el caso)
    }
    else if (code.rfind("G0", 0) == 0 || code.rfind("G1", 0) == 0 || code.rfind("G2", 0) == 0 || code.rfind("G3", 0) == 0) {
      char nx = code.size() > 2 ? code[2] : ' ';
      if (nx == ' ' || nx == 0) printf("%ld %d %d %u\n", n, (int)cancelable.active_object, (int)cancelable.skipping, (unsigned)cancelable.canceled);
    }
  }
  return 0;
}
