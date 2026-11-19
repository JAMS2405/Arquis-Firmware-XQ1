# ARQUIS Slicer Suite — módulo de Cura

Módulo independiente (no se compila con el firmware) del repositorio ARQUIS. Cura 5.11.0, Python 3.12, plugin API 8.

**Licencia: LGPL-3.0-or-later** (`LICENSE` en esta carpeta; el firmware del repositorio sigue bajo GPL-3.0, ver `../LICENSE`). Cada archivo fuente lleva `SPDX-License-Identifier: LGPL-3.0-or-later`.

- `plugins/ARQUISPlugin/engines/` — motores en Python puro (no importan Cura): escaneo del G-code, correspondencia instancia↔tramo, M486, validación, modelo de Marlin para pruebas.
- `plugins/ARQUISPlugin/adapters/` — **única** capa que conoce la API de Cura/Uranium.
- `scripts/` — scripts de post-procesado de Cura (`ARQUIS_M486.py`, `ARQUIS_Validation.py`); solo adaptan los motores.
- `tests/` — pruebas (`pytest`); no necesitan Cura.
- `tools/` — `arquis_check.py` (valida un .gcode exportado), `make_orientation_gcode.py`, `verify_cura_capture.py` (verifica el volcado que genera el plugin dentro de Cura), `check_cura_api_source.py` (comprueba la API contra el código fuente 5.11.0).
- **`VALIDACION_EN_CURA.md`** — cómo validar el plugin en Cura real (hoy **no validado**).

```bash
cd cura
python3 -m venv .venv && . .venv/bin/activate
pip install -r requirements-dev.txt
python -m pytest tests
```
Instalación en Cura y validación: ver `docs/slicer_suite/03_etapa1_m486.md` §7. Contrato con el firmware: `shared/object_protocol/`. Organización del repositorio: `docs/slicer_suite/01_arquitectura_repositorio.md`.

Estado: **Etapas 1 y 2** implementadas (motor M486; gestor de objetos y manifiesto) y **validadas en Cura 5.11 real** con una escena de 6 copias y cama 300 × 300 (`cura/VALIDACION_EN_CURA.md`); orientación `ORG=FL` confirmada por el propietario. La Etapa 3 (firmware y TFT) está **cerrada**: visualización probada en la impresora; la cancelación visual sigue bloqueada y su prueba física, diferida. Etapa 5 (no planar) en curso, desactivada por defecto; Etapa 4 (calibración) aplazada; Etapa 6 (distribución) sin iniciar. Detalle: `docs/slicer_suite/08_estado_proyecto.md`.

## Instalación en Cura 5.11
1. Copia la carpeta `plugins/ARQUISPlugin/` a la carpeta de plugins de Cura (`%APPDATA%\cura\5.11\plugins\`, `~/.local/share/cura/5.11/plugins/` o `~/Library/Application Support/cura/5.11/plugins/`).
2. Copia `scripts/ARQUIS_M486.py` (y `ARQUIS_Validation.py`) a `…/cura/5.11/scripts/`. Reinicia Cura.
3. *Extensiones ▸ Post-procesado ▸ Modificar G-code ▸ Añadir un script ▸ **ARQUIS M486***; después **ARQUIS Validación**. *Extensiones ▸ ARQUIS ▸ Gestor de objetos…* abre el mapa y la lista.

## Herramientas (sin Cura)
- `tools/arquis_check.py archivo.gcode --bed 300x300` — comprueba un G-code exportado (M486 + manifiesto) como lo vería el firmware.
- `tools/make_orientation_gcode.py` — G-code de la prueba de orientación física (5 piezas).
- `tools/gen_shared_vectors.py [--check]` — regenera los vectores de `shared/object_protocol/test_vectors/`.

Pruebas de interfaz (opcional): `pip install PyQt6`; en Linux sin pantalla hacen falta `libegl1 libgl1` (usan Qt `offscreen`).
