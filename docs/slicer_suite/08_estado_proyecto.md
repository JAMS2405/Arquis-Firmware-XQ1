# ARQUIS Slicer Suite — Estado del proyecto frente al PROMPT MAESTRO

Actualizado el 2026-11-19 a partir de lo que el propietario ha probado en la impresora. **Documento de estado: no cambia código ni configuración.**
Rama: `claude/dreamy-allen-gc05as` (la rama estable `main` no se ha tocado; `platformio.ini` e `ini/` sin diferencias respecto a `main`).

> **Aviso de nomenclatura.** En `docs/arquis/` hay documentos llamados `ARQUIS_UI_etapa_1…6`. Son las etapas de un trabajo **anterior y distinto**: el rediseño de la interfaz TFT (v1.0.0). **No son** las etapas del PROMPT MAESTRO de la Slicer Suite. Cuando este repositorio dice «Etapa N» sin más, se refiere al PROMPT MAESTRO. Los números de memoria «base de la Etapa 6» que aparecen en la Etapa 3 vienen de ese trabajo de interfaz.

> **Leyenda de evidencia** (la misma de `07_etapa3_firmware.md`):
> **[SINTÉTICA]** código o modelo ejecutado en el PC con datos fabricados · **[SIMULACIÓN]** código real del firmware ejecutado en PC/ARM-qemu, sin la placa · **[LECTURA]** se comprobó que algo existe, sin ejecutarlo · **[COMPILADO]** `pio run` correcto en el PC del propietario · **[FÍSICA]** probado en la impresora por el propietario.
> Una función solo se declara **validada** con evidencia [FÍSICA] (y, en Cura, ejecutada dentro de Cura real). Compilar no equivale a que funcione.

## 1. Funciones comprobadas por el propietario en la impresora [FÍSICA]

Confirmadas por el propietario el 2026-11-19. No se repiten.

| Función | Estado |
|---|---|
| Interfaz TFT ARQUIS | **Funciona** |
| Lectura de archivos desde la tarjeta SD | **Funciona** |
| Información de impresión y temperaturas | **Funciona** |
| Acceso al administrador de objetos | **Funciona** |
| Mapa de distribución de objetos | **Funciona** |
| Lista de objetos y selección sincronizada mapa ↔ lista | **Funciona** |
| Zoom ×1, ×2 y ×4 | **Funciona** |
| Navegación entre páginas de la lista | **Funciona** |
| Seis engranajes geométricamente idénticos, mostrados como objetos independientes | **Funciona** |

Además, medido en el PC del propietario [COMPILADO]: `pio run` correcto para `mks_robin_nano_v3_1_usb_flash_drive_msc`, **Flash 87,2 % (414 460 B)** y **RAM 62,8 % (82 304 B)**, dentro de los límites (< 90 % / < 70 %).

**Dato por confirmar:** la prueba de los seis engranajes no indica si el G-code llevaba **manifiesto** (nombres «engranaje 1…6», cama comprobada) o si la pantalla trabajó en **modo anterior** (posiciones aprendidas durante la impresión). Cambia qué se ha validado: con manifiesto se valida lector de SD, vinculación manifiesto ↔ archivo y nombres; sin él, solo el mapa con posiciones aprendidas. Hasta saberlo, solo se da por validado lo que es cierto en ambos casos.

**Conocido y fuera de alcance de esta fase:** las **miniaturas** no son un requisito. Los G-code de muestra no las traen y el repositorio no incluye un generador (ver §6).

## 2. Estado por etapa del PROMPT MAESTRO

