# Pruebas que cruzan módulos

- `capacity64/` — estudio de capacidad de 64 objetos: ejecuta el código REAL del motor M486 de Marlin (sin modificarlo) y prototipos aparte. `run.sh` (rápido) y `run_lvgl.sh` (descarga LVGL). Resultados y análisis: `docs/slicer_suite/02_capacidad_64.md`.
- (Etapa 2) conformidad del protocolo: los mismos vectores de `shared/object_protocol/test_vectors/` los ejecutarán el generador de Cura y el parser del firmware.

Las pruebas propias del módulo de Cura están en `cura/tests/`; las del firmware, en `herramientas/pruebas/`.
