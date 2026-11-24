# Pruebas que cruzan módulos

- `capacity/` — estudio de capacidad de 30 objetos: compila y ejecuta el motor M486 REAL de Marlin (sin modificarlo), prototipos aparte, tamaños y heap de LVGL. `run.sh` (rápido) y `run_lvgl.sh` (descarga LVGL). Resultados y análisis: `docs/slicer_suite/02_capacidad_30.md`. El arnés lo usan también las pruebas de `cura/tests/test_real_marlin_engine.py`.
- Conformidad del protocolo: los vectores de `shared/object_protocol/test_vectors/` (417 casos) los cumple el parser de referencia de Cura (`cura/tests/test_shared_vectors.py`) y deberá cumplirlos el parser/mapa del firmware (Etapa 3).

Las pruebas propias del módulo de Cura están en `cura/tests/`; las del firmware, en `herramientas/pruebas/`.

## tests/firmware (Etapa 3)
`arq_cli.cpp` + `build_cli.sh` compilan el módulo REAL `Marlin/src/lcd/extui/mks_ui/arquis_objects.cpp` en el PC (opción `san`: ASan+UBSan) y lo exponen a las pruebas de Python (`cura/tests/test_firmware_manifest_parser.py`, `test_machine_profile.py`). `measure_size.sh` mide el tamaño ARM antes/después. `tests/capacity/stock_marlin/` guarda el motor M486 estable sin modificar para comparar.

## tests/mesh (nivelación manual 5×5)
`run.sh` ejecuta: (1) `test_config.py` — comprobaciones estáticas de configuración y de cómo Marlin se comporta (G28, EEPROM, signos); (2) `mesh_logic_test.cpp` — la lógica pura REAL (`arquis_mesh_logic.cpp`, ASan+UBSan); (3) `bilinear_harness.cpp` — la interpolación bilineal y el *fade* REALES de Marlin (texto extraído de `bbl.cpp`/`planner.h` al compilar). La pantalla se prueba con `herramientas/arquis_ui/sim/build_mesh_sim.sh` (LVGL real + capturas) y el pegamento con `check_marlin_api.sh` (firmas, no es una compilación de Marlin). Documentación: `docs/arquis/MALLA_5x5.md`.

## tests/pid (autoajuste PID)
`run.sh`: comprobaciones estáticas (configuración, parche de `temperature.cpp`, cancelación con `wait_for_heatup`, convención de M301/M304) y la lógica pura REAL (`arquis_pid_logic.cpp`). La pantalla: `herramientas/arquis_ui/sim/build_pid_sim.sh`. Documentación: `docs/arquis/PID.md`.

## tests/machines (firmware único XQ1 / Aquila D1)
`tests/machines/run.sh`:
- **`test_profiles.py`** (preprocesador de gcc, sin compilar Marlin):
  - la tabla de perfiles de fábrica coincide con la configuración real de cada máquina (`Configuration*.h` + `Marlin/machine/xq1.h` / `aquila_d1.h`);
  - la configuración común no cambia salvo lo previsto;
  - restricciones: `stepper.cpp`, el planificador, `endstops.cpp`, Input Shaping, la malla, el PID, el Wi-Fi y `temperature.cpp` siguen iguales que en `a98dd6a`; un solo entorno; USB solo CDC.
- **`machine_logic_test.cpp`** (lógica REAL de `arquis_machine_logic.cpp`, con ASan+UBSan): rangos, detección del modelo, registro EEPROM (CRC, dos copias, contador de secuencia, cortes de luz a mitad de escritura, migración) y máscara `shaft`.

La pantalla (asistente y Ajustes → Máquina) se prueba con `herramientas/arquis_ui/sim/build_machine_sim.sh` (LVGL real, toques simulados y capturas). Documentación: `docs/arquis/MAQUINAS.md`.

## tests/perf (pantalla Rendimiento)
`run.sh`: comprobaciones estáticas (los límites de la pantalla frente al código de Marlin, lo que guarda M500, integración) y la lógica pura REAL (`arquis_perf_logic.cpp`, ASan/UBSan). La pantalla: `herramientas/arquis_ui/sim/build_perf_sim.sh`. Documentación: `docs/arquis/RENDIMIENTO.md`.

## tests/torres (torres de prueba)
`python3 tests/torres/test_torres.py`: generador de G-code (`herramientas/torres/generar_torres.py`): límites frente a `Configuration.h` y `machine/aquila_d1.h`, tablas, alturas, extrusión, rechazos, verificación del texto y archivos listos reproducibles.

