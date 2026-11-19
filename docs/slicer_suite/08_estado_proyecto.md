# ARQUIS Slicer Suite — Estado del proyecto frente al PROMPT MAESTRO

Actualizado el 2026-11-19 (segunda revisión, con la captura real de Cura y la foto de la pantalla del propietario). **Documento de estado: no cambia código ni configuración.**
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

**Aclarado con la foto de la pantalla** (`img/fisico_tft_6_pinones.webp`): la prueba de los seis engranajes se hizo **con manifiesto**. Se ven los nombres («pinon rusvel 66 dien 4»), el aviso «Cancelación no validada» y el botón «Cancelación bloqueada», que solo aparecen en el modo con manifiesto verificado. Por tanto quedaron probados físicamente: lectura del manifiesto desde la SD, vinculación manifiesto ↔ archivo, nombres y estados, mapa y lista con seis copias idénticas, y **el bloqueo de la cancelación visual** (el botón está deshabilitado, como debe).

**Observación del propietario sobre la orientación:** «físicamente veo bien la posición de todo y el zoom funciona». La disposición de la prueba es asimétrica (1 y 3 arriba, 2-4-6 en medio, 5 abajo), así que un espejo o un giro del mapa se notaría; es una **evidencia favorable a `ORG=FL`**. Se registra como *observación*, **no** como prueba de orientación superada: la regla vigente exige la prueba formal (§3 #1) o que el propietario confirme expresamente que esta impresión la sustituye (pieza «5» hacia el frente de la máquina y pieza «6» a la derecha, vistas desde el frente). `ORG=FL` sigue marcado *provisional* y la puerta de cancelación sigue en 0.

**Conocido y fuera de alcance de esta fase:** las **miniaturas** no son un requisito. Los G-code de muestra no las traen y el repositorio no incluye un generador (ver §6).

## 2. Estado por etapa del PROMPT MAESTRO

| Etapa | Implementado | Simulación / sintética | Comprobado físicamente | Cerrada |
|---|---|---|---|---|
| **0** Auditoría y arquitectura | Sí (`00_etapa0_auditoria.md`, `01_arquitectura_repositorio.md`, `02_capacidad_30.md`, `06_decisiones.md`) | n/a | n/a | **Sí** |
| **1** ARQUIS M486 Engine | Sí (`m486_engine.py`, `ARQUIS_M486.py`, `ARQUIS_Validation.py`) | Sí: modelo ejecutable de Marlin y G-code real de Cura procesado fuera de Cura | **En Cura 5.11 real: sí** (el script cargó, exportó `;ARQUIS_M486 … STATUS=OK` con 6 `M486 S0…S5` y el validador lo da por correcto). **En la impresora: no se ha cancelado ningún objeto** | **Parcial**: falta la cancelación real (§3 #4–#6) |
| **2** Object Manager y manifiesto | Sí (`manifest_engine.py`, `object_manager_model.py`, `BedPreview.qml`, `shared/object_protocol/`) | Sí: 46 vectores del manifiesto, 391 del mapa, QML con Qt real | **En Cura real: sí** (captura, 6 duplicados, posiciones, manifiesto válido = escena; ventana del gestor abre con mapa y lista). **En la impresora:** el manifiesto generado se leyó y se mostró bien. Orientación: observación favorable, **sin confirmar formalmente**. Sin ejecutar en Cura real: perfil 450 × 450, rotaciones, grupos, modelos pequeños, clic mapa↔fila dentro de Cura | **Casi cerrada**: falta confirmar la orientación y esos casos |
| **3** Firmware y TFT | Sí (M486 con rango 0–29, lector de manifiesto en SD, mapa y lista de 30, puerta de cancelación visual) | Sí: motor M486 real en el PC, parser contra la referencia, vista con LVGL real en ARM/qemu | **Sí, la parte de visualización con manifiesto** (§1), incluido el bloqueo del botón. **No**: orientación formal, cancelación real, Z/E, rechazo de `M486` fuera de rango en la placa (T6) | **Parcial: visualización cerrada; cancelación visual NO** |
| **4** Calibration Suite | **No** (`calibration_engine.py` no existe) | — | — | **No iniciada** |
| **5** Non-Planar Engine | **No** (`nonplanar_engine.py` no existe). Debe quedar **desactivado por defecto** cuando exista | — | — | **No iniciada** |
| **6** Distribución | **No** (paquete/instalador de la suite). *No confundir con la «etapa 6» del trabajo de interfaz v1.0.0, que es otra cosa* | — | — | **No iniciada** |

## 3. Qué falta exactamente para cerrar la Etapa 3

La cancelación visual con manifiesto sigue **bloqueada** (`ARQ_OBJ_VISUAL_CANCEL_VALIDATED 0` en `draw_cancel_object.cpp`). El orden es obligatorio: cada punto depende del anterior.

| # | Pendiente | Cómo se prueba (resumen) | Criterio de aceptación | Estado |
|---|---|---|---|---|
| 1 | **Orientación física del mapa respecto a la cama** | Imprimir `python3 cura/tools/make_orientation_gcode.py --bed 300x300` y comparar con la pantalla | La pieza de 20 mm queda en la esquina que la pantalla marca como «X0 Y0» (abajo-izquierda, `ORG=FL`). Si no coincide: **no se invierte ningún eje automáticamente**; se decide con el resultado | **Pendiente** (hay una observación favorable del propietario; ver §1) |
| 2 | **Correspondencia entre IDs de Cura, G-code y marcadores TFT** | Con el plugin instalado, volcado de diagnóstico (`cura/VALIDACION_EN_CURA.md`), `verify_cura_capture.py` y `arquis_check.py` sobre el G-code exportado; comparar con las posiciones de la pantalla | Cada objeto de la escena de Cura ↔ su `M486 S<id>` ↔ su marcador, uno a uno, también para copias idénticas | **Parcial: cadena Cura → manifiesto → G-code verificada con datos reales** (captura y `arquis_check`: cada `S0…S5` cae dentro de la caja de su objeto; la pantalla muestra el mismo reparto). **Falta** la comprobación física pieza a pieza (#3). *Ojo con la numeración: la pantalla y el gestor de Cura numeran de 1 a 6; el G-code usa `M486 S0…S5`. El número mostrado = `S` + 1* |
| 3 | **Selección física de objetos idénticos** | Elegir en la pantalla una copia concreta de las 6 y confirmar que el marcador resaltado es la pieza que está en esa posición de la cama | El marcador elegido coincide con la pieza real, para las seis | **Pendiente** (el propietario ve la disposición correcta; falta resaltar cada marcador y confirmar que es esa pieza, p. ej. cancelando una en #4) |
| 4 | **Cancelación individual real** | Con la puerta en 1 en una compilación de prueba: cancelar **una** pieza en plena impresión | Se detiene **exactamente** esa pieza | **Pendiente** (depende de #1–#3) |
| 5 | **Las demás piezas siguen imprimiéndose** | Mismo trabajo que #4 hasta el final | Todas las demás terminan completas y sin defectos nuevos | **Pendiente** |
| 6 | **Seguridad de movimientos Z/E, sin interferencias** | Observación durante #4: altura de capa tras cancelar, extrusión continua, sin choque con piezas ya impresas. Revisar si aparece el riesgo R1 de la Etapa 0 (`G1 Z` dentro de un tramo omitido no mueve Z) | Z correcta capa a capa, sin sobre/subextrusión, sin colisiones. Con G-code real de Cura la subida de capa está fuera de los tramos de objeto (`04_cambio_capacidad_30.md` §3), pero **no se ha probado con la placa** | **Pendiente** |
| 7 | **Habilitar la cancelación visual** | Cambiar la puerta a 1 **solo** tras superar #1–#6 | Cancelación desde pantalla validada y documentada | **Bloqueada** hasta #1–#6 |

**Pruebas sugeridas para #4–#6:** empezar con un trabajo barato y corto (6 piezas pequeñas), con la mano cerca del paro, y cancelar una pieza en una capa baja. Repetir después a media altura. No imprimir una pieza valiosa hasta superar #5.

**Pendientes adicionales de la Etapa 3 (no de la cancelación):**
- **Cura 5.11 real (2026-11-19): hecho en su mayor parte.** Plugin instalado y cargado, captura real de 6 copias, posiciones, manifiesto válido y G-code verificado (`cura/VALIDACION_EN_CURA.md`, registro). **Falta:** perfil 450 × 450 en Cura real, clic mapa↔fila dentro de Cura, rotaciones/grupos/modelos pequeños.
- **Perfil ARQUIS 300 × 300 en Cura:** ya usado en la captura real (definición `creality_cr10max` renombrada «Arquis XQ1», 300 × 300). Los G-code de muestra antiguos usan CR-10 Max 450 × 450; con ese perfil el manifiesto se omite o el firmware lo rechaza (comportamiento seguro y esperado, verificado fuera de Cura).
- **Pruebas T1–T8** (`07_etapa3_firmware.md` §6): el propietario ha confirmado varias funciones (§1) pero no ha indicado qué casos concretos corresponden a T1–T8. **No se marca ninguna como superada** hasta que se informen, salvo la visualización de §1. T6 (`M486` fuera de rango por USB) y T7 (orientación) siguen sin resultado.
- **Observación de diseño:** el orden de impresión de los seis es 2-1-3-4-5-6 (`order` del manifiesto), distinto del ID; es normal (Cura imprime en su orden) y el estado «Imprimiendo» de la pantalla lo sigue.
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
| 1 | ARQUIS M486 Engine (Cura) | Hecha | Sintéticas y con G-code real | **Validada en Cura 5.11 real** (exporta y verifica). Sin cancelación real en la impresora | **Implementada; validada en Cura; sin cancelación real** |
| 2 | Object Manager y manifiesto | Hecha | Vectores compartidos, QML, G-code real | **Validada en Cura 5.11 real** (6 copias, manifiesto) y **leída por la impresora**. Orientación sin confirmar formalmente | **Casi cerrada** |
| 3 | Firmware y TFT | Hecha (compila: Flash 87,2 %, RAM 62,8 %) | Simulación con motor real, parser y LVGL | **Visualización probada** (mapa, lista, zoom, páginas, 6 idénticos). Cancelación y orientación **pendientes** | **Visualización cerrada; cancelación visual bloqueada** |
| 4 | Calibration Suite | 0 % | — | — | **No iniciada** |
| 5 | Non-Planar Engine | 0 % | — | — | **No iniciada** (desactivado por defecto) |
| 6 | Distribución | 0 % | — | — | **No iniciada** |

## 7. Plan de continuación

1. ~~Confirmar si los seis engranajes llevaban manifiesto~~ — **resuelto** (sí, ver §1).
2. ~~Instalar el plugin y ejecutar la guía~~ — **hecho en su mayor parte** (captura real validada). Opcional: repetir con el perfil 450 × 450 y probar clic mapa↔fila dentro de Cura.
3. **Orientación (#1):** confirmar expresamente que la impresión de los seis engranajes sustituye la prueba (pieza «5» al frente, «6» a la derecha, visto desde el frente) **o** imprimir `make_orientation_gcode.py`.
4. Con #1 y #2 resueltos: comprobar IDs ↔ marcadores (#2) y la selección de idénticos (#3) sobre la pieza real.
5. **Compilación de prueba con la puerta en 1** y cancelación individual en un trabajo barato (#4–#6), observando Z/E.
6. Solo si #1–#6 pasan: dejar la puerta en 1 de forma definitiva, actualizar este documento y cerrar la Etapa 3.
7. Pedir autorización para la **Etapa 4**.
