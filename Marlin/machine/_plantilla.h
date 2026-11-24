#pragma once
/**
 * Anadir OTRA impresora al firmware unico (ARQUIS). Ver docs/arquis/MAQUINAS.md.
 * Ya no se compila un BIN por maquina: los modelos son filas de la tabla `profiles` de
 * Marlin/src/lcd/extui/mks_ui/arquis_machine_logic.cpp y se eligen en la pantalla (asistente o Ajustes > Maquina).
 *   1) Copia este archivo como Marlin/machine/<nombre>.h con SOLO los valores que difieren del XQ1 (referencia documentada).
 *   2) Anade una fila a `profiles` (nombre, medidas, logica de finales, direcciones, valores mecanicos) y sube ARQ_MC_NPROF.
 *   3) Si la maquina es mas grande que 300 x 300 x 450, sube X_BED_SIZE / Y_BED_SIZE / Z_MAX_POS en Configuration.h
 *      (son la envolvente de compilacion: la maquina mas grande).
 *   4) Amplia tests/machines/test_profiles.py para comparar la fila nueva con su archivo de referencia.
 * Lo que NO puede cambiar entre modelos (es comun en el BIN): placa, drivers TMC2209 por UART, micropasos, chopper, termistores,
 * temperaturas maximas, protecciones termicas, homing al minimo con origen 0.
 */
