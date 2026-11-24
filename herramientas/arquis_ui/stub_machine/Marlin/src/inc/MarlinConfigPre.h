// STUB para el simulador del asistente de maquina (herramientas/arquis_ui/sim/machine_sim.cpp). Solo lo que usa
// arquis_machine_ui.cpp, con los nombres y significados de Marlin 2.1.2.10.
#pragma once
#include <stdint.h>
#include <string.h>
#define ENABLED(O) ENA_ ## O
#define ENA_ARQUIS_UI 1
#define DISABLED(O) (!ENABLED(O))
#define HAS_TFT_LVGL_UI 1
#define ARQUIS_UI
typedef uint32_t millis_t;
#define PENDING(NOW,SOON) ((int32_t)((NOW)-(SOON)) < 0)
#define ELAPSED(NOW,SOON) (!PENDING(NOW,SOON))
#define ZERO(a) memset(a, 0, sizeof(a))
#define F(s) (s)
typedef const char *FSTR_P;
#define PROBING_MARGIN 15            // Configuration.h
#define EXTRUDE_MINTEMP 170          // Configuration.h
#define X_MIN_POS 0
#define Y_MIN_POS 0
#define X_MIN_PIN 0
#define Y_MIN_PIN 1
#define Z_MIN_PIN 2
bool sim_read_pin(int pin);
#define READ(P) sim_read_pin(P)
