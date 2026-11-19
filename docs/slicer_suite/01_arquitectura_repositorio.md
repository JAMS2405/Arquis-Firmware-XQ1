# Organización del repositorio

Un único repositorio, tres bloques que no se mezclan:

```
(raíz)
├── Marlin/ ini/ buildroot/ config/ platformio.ini …   ← FIRMWARE (donde ya estaba; no se ha movido ni un archivo)
├── herramientas/ sd_assets/ docs/arquis/ CAMBIOS.md   ← herramientas y documentación del firmware (sin cambios)
│
├── cura/                       ← MÓDULO DE CURA (independiente)
│   ├── plugins/ARQUISPlugin/   plugin de Cura 5.11: engines/ (Python puro), adapters/ (única capa que importa Cura), …
│   ├── scripts/                scripts de post-procesado (adaptadores finos sobre los motores)
│   ├── tests/                  pruebas del módulo (pytest, sin Cura)
│   ├── pyproject.toml  requirements-dev.txt  README.md
│
├── shared/object_protocol/     ← CONTRATO entre Cura y firmware: especificación, límites, (esquema y vectores de prueba en la Etapa 2)
├── docs/slicer_suite/          ← documentación de la suite
└── tests/                      ← reservado para pruebas que cruzan módulos (conformidad del protocolo con vectores compartidos)
```

**Por qué no hay carpeta `firmware/`:** `platformio.ini` fija `src_dir = Marlin` y los scripts de `herramientas/` apuntan a `Marlin/src/...`. Mover el firmware a `firmware/` rompería la compilación y el historial sin ningún beneficio; "firmware" es, por definición, lo que ya estaba en la raíz. Tampoco hay `docs/` duplicado: `docs/arquis/` sigue siendo del firmware y `docs/slicer_suite/` de la suite.

## Reglas de separación
1. **Compilación del firmware:** PlatformIO solo compila `Marlin/` (`src_dir`). `cura/`, `shared/` y `tests/` no participan ni se referencian desde ningún `.ini`. No se ha tocado `platformio.ini` ni ningún archivo del firmware.
2. **Pruebas del módulo de Cura:** `cd cura && python -m pytest tests` (Python ≥ 3.12, solo `pytest`). No necesitan Cura, PlatformIO ni la impresora.
3. **Dependencias:** el plugin de Cura no depende de nada fuera de `cura/plugins/ARQUISPlugin/` en tiempo de ejecución (se instala copiando esa carpeta). Las constantes compartidas viven en `shared/object_protocol/limits.json` y se **copian** al plugin (`engines/protocol_limits.py`); una prueba (`test_protocol_limits.py`) falla si divergen.
4. **El contrato es `shared/object_protocol/`:** el firmware y Cura solo coinciden en ese protocolo (versionado `ARQUIS_OBJECTS_V1`), nunca en código.
5. **Cambios en el firmware** (Etapa 3) irán en commits propios y pequeños, separados de los del módulo de Cura.
