#pragma once
/**
 * PLANTILLA de perfil de maquina. Para una impresora nueva:
 *   1) copia este archivo como Marlin/machine/<nombre>.h y deja SOLO las lineas que difieran del XQ1;
 *   2) anade en Marlin/machine/machine.h:  #elif defined(ARQUIS_MACHINE_<NOMBRE>)  #include "<nombre>.h"
 *   3) anade el entorno en ini/arquis.ini (copia [env:aquila_d1] y cambia el nombre y el -D).
 * Valores que un perfil puede definir (los protegidos con #ifndef en Configuration.h / Configuration_adv.h):
 *  Configuration.h:     CUSTOM_MACHINE_NAME X_BED_SIZE Y_BED_SIZE X_MIN_POS Y_MIN_POS Z_MIN_POS Z_MAX_POS
 *                       DEFAULT_AXIS_STEPS_PER_UNIT DEFAULT_MAX_FEEDRATE DEFAULT_MAX_ACCELERATION DEFAULT_ACCELERATION
 *                       DEFAULT_RETRACT_ACCELERATION DEFAULT_TRAVEL_ACCELERATION JUNCTION_DEVIATION_MM
 *                       INVERT_X_DIR INVERT_Y_DIR INVERT_Z_DIR INVERT_E0_DIR X_HOME_DIR Y_HOME_DIR Z_HOME_DIR
 *                       X_MIN_ENDSTOP_INVERTING Y_MIN_ENDSTOP_INVERTING Z_MIN_ENDSTOP_INVERTING Z_MIN_PROBE_ENDSTOP_INVERTING
 *                       TEMP_SENSOR_0 TEMP_SENSOR_BED HEATER_0_MAXTEMP BED_MAXTEMP
 *                       DEFAULT_KP DEFAULT_KI DEFAULT_KD DEFAULT_BED_KP DEFAULT_BED_KI DEFAULT_BED_KD
 *  Configuration_adv.h: SHAPING_FREQ_X SHAPING_FREQ_Y SHAPING_ZETA_X SHAPING_ZETA_Y CHOPPER_TIMING
 *                       X_CURRENT Y_CURRENT Z_CURRENT E0_CURRENT X_MICROSTEPS Y_MICROSTEPS Z_MICROSTEPS E0_MICROSTEPS
 * Si necesitas otro valor, protegelo en Configuration*.h con el mismo #ifndef (un cambio de 3 lineas) y ponlo en tu perfil.
 */