| Etapa | Implementado | Simulación / sintética | Comprobado físicamente | Cerrada |
|---|---|---|---|---|
| **0** Auditoría y arquitectura | Sí (`00_etapa0_auditoria.md`, `01_arquitectura_repositorio.md`, `02_capacidad_30.md`, `06_decisiones.md`) | n/a | n/a | **Sí** |
| **1** ARQUIS M486 Engine | Sí (`m486_engine.py`, `ARQUIS_M486.py`, `ARQUIS_Validation.py`) | Sí: modelo ejecutable de Marlin y G-code real de Cura procesado fuera de Cura | **No**: no se ha cancelado ningún objeto de verdad. El script carga en Cura 5.11 real (el fallo de `description` se corrigió el 2026-11-19; los avisos posteriores se debían a que el plugin no estaba instalado) | **Parcial**: falta cancelación real y validación del plugin en Cura (§3) |
| **2** Object Manager y manifiesto | Sí (`manifest_engine.py`, `object_manager_model.py`, `BedPreview.qml`, `shared/object_protocol/`) | Sí: 46 vectores del manifiesto, 391 del mapa, QML con Qt real | **No** (la prueba física fue de la pantalla, Etapa 3). Orientación **sin confirmar**. Plugin **sin validar en Cura real** | **Parcial** |
| **3** Firmware y TFT | Sí (M486 con rango 0–29, lector de manifiesto en SD, mapa y lista de 30, puerta de cancelación visual) | Sí: motor M486 real en el PC, parser contra la referencia, vista con LVGL real en ARM/qemu | **Sí, la parte de visualización** (§1). **No**: orientación, correspondencia de IDs, cancelación real, Z/E | **Parcial: visualización cerrada; cancelación visual NO** |
| **4** Calibration Suite | **No** (`calibration_engine.py` no existe) | — | — | **No iniciada** |
| **5** Non-Planar Engine | **No** (`nonplanar_engine.py` no existe). Debe quedar **desactivado por defecto** cuando exista | — | — | **No iniciada** |
| **6** Distribución | **No** (paquete/instalador de la suite). *No confundir con la «etapa 6» del trabajo de interfaz v1.0.0, que es otra cosa* | — | — | **No iniciada** |

## 3. Qué falta exactamente para cerrar la Etapa 3

La cancelación visual con manifiesto sigue **bloqueada** (`ARQ_OBJ_VISUAL_CANCEL_VALIDATED 0` en `draw_cancel_object.cpp`). El orden es obligatorio: cada punto depende del anterior.

