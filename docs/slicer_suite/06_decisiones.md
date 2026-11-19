# Decisiones confirmadas del propietario

Estas decisiones **no están pendientes**. Cualquier documento que las presente como pregunta abierta está desactualizado.

| # | Decisión | Dónde se aplica |
|---|---|---|
| 1 | **Manifiesto `ARQUIS_OBJECTS_V1` al final del G-code, con marcador corto en la cabecera** | `shared/object_protocol/ARQUIS_OBJECTS_V1.md` §1; generador/lector en `cura/`; lector del firmware (Etapa 3) busca en los últimos 16 KB |
| 2 | **Licencia del módulo de Cura: LGPL-3.0-or-later** | `cura/LICENSE`, cabeceras SPDX en los `.py` de `cura/` y `shared/object_protocol/`. El firmware sigue bajo GPL-3.0 (`LICENSE` raíz) |
| 3 | **Capacidad de diseño: 30 objetos** (IDs 0–29), máscara `uint32_t` | `shared/object_protocol/limits.json` (única fuente) |
| 4 | **Un único repositorio ARQUIS**; Cura es un módulo independiente (`cura/`), sin mezclar código ni compilación con el firmware | `01_arquitectura_repositorio.md` |

## Condiciones vigentes (mensaje del 2026-11-19)
- **Orientación `ORG=FL` es PROVISIONAL** hasta que el propietario imprima la prueba de orientación (`cura/tools/make_orientation_gcode.py`). No se invierten ejes automáticamente ni se habilita la cancelación visual hasta confirmar la correspondencia física.
- **Cama:** los G-code de muestra usaban un perfil CR-10 Max (450 × 450 mm); la máquina es de **300 × 300 mm**. Un manifiesto cuya cama no coincida con la del firmware **no habilita mapa ni cancelación visual**.
- **Cancelación desde la pantalla protegida:** con manifiesto, la acción de cancelar/restaurar sale **deshabilitada** mientras no se compile con `ARQ_OBJ_VISUAL_CANCEL_VALIDATED 1` (ver `07_etapa3_firmware.md`), que solo se cambia tras la prueba física de orientación y la validación dentro de Cura real. Sin manifiesto, la pantalla conserva el comportamiento ya existente.
- **Cura real:** la integración con Cura 5.11 no se considera terminada hasta ejecutar el plugin en el programa real (ver `cura/VALIDACION_EN_CURA.md`). Las pruebas con nodos falsos no validan la API.
- Etapa 3 (firmware y TFT) autorizada en rama de desarrollo; rama estable intacta; `platformio.ini` e `ini/` solo con justificación y autorización.

## Actualización de estado (2026-11-19, después de probar en la impresora)
- El propietario **confirmó en la impresora**: interfaz TFT, lectura de SD, información de impresión y temperaturas, acceso al administrador de objetos, mapa, lista con selección sincronizada, zoom ×1/×2/×4, paginación y seis engranajes idénticos como objetos independientes. Detalle y límites en `08_estado_proyecto.md`.
- **Las miniaturas no son requisito de esta fase**; quedan como mejora futura.
- La cancelación visual **sigue bloqueada** (`ARQ_OBJ_VISUAL_CANCEL_VALIDATED 0`) hasta superar los pendientes #1–#6 de `08_estado_proyecto.md` §3.
- **No hay etapa nueva autorizada.** Tras cerrar la Etapa 3 el orden es Etapa 4 (Calibration Suite), 5 (Non-Planar, desactivado por defecto) y 6 (distribución), cada una con autorización explícita.
