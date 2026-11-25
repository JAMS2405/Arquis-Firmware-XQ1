#pragma once
#include "inc/MarlinConfigPre.h"
uint32_t millis();
bool printJobOngoing();
bool printingIsPaused();
void kill(FSTR_P const lcd_error = nullptr, FSTR_P const lcd_component = nullptr, const bool steppers_off = false);
// Salida serie: la prueba guarda la ultima linea
void arq_fake_serial(const char *s);
#include <stdio.h>
#include <string>
template<typename T> inline void _arq_ser(std::string &o, T v) { o += std::to_string(v); }
inline void _arq_ser(std::string &o, const char *v) { o += v ? v : ""; }
inline void _arq_ser(std::string &o, char *v) { o += v ? v : ""; }
template<typename... A> inline void SERIAL_ECHOLNPGM(A... a) { std::string o; (void)std::initializer_list<int>{ (_arq_ser(o, a), 0)... }; arq_fake_serial(o.c_str()); }
#define SERIAL_ERROR_MSG(S) arq_fake_serial("Error:" S)
#define SERIAL_ERROR_START() arq_fake_serial("Error:")
