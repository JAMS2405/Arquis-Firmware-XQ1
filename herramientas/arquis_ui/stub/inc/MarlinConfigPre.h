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
#define ZERO(a) memset((void*)a, 0, sizeof(a))
#define TERN(O,A,B) B      // MULTI_VOLUME no esta activo en esta configuracion
