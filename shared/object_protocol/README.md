# Protocolo ARQUIS_OBJECTS (contrato Cura ↔ firmware)

- `limits.json` — **única fuente** de límites y constantes (capacidad de diseño 30 = IDs 0–29, techo actual del motor 32, límite del protocolo 128, unidades).
- `gen_limits.py` — genera de `limits.json` el módulo de Cura (`protocol_limits.py`) y `generated/arquis_object_limits.h` (para el firmware, Etapa 3). `--check` falla si algo está desactualizado (lo ejecuta una prueba).
- `ARQUIS_OBJECTS_V1.md` — especificación (borrador; la implementan la Etapa 2 en Cura y la Etapa 3 en el firmware).
- (Etapa 2) `arquis_objects_v1.schema.json` y `test_vectors/` — vectores compartidos que ejecutan tanto las pruebas de Cura como las del parser del firmware.
