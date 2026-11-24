#pragma once
// REFERENCIA (firmware unico): el firmware YA NO incluye este archivo. Los valores de fabrica del Aquila D1 estan en la tabla de
// Marlin/src/lcd/extui/mks_ui/arquis_machine_logic.cpp; tests/machines/test_profiles.py comprueba que coinciden con
// Configuration.h + este archivo. Tambien lo usa herramientas/torres (limites del generador de torres).
/**
 * Perfil Aquila D1 (cama 230x240, Z 240). Valores tomados de su Configuration.h (Aquila_D1.zip): SOLO lo que difiere del XQ1.
 * Lo que no aparece aqui (placa, TMC2209, pantalla, PID, input shaping, corrientes...) coincide con el XQ1 y se hereda;
 * si en su maquina es distinto, anadelo aqui (nunca en Configuration.h). Lista completa de valores permitidos: docs/arquis/MAQUINAS.md.
 */
#define CUSTOM_MACHINE_NAME "Aquila D1"

// Geometria
#define X_BED_SIZE 230
#define Y_BED_SIZE 240
#define Z_MAX_POS  240

// Movimiento
#define DEFAULT_AXIS_STEPS_PER_UNIT   { 160, 160, 803.41, 781.08 }
#define DEFAULT_MAX_ACCELERATION      { 4500, 3500, 150, 10000 }
#define INVERT_X_DIR false
#define INVERT_Y_DIR false
#define INVERT_E0_DIR true

// Finales de carrera
#define X_MIN_ENDSTOP_INVERTING false
#define Y_MIN_ENDSTOP_INVERTING false
