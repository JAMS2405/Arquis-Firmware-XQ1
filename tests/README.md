# Pruebas que cruzan módulos

- `capacity/` — estudio de capacidad de 30 objetos: compila y ejecuta el motor M486 REAL de Marlin (sin modificarlo), prototipos aparte, tamaños y heap de LVGL. `run.sh` (rápido) y `run_lvgl.sh` (descarga LVGL). Resultados y análisis: `docs/slicer_suite/02_capacidad_30.md`. El arnés lo usan también las pruebas de `cura/tests/test_real_marlin_engine.py`.
- (Etapa 2) conformidad del protocolo: los mismos vectores de `shared/object_protocol/test_vectors/` los ejecutarán el generador de Cura y el parser del firmware.

Las pruebas propias del módulo de Cura están en `cura/tests/`; las del firmware, en `herramientas/pruebas/`.
