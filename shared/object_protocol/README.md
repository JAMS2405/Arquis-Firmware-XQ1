# Protocolo ARQUIS_OBJECTS (contrato Cura ↔ firmware)

- `limits.json` — constantes del protocolo (capacidad objetivo 64, límite de contadores de Marlin 127, unidades). Las lee el módulo de Cura (copia comprobada por prueba) y las leerá el firmware.
- `ARQUIS_OBJECTS_V1.md` — especificación (borrador; la implementan la Etapa 2 en Cura y la Etapa 3 en el firmware).
- (Etapa 2) `arquis_objects_v1.schema.json` y `test_vectors/` — vectores compartidos que ejecutan tanto las pruebas de Cura como las del parser del firmware.
