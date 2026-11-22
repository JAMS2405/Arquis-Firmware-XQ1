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

## tests/machines (perfiles de máquina)
`python3 tests/machines/test_profiles.py`: con el preprocesador de gcc comprueba que el XQ1 queda idéntico a la configuración anterior a los perfiles, que Aquila D1 cambia exactamente lo que declara (y coincide con su `Configuration.h` original) y que cada valor de un perfil está protegido. Documentación: `docs/arquis/MAQUINAS.md`.
