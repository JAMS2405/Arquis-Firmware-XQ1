# ARQUIS Slicer Suite — Estado del proyecto frente al PROMPT MAESTRO

Actualizado el 2026-11-19 (tercera revisión: **Etapa 3 cerrada por decisión del propietario** y autorizada la **Etapa 5**). **Documento de estado: no cambia código ni configuración.**
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

**Orientación confirmada por el propietario (2026-11-19):** «la rotación está bien, yo veo todo bien». La base es la impresión de los seis engranajes, cuya disposición es asimétrica (1 y 3 arriba, 2-4-6 en medio, 5 abajo): un espejo o un giro del mapa se notaría. **`ORG=FL` pasa a «confirmado»** (`origin_confirmed: true` en `shared/machines/arquis_xq1.json`, `ORIGIN_CONFIRMED = True` en `machine_profile.py`, y el mapa de Cura ya no dice «sin confirmar»). **Matiz que debe quedar escrito:** no se imprimió la prueba formal de 5 piezas (`make_orientation_gcode.py`); la confirmación es por observación del propietario. Ningún eje se invirtió.

**Conocido y fuera de alcance de esta fase:** las **miniaturas** no son un requisito. Los G-code de muestra no las traen y el repositorio no incluye un generador (ver §6).

## 2. Estado por etapa del PROMPT MAESTRO

| Etapa | Implementado | Simulación / sintética | Comprobado físicamente | Cerrada |
|---|---|---|---|---|
| **0** Auditoría y arquitectura | Sí (`00_etapa0_auditoria.md`, `01_arquitectura_repositorio.md`, `02_capacidad_30.md`, `06_decisiones.md`) | n/a | n/a | **Sí** |
| **1** ARQUIS M486 Engine | Sí (`m486_engine.py`, `ARQUIS_M486.py`, `ARQUIS_Validation.py`) | Sí: modelo ejecutable de Marlin y G-code real de Cura procesado fuera de Cura | **En Cura 5.11 real: sí** (el script cargó, exportó `;ARQUIS_M486 … STATUS=OK` con 6 `M486 S0…S5` y el validador lo da por correcto). **En la impresora: no se ha cancelado ningún objeto** | **Cerrada** (por el propietario), salvo la cancelación real en la impresora, **diferida** (§3) |
| **2** Object Manager y manifiesto | Sí (`manifest_engine.py`, `object_manager_model.py`, `BedPreview.qml`, `shared/object_protocol/`) | Sí: 46 vectores del manifiesto, 391 del mapa, QML con Qt real | **En Cura real: sí** (captura, 6 duplicados, posiciones, manifiesto válido = escena; ventana del gestor abre con mapa y lista). **En la impresora:** el manifiesto generado se leyó y se mostró bien. Orientación `ORG=FL` **confirmada por el propietario**. Sin ejecutar en Cura real: perfil 450 × 450, rotaciones, grupos, modelos pequeños, clic mapa↔fila dentro de Cura | **Cerrada** (por el propietario); esos casos quedan como pruebas diferidas |
| **3** Firmware y TFT | Sí (M486 con rango 0–29, lector de manifiesto en SD, mapa y lista de 30, puerta de cancelación visual) | Sí: motor M486 real en el PC, parser contra la referencia, vista con LVGL real en ARM/qemu | **Sí, la parte de visualización con manifiesto** (§1), incluido el bloqueo del botón. Orientación confirmada por el propietario. **No probado:** cancelación real, Z/E, rechazo de `M486` fuera de rango en la placa (T6) | **CERRADA** (por el propietario, 2026-11-19). **La cancelación visual con manifiesto sigue BLOQUEADA** (`ARQ_OBJ_VISUAL_CANCEL_VALIDATED 0`) y su validación física queda **diferida** |
| **4** Calibration Suite | **No** (`calibration_engine.py` no existe) | — | — | **Aplazada por el propietario** (se pasa a la Etapa 5; se retomará después) |
| **5** Non-Planar Engine | **En curso (autorizada 2026-11-19).** Ver `09_etapa5_nonplanar.md`. Desactivado por defecto | Ver ese documento | **Ninguna** | **En curso** |
| **6** Distribución | **No** (paquete/instalador de la suite). *No confundir con la «etapa 6» del trabajo de interfaz v1.0.0, que es otra cosa* | — | — | **No iniciada** |

## 3. Cierre de la Etapa 3: qué se aceptó y qué queda diferido

El propietario cerró la Etapa 3 el 2026-11-19. **Cerrar la etapa no equivale a haber validado la cancelación real**: esa parte **no se ha probado en la impresora** y se registra como diferida. La puerta `ARQ_OBJ_VISUAL_CANCEL_VALIDATED` **sigue en 0**; con manifiesto, la pantalla muestra «Cancelación bloqueada». Los 7 puntos que existían:

