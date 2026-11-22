// STUB para verificar arquis_data/nav/dashboard fuera de PlatformIO. Reproduce SOLO las firmas reales de Marlin 2.1.2.10
// leidas en: temperature.h, printcounter.h, cardreader.h, MarlinCore.h, utility.h, macros.h, Configuration*.h.
#pragma once
#include <stdint.h>
#include <string.h>
#define ENABLED(O) ENA_ ## O
#define ENA_ARQUIS_UI 1
#define HAS_TFT_LVGL_UI 1
#define ARQUIS_UI
#define HAS_HOTEND 1
#define HAS_HEATED_BED 1
#define HAS_FAN0 1
#define HAS_FAN1 1
#define HAS_MEDIA 1
#define FAN_COUNT 2
#define TEMP_HYSTERESIS 3
#define TEMP_BED_HYSTERESIS 3
#define E0_AUTO_FAN_PIN -1
#define EXTRUDER_AUTO_FAN_TEMPERATURE 50
#define EXTRUDER_AUTO_FAN_SPEED 255
#define PIN_EXISTS(P) (defined(P##_PIN) && P##_PIN >= 0)
#define MACHINE_NAME "Arquis XQ1"
#define HAS_USER_ITEM(...) 0   // CUSTOM_MENU_MAIN desactivado en esta configuracion
#define COUNT(a) (sizeof(a)/sizeof(*(a)))
#define ZERO(a) memset((void*)a, 0, sizeof(a))
#define TERN(O,A,B) B      // MULTI_VOLUME no esta activo en esta configuracion
#define ENA_CANCEL_OBJECTS 1
#define ENA_POWER_LOSS_RECOVERY 1
#define HAS_SUICIDE 1
#define ALL(A,B) 1          // solo para '#if ALL(HAS_TFT_LVGL_UI, CANCEL_OBJECTS)' de draw_cancel_object.cpp
#define CANCEL_OBJECTS 1
#define X_BED_SIZE 220
#define Y_BED_SIZE 220
#define TEST(n,b) !!((n)&_BV32(b))    // macros.h real: TEST(n,b)
#define _BV32(b) (1UL << (b))
// --- anadidos para arquis_controls.cpp (valores de la configuracion del XQ1; solo verifican que el codigo compila) ---
#include <stdio.h>
#define ENA_HAS_HOTEND 1
#define ENA_HAS_HEATED_BED 1
#define ENA_HAS_MULTI_EXTRUDER 0
#define HAS_FAN 1
#define HEATER_0_MAXTEMP 275
#define WATCH_TEMP_INCREASE 2
#define BED_MAXTEMP 150
#define WATCH_BED_TEMP_INCREASE 2
#define PREHEAT_1_LABEL "PLA"
#define PREHEAT_2_LABEL "ABS"
#define PREHEAT_1_TEMP_HOTEND 210
#define PREHEAT_2_TEMP_HOTEND 240
#define PREHEAT_1_TEMP_BED 60
#define PREHEAT_2_TEMP_BED 90
#define PSTR(s) s
#define sprintf_P sprintf
#define TERN_(O,A) A
#define TERN0(O,A) A
char *dtostrf(double, signed char, unsigned char, char *);
#define DISABLED(O) (!ENABLED(O))
#define strcpy_P strcpy
#define ENA_HAS_SUICIDE 1
#define ENA_AUTO_BED_LEVELING_BILINEAR 1
#define ENA_PRINTCOUNTER 1
#define ENA_EEPROM_SETTINGS 1
#define MAX_EXT_SPEED_PERCENT 999
#define MIN_EXT_SPEED_PERCENT 10
#define X_MIN_POS 0
#define Y_MIN_POS 0
#define X_MAX_POS 220
#define Y_MAX_POS 220
// --- nivelacion manual 5x5 (arquis_mesh*.cpp): valores de la configuracion del XQ1 ---
#define ENA_PROBE_MANUALLY 1
#define ENA_ENABLE_LEVELING_AFTER_G28 1
#define ENA_ENABLE_LEVELING_FADE_HEIGHT 1
#define ANY(A,B) (ENABLED(A) || ENABLED(B))
#define BED_MAX_TARGET 140
#define XY_PROBE_FEEDRATE (133*60)
#define Z_CLEARANCE_BETWEEN_PROBES 5
#define MANUAL_PROBE_START_Z 1.0
typedef uint32_t millis_t;
#define SEC_TO_MS(N) millis_t((N)*1000UL)
