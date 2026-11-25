// STUB para el simulador del asistente de maquina (herramientas/arquis_ui/sim/machine_sim.cpp) y para la prueba del pegamento
// REAL (tests/machines/glue_test.cpp). Solo lo que usan arquis_machine.cpp y arquis_machine_ui.cpp, con los nombres y
// significados de Marlin 2.1.2.10 y los valores de esta configuracion (Configuration.h / Configuration_adv.h).
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
#define Z_MIN_POS 0
#define X_HOME_DIR -1
#define Y_HOME_DIR -1
#define Z_HOME_DIR -1
#define X_MIN_PIN 0
#define Y_MIN_PIN 1
#define Z_MIN_PIN 2
bool sim_read_pin(int pin);
#define READ(P) sim_read_pin(P)

// Opciones (1 = activada en esta configuracion) y macros TERN de Marlin
#define HAS_MEDIA 0                  // la prueba no simula la microSD
#define HAS_TRINAMIC_CONFIG 1
#define HAS_LEVELING 1
#define HAS_HOME_OFFSET 1
#define HAS_JUNCTION_DEVIATION 1
#define HAS_LINEAR_E_JERK 0
#define HAS_ZV_SHAPING 1
#define INPUT_SHAPING_X 1
#define INPUT_SHAPING_Y 1
#define EXTRUDERS 1
#define NUM_AXES 3
#define INVERT_X_DIR true            // direcciones compiladas = XQ1
#define INVERT_Y_DIR true
#define INVERT_Z_DIR false
#define INVERT_E0_DIR false
#define AXIS_DRIVER_TYPE_X(T) 1
#define AXIS_DRIVER_TYPE_Y(T) 1
#define AXIS_DRIVER_TYPE_Z(T) 1
#define AXIS_DRIVER_TYPE_E0(T) 1
#define _ARQ_T1(A,B) A
#define _ARQ_T0(A,B) B
#define _ARQ_T(E,A,B) _ARQ_T##E(A,B)
#define __ARQ_T(E,A,B) _ARQ_T(E,A,B)
#define TERN(O,A,B) __ARQ_T(O,A,B)
#define TERN_(O,A) TERN(O,A,)
#define TERN0(O,A) TERN(O,A,0)
#define LOOP_NUM_AXES(VAR) for (uint8_t VAR = 0; VAR < NUM_AXES; ++VAR)
