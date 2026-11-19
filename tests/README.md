# Pruebas que cruzan módulos

- `capacity/` — estudio de capacidad de 30 objetos: compila y ejecuta el motor M486 REAL de Marlin (sin modificarlo), prototipos aparte, tamaños y heap de LVGL. `run.sh` (rápido) y `run_lvgl.sh` (descarga LVGL). Resultados y análisis: `docs/slicer_suite/02_capacidad_30.md`. El arnés lo usan también las pruebas de `cura/tests/test_real_marlin_engine.py`.
- Conformidad del protocolo: los vectores de `shared/object_protocol/test_vectors/` (417 casos) los cumple el parser de referencia de Cura (`cura/tests/test_shared_vectors.py`) y deberá cumplirlos el parser/mapa del firmware (Etapa 3).

Las pruebas propias del módulo de Cura están en `cura/tests/`; las del firmware, en `herramientas/pruebas/`.
