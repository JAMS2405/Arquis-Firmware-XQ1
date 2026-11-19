# ARQUIS Slicer Suite — módulo de Cura

Módulo independiente (no se compila con el firmware) del repositorio ARQUIS. Cura 5.11.0, Python 3.12, plugin API 8.

- `plugins/ARQUISPlugin/engines/` — motores en Python puro (no importan Cura): escaneo del G-code, correspondencia instancia↔tramo, M486, validación, modelo de Marlin para pruebas.
- `plugins/ARQUISPlugin/adapters/` — **única** capa que conoce la API de Cura/Uranium.
- `scripts/` — scripts de post-procesado de Cura (`ARQUIS_M486.py`, `ARQUIS_Validation.py`); solo adaptan los motores.
- `tests/` — pruebas (`pytest`); no necesitan Cura.

```bash
cd cura
python3 -m venv .venv && . .venv/bin/activate
pip install -r requirements-dev.txt
python -m pytest tests
```
Instalación en Cura y validación: ver `docs/slicer_suite/03_etapa1_m486.md` §7. Contrato con el firmware: `shared/object_protocol/`. Organización del repositorio: `docs/slicer_suite/01_arquitectura_repositorio.md`.

Estado: **Etapa 1** (M486 Engine). Etapas 2 (gestor de objetos y manifiesto), 4 (calibración) y 5 (no planar) pendientes.