| # | Pendiente | Cómo se prueba (resumen) | Criterio de aceptación | Estado |
|---|---|---|---|---|
| 1 | **Orientación física del mapa respecto a la cama** | Imprimir `python3 cura/tools/make_orientation_gcode.py --bed 300x300` y comparar con la pantalla | La pieza de 20 mm queda en la esquina que la pantalla marca como «X0 Y0» (abajo-izquierda, `ORG=FL`). Si no coincide: **no se invierte ningún eje automáticamente**; se decide con el resultado | **Pendiente** |
| 2 | **Correspondencia entre IDs de Cura, G-code y marcadores TFT** | Con el plugin instalado, volcado de diagnóstico (`cura/VALIDACION_EN_CURA.md`), `verify_cura_capture.py` y `arquis_check.py` sobre el G-code exportado; comparar con las posiciones de la pantalla | Cada objeto de la escena de Cura ↔ su `M486 S<id>` ↔ su marcador, uno a uno, también para copias idénticas | **Pendiente** (depende de #1 y del plugin en Cura real) |
| 3 | **Selección física de objetos idénticos** | Elegir en la pantalla una copia concreta de las 6 y confirmar que el marcador resaltado es la pieza que está en esa posición de la cama | El marcador elegido coincide con la pieza real, para las seis | **Pendiente** (la visualización ya funciona; falta la correspondencia con la pieza física) |
| 4 | **Cancelación individual real** | Con la puerta en 1 en una compilación de prueba: cancelar **una** pieza en plena impresión | Se detiene **exactamente** esa pieza | **Pendiente** (depende de #1–#3) |
| 5 | **Las demás piezas siguen imprimiéndose** | Mismo trabajo que #4 hasta el final | Todas las demás terminan completas y sin defectos nuevos | **Pendiente** |
| 6 | **Seguridad de movimientos Z/E, sin interferencias** | Observación durante #4: altura de capa tras cancelar, extrusión continua, sin choque con piezas ya impresas. Revisar si aparece el riesgo R1 de la Etapa 0 (`G1 Z` dentro de un tramo omitido no mueve Z) | Z correcta capa a capa, sin sobre/subextrusión, sin colisiones. Con G-code real de Cura la subida de capa está fuera de los tramos de objeto (`04_cambio_capacidad_30.md` §3), pero **no se ha probado con la placa** | **Pendiente** |
| 7 | **Habilitar la cancelación visual** | Cambiar la puerta a 1 **solo** tras superar #1–#6 | Cancelación desde pantalla validada y documentada | **Bloqueada** hasta #1–#6 |

**Pruebas sugeridas para #4–#6:** empezar con un trabajo barato y corto (6 piezas pequeñas), con la mano cerca del paro, y cancelar una pieza en una capa baja. Repetir después a media altura. No imprimir una pieza valiosa hasta superar #5.

**Pendientes adicionales de la Etapa 3 (no de la cancelación):**
- **Cura 5.11 real:** instalar el plugin y completar `cura/VALIDACION_EN_CURA.md` (instalación y carga, captura de escena, duplicados, posiciones y dimensiones, sincronización mapa/lista, manifiesto). Los 33 símbolos de la API se comprobaron solo por lectura del código fuente.
- **Perfil ARQUIS 300 × 300 en Cura:** los G-code de muestra usan CR-10 Max 450 × 450; con ese perfil el manifiesto se omite o el firmware lo rechaza (comportamiento seguro y esperado).
- **Pruebas T1–T8** (`07_etapa3_firmware.md` §6): el propietario ha confirmado varias funciones (§1) pero no ha indicado qué casos concretos corresponden a T1–T8. **No se marca ninguna como superada** hasta que se informen, salvo la visualización de §1. T6 (`M486` fuera de rango por USB) y T7 (orientación) siguen sin resultado.
- **Decisión abierta:** si, sin manifiesto, la pantalla también debe bloquear la cancelación (hoy conserva el comportamiento anterior, con confirmación). Es un cambio de una línea en `can_act_from_screen()`.

## 4. Mejoras futuras (no bloquean ninguna etapa)

- Miniaturas en el G-code (formato MKS `;simage`/`;gimage`): decidir el generador (plugin de terceros o propio) y comprobar que el manifiesto no desplaza `;simage:` fuera de los primeros 512 B.
- Cancelar también soportes, brim y torres de purga junto a su objeto (hoy son `NONMESH` y no se cancelan).
- Tiempo de carga del manifiesto y retraso entre elegir el archivo y poder imprimir (T8) con medición física.
- Revisión de `Z_MAX_POS 450` frente a cama 300 × 300 (solo anotado; no se ha tocado).

## 5. Próxima etapa autorizada

**Ninguna etapa nueva está autorizada todavía.** Lo autorizado es completar únicamente las validaciones pendientes de la Etapa 3 (§3). Orden previsto por el PROMPT MAESTRO tras cerrarlas: **Etapa 4 (Calibration Suite) → Etapa 5 (Non-Planar Engine, desactivado por defecto) → Etapa 6 (distribución)**. Cada una necesita autorización explícita del propietario antes de empezar y sigue las reglas permanentes (rama estable intacta, `platformio.ini`/`ini/` solo con justificación, sin tocar el planificador sin necesidad, sin inventar APIs de Cura ni órdenes de Marlin, sin declarar validado lo no probado).

## 6. Tabla general de avance del PROMPT MAESTRO

| Etapa | Alcance | Implementación | Pruebas en PC | Prueba física | Estado global |
|---|---|---|---|---|---|
| 0 | Auditoría y arquitectura | 100 % | n/a | n/a | **Cerrada** |
| 1 | ARQUIS M486 Engine (Cura) | Hecha | Sintéticas y con G-code real procesado fuera de Cura | Sin cancelación real | **Implementada; sin validar en Cura/impresora** |
| 2 | Object Manager y manifiesto | Hecha | Vectores compartidos, QML, G-code real | Orientación sin confirmar | **Implementada; sin validar en Cura/impresora** |
| 3 | Firmware y TFT | Hecha (compila: Flash 87,2 %, RAM 62,8 %) | Simulación con motor real, parser y LVGL | **Visualización probada** (mapa, lista, zoom, páginas, 6 idénticos). Cancelación y orientación **pendientes** | **Visualización cerrada; cancelación visual bloqueada** |
| 4 | Calibration Suite | 0 % | — | — | **No iniciada** |
| 5 | Non-Planar Engine | 0 % | — | — | **No iniciada** (desactivado por defecto) |
| 6 | Distribución | 0 % | — | — | **No iniciada** |

## 7. Plan de continuación

1. **Confirmar** qué G-code se usó en la prueba de los seis engranajes (con o sin manifiesto) y, si se tiene, la compilación exacta.
2. **Instalar el plugin** `ARQUISPlugin` en Cura 5.11 y ejecutar `cura/VALIDACION_EN_CURA.md` con el perfil 300 × 300: devolver `arquis_cura_captura.json`, la salida de `verify_cura_capture.py` y una captura del gestor.
3. **Imprimir la prueba de orientación** (#1) y anotar la esquina de la pieza de 20 mm.
4. Con #1 y #2 resueltos: comprobar IDs ↔ marcadores (#2) y la selección de idénticos (#3) sobre la pieza real.
5. **Compilación de prueba con la puerta en 1** y cancelación individual en un trabajo barato (#4–#6), observando Z/E.
6. Solo si #1–#6 pasan: dejar la puerta en 1 de forma definitiva, actualizar este documento y cerrar la Etapa 3.
7. Pedir autorización para la **Etapa 4**.
