#pragma once
/**
 * ARQUIS - firmware UNICO para Arquis XQ1 y Aquila D1 (un solo BIN). Ver docs/arquis/MAQUINAS.md.
 *
 * El modelo de maquina ya NO se elige al compilar: se elige en la pantalla (asistente del primer arranque o
 * Ajustes > Maquina) y se guarda en un registro ARQUIS de la EEPROM (Marlin/src/lcd/extui/mks_ui/arquis_machine*.cpp).
 *  - Valores de fabrica de cada modelo: tabla de arquis_machine_logic.cpp (tests/machines comprueba que coincide con
 *    Marlin/machine/xq1.h y Marlin/machine/aquila_d1.h, que quedan como REFERENCIA y ya no se incluyen).
 *  - Lo que se compila en Configuration.h / Configuration_adv.h es comun. Las medidas compiladas (300 x 300 x 450, la
 *    maquina mas grande) solo son la envolvente de compilacion; los limites reales los da el modelo en tiempo de ejecucion.
 *  - Direcciones: las compiladas son las del XQ1; otro sentido se obtiene con el bit GCONF.shaft del TMC2209.
 *
 * Logica de los finales de carrera MIN de X, Y y Z: en tiempo de ejecucion. endstops.cpp la usa como siempre
 * (READ(pin) != X_MIN_ENDSTOP_INVERTING); solo cambia que el valor sale del modelo elegido.
 */
#ifndef __ASSEMBLER__
  extern unsigned char arq_mc_es_inv;     // bit 0 = X, bit 1 = Y, bit 2 = Z (arquis_machine.cpp)
#endif
#define X_MIN_ENDSTOP_INVERTING ((arq_mc_es_inv & 0x01) != 0)
#define Y_MIN_ENDSTOP_INVERTING ((arq_mc_es_inv & 0x02) != 0)
#define Z_MIN_ENDSTOP_INVERTING ((arq_mc_es_inv & 0x04) != 0)