| # | Punto | Estado al cerrar |
|---|---|---|
| 1 | Orientación física del mapa | **Confirmada por el propietario** (observación de la impresión de 6 piezas; sin la prueba formal de 5 piezas) |
| 2 | Correspondencia IDs Cura ↔ G-code ↔ marcadores | **Verificada** con datos reales (captura de Cura, `arquis_check`, y la pantalla muestra el mismo reparto). *Numeración: la pantalla y el gestor numeran 1–6; el G-code usa `M486 S0…S5`; número mostrado = `S` + 1* |
| 3 | Selección de objetos idénticos | **Aceptada por el propietario** (selección y zoom correctos sobre la disposición real) |
| 4 | Cancelación individual real | **DIFERIDA: no probada** |
| 5 | Las demás piezas siguen imprimiéndose | **DIFERIDA: no probada** |
| 6 | Seguridad de movimientos Z/E | **DIFERIDA: no probada** (el riesgo R1 de la Etapa 0 no se ejerce con G-code real de Cura, pero tampoco se ha visto en la placa) |
| 7 | Habilitar la cancelación visual | **NO habilitada.** Se hará solo tras #4–#6 |

**Para retomar la cancelación real** (cuando el propietario quiera): compilación de prueba con la puerta en 1, trabajo barato de 6 piezas pequeñas, mano cerca del paro, cancelar una pieza en capa baja y otra a media altura; no usar una pieza valiosa hasta superar #5.

**Pruebas diferidas que no bloquean ninguna etapa:**
- Cura 5.11 real: perfil 450 × 450, clic mapa↔fila dentro de Cura, rotaciones, grupos y modelos pequeños.
- Pruebas T1–T8 (`07_etapa3_firmware.md` §6): no se marcan como superadas las que el propietario no informó; T6 (`M486` fuera de rango por USB) sin resultado.
- Decisión abierta: si, sin manifiesto, la pantalla también debe bloquear la cancelación (hoy conserva el comportamiento anterior, con confirmación). Un cambio de una línea en `can_act_from_screen()`.
- Observación de diseño: el orden de impresión de los seis es 2-1-3-4-5-6 (`order` del manifiesto), distinto del ID.

## 4. Mejoras futuras (no bloquean ninguna etapa)

- Miniaturas en el G-code (formato MKS `;simage`/`;gimage`): decidir el generador (plugin de terceros o propio) y comprobar que el manifiesto no desplaza `;simage:` fuera de los primeros 512 B.
- Cancelar también soportes, brim y torres de purga junto a su objeto (hoy son `NONMESH` y no se cancelan).
- Tiempo de carga del manifiesto y retraso entre elegir el archivo y poder imprimir (T8) con medición física.
- Revisión de `Z_MAX_POS 450` frente a cama 300 × 300 (solo anotado; no se ha tocado).

## 5. Próxima etapa autorizada

**Etapa 5: Non-Planar Engine** (autorizada por el propietario el 2026-11-19, **saltando la Etapa 4**, que queda aplazada). Alcance autorizado por el PROMPT MAESTRO: *análisis y generación experimental; prueba controlada con vista previa y validación*. Reglas: desactivado por defecto, no se modifica el núcleo de Marlin, no se genera G-code no planar de una impresión completa sin vista previa y validación, ejemplo pequeño y controlado, limitaciones documentadas. Detalle en [`09_etapa5_nonplanar.md`](09_etapa5_nonplanar.md). Después: Etapa 4 (si el propietario la retoma) y Etapa 6 (distribución), cada una con autorización explícita.

## 6. Tabla general de avance del PROMPT MAESTRO

| Etapa | Alcance | Implementación | Pruebas en PC | Prueba física | Estado global |
|---|---|---|---|---|---|
| 0 | Auditoría y arquitectura | 100 % | n/a | n/a | **Cerrada** |
| 1 | ARQUIS M486 Engine (Cura) | Hecha | Sintéticas y con G-code real | **Validada en Cura 5.11 real** (exporta y verifica). Sin cancelación real en la impresora | **Cerrada** (cancelación real diferida) |
| 2 | Object Manager y manifiesto | Hecha | Vectores compartidos, QML, G-code real | **Validada en Cura 5.11 real** (6 copias, manifiesto) y **leída por la impresora**. Orientación confirmada por el propietario | **Cerrada** |
| 3 | Firmware y TFT | Hecha (compila: Flash 87,2 %, RAM 62,8 %) | Simulación con motor real, parser y LVGL | **Visualización probada** (mapa, lista, zoom, páginas, 6 idénticos) y orientación confirmada. **Cancelación real: diferida, no probada** | **Cerrada** (cancelación visual **bloqueada**) |
| 4 | Calibration Suite | 0 % | — | — | **Aplazada** (decisión del propietario) |
| 5 | Non-Planar Engine | En curso (ver `09_etapa5_nonplanar.md`) | Ver ese documento | Ninguna | **En curso** (desactivado por defecto) |
| 6 | Distribución | 0 % | — | — | **No iniciada** |

## 7. Plan de continuación

1. **Etapa 5 (en curso):** motor experimental `nonplanar_engine.py`, ejemplo pequeño y controlado, vista previa, verificación de límites y colisiones, exportación bloqueada hasta tener vista previa aceptada y parámetros del cabezal confirmados (`09_etapa5_nonplanar.md`).
2. **Cuando el propietario lo decida:** validar físicamente la cancelación (puntos #4–#6 de §3) y solo entonces cambiar la puerta a 1.
3. **Después de la Etapa 5:** Etapa 4 (Calibration Suite) si se retoma, y Etapa 6 (distribución), con autorización explícita.
