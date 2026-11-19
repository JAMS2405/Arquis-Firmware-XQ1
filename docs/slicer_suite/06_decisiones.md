# Decisiones confirmadas del propietario

Estas decisiones **no están pendientes**. Cualquier documento que las presente como pregunta abierta está desactualizado.

| # | Decisión | Dónde se aplica |
|---|---|---|
| 1 | **Manifiesto `ARQUIS_OBJECTS_V1` al final del G-code, con marcador corto en la cabecera** | `shared/object_protocol/ARQUIS_OBJECTS_V1.md` §1; generador/lector en `cura/`; lector del firmware (Etapa 3) busca en los últimos 16 KB |
| 2 | **Licencia del módulo de Cura: LGPL-3.0-or-later** | `cura/LICENSE`, cabeceras SPDX en los `.py` de `cura/` y `shared/object_protocol/`. El firmware sigue bajo GPL-3.0 (`LICENSE` raíz) |
| 3 | **Capacidad de diseño: 30 objetos** (IDs 0–29), máscara `uint32_t` | `shared/object_protocol/limits.json` (única fuente) |
| 4 | **Un único repositorio ARQUIS**; Cura es un módulo independiente (`cura/`), sin mezclar código ni compilación con el firmware | `01_arquitectura_repositorio.md` |

## Condiciones vigentes (mensaje del 2026-11-19)
- **Orientación `ORG=FL`:** era provisional; **confirmada por el propietario el 2026-11-19** (ver el final de este documento). No se invierten ejes automáticamente. La cancelación visual sigue sin habilitarse hasta validarla físicamente.
- **Cama:** los G-code de muestra usaban un perfil CR-10 Max (450 × 450 mm); la máquina es de **300 × 300 mm**. Un manifiesto cuya cama no coincida con la del firmware **no habilita mapa ni cancelación visual**.
- **Cancelación desde la pantalla protegida:** con manifiesto, la acción de cancelar/restaurar sale **deshabilitada** mientras no se compile con `ARQ_OBJ_VISUAL_CANCEL_VALIDATED 1` (ver `07_etapa3_firmware.md`), que solo se cambia tras la prueba física de orientación y la validación dentro de Cura real. Sin manifiesto, la pantalla conserva el comportamiento ya existente.
- **Cura real:** la integración con Cura 5.11 no se considera terminada hasta ejecutar el plugin en el programa real (ver `cura/VALIDACION_EN_CURA.md`). Las pruebas con nodos falsos no validan la API.
- Etapa 3 (firmware y TFT) autorizada en rama de desarrollo; rama estable intacta; `platformio.ini` e `ini/` solo con justificación y autorización.

## Actualización de estado (2026-11-19, después de probar en la impresora)
- El propietario **confirmó en la impresora**: interfaz TFT, lectura de SD, información de impresión y temperaturas, acceso al administrador de objetos, mapa, lista con selección sincronizada, zoom ×1/×2/×4, paginación y seis engranajes idénticos como objetos independientes. Detalle y límites en `08_estado_proyecto.md`.
- **Las miniaturas no son requisito de esta fase**; quedan como mejora futura.
- La cancelación visual **sigue bloqueada** (`ARQ_OBJ_VISUAL_CANCEL_VALIDATED 0`) hasta superar los pendientes #1–#6 de `08_estado_proyecto.md` §3.
- **No hay etapa nueva autorizada.** Tras cerrar la Etapa 3 el orden es Etapa 4 (Calibration Suite), 5 (Non-Planar, desactivado por defecto) y 6 (distribución), cada una con autorización explícita.
- **Cura real (2026-11-19):** captura hecha dentro de Cura 5.11 (`source: "cura"`) y G-code exportado verificados: validación en Cura real **cumplida** para captura de escena, duplicados, posiciones y manifiesto. La orientación sigue siendo independiente y **provisional** (`ORG=FL`): el propietario observa que la posición coincide, pero no se da por probada hasta que la confirme expresamente o imprima la prueba.

## Cierre de la Etapa 3 y autorización de la Etapa 5 (2026-11-19)
- **Etapa 3 cerrada por el propietario.** Orientación `ORG=FL` **confirmada** por observación de la impresión de 6 piezas (no se imprimió la prueba formal de 5 piezas). La cancelación real **no se ha probado** y queda diferida; `ARQ_OBJ_VISUAL_CANCEL_VALIDATED` **sigue en 0** (cancelación visual con manifiesto bloqueada).
- **Etapa 4 (Calibration Suite) aplazada** por el propietario.
- **Etapa 5 (Non-Planar Engine) autorizada.** Debe quedar desactivada por defecto, sin tocar el núcleo de Marlin y sin exportar G-code de una impresión completa sin vista previa y validación. Ver `09_etapa5_nonplanar.md`.

## Ondulado no planar: autorizaciones (2026-11-19)
- El objetivo del propietario con la impresión no planar es **crear volumen con capas onduladas para que las piezas sean más rígidas** (técnica de `makertum/non-planar-layer-fdm`; implementación propia, sin copiar su código AGPL). La rigidez **no está demostrada**: hace falta ensayar probetas.
- **El propietario autoriza que Claude ponga `head.confirmed: true`** (hecho, con la tolerancia del cono en 35°; las estimaciones que quedan están listadas en `09_etapa5_nonplanar.md` §15).
- **Autoriza la probeta de 30 mm** (la vista previa la acepta el propietario tecleando el código al exportar) y la **primera ejecución en seco**.
- **Autoriza integrarlo en Cura como script del plugin**, apagado por defecto y con el código de aceptación dentro de Cura. Pendiente de validar en Cura real.

