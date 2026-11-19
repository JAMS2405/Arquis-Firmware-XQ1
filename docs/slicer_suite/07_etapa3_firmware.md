# Etapa 3 — Firmware y pantalla (TS35): lector de manifiesto, mapa y lista de 30 objetos

Rama de desarrollo: `claude/dreamy-allen-gc05as` (la rama estable `main` **no se ha tocado**). **No se han modificado `platformio.ini` ni `ini/`.**
Autorizada por el propietario el 2026-11-19 con las condiciones de `06_decisiones.md`.

> **Leyenda de pruebas.** **[SINTÉTICA]** código real o modelo ejecutado en el PC con datos fabricados · **[SIMULACIÓN]** código real del firmware ejecutado en PC/ARM-qemu, sin la placa · **[LECTURA]** comprobación de que algo existe, sin ejecutarlo · **[FÍSICA]** en la impresora. **Hasta hoy no se ha hecho ninguna prueba [FÍSICA] de la Etapa 3**, ni se ha compilado el firmware completo con PlatformIO (no es posible en esta sesión: el núcleo STM32 de PlatformIO es un `.zip` de GitHub que el proxy bloquea).

## 1. Qué cambia (resumen)

| Condición del propietario | Qué se hizo | Estado |
|---|---|---|
| Rechazar `M486` fuera de 0–29, mantener `uint32_t` | Los índices se leen como `int32_t` (`parser.value_long()`) y se validan **antes** de estrechar; fuera de 0–29 no se activa ni se cancela nada (se marca `overflow`); `T` se acota a 30. Máscara `uint32_t` sin cambios; matrices de cajas de 32 → 30 | [SIMULACIÓN] motor real compilado en el PC: 0 alias en 65 548 valores (antes: 8 170 cancelaciones y 32 746 activaciones ajenas) |
| Leer el manifiesto sin interferir con G-code, miniaturas ni SD | Carga **solo lectura** al elegir archivo, **después** de la miniatura, un trozo de 512 B por paso (abre/lee/cierra, como la miniatura), nunca con un archivo abierto o una impresión en curso | [SIMULACIÓN] lógica; **[FÍSICA] pendiente** (lectura real de la SD) |
| Mapa y lista sincronizados en la TS35 | Vista nueva: mapa proporcional con zoom ×1/×2/×4, lista paginada (30 = 6 páginas), una sola selección | [SIMULACIÓN] LVGL 6.1.1-MKS real en ARM de 32 bits (qemu): 226 comprobaciones OK |
| Optimizar memoria, evitar copias | La vista guarda el **puntero** al modelo (antes copiaba ≈ 130 B en pila en cada refresco) y una firma de 32 bits; 6 objetos LVGL fijos; heap constante | [SIMULACIÓN] pico 1 860 B con 0…30 objetos |
| Impresiones normales sin manifiesto | Sin manifiesto: comportamiento anterior (posiciones aprendidas, cancelar con confirmación) | [SIMULACIÓN] |
| No habilitar cancelación visual sin correspondencia validada | Puerta `ARQ_OBJ_VISUAL_CANCEL_VALIDATED = 0` por defecto; con manifiesto el botón sale **«Cancelación bloqueada»**; manifiesto inválido/incoherente también bloquea | [SIMULACIÓN] |

### Política de cancelación desde la pantalla
| Situación | Mapa | Cancelar / restaurar desde la pantalla |
|---|---|---|
| Sin manifiesto (G-code de siempre) | posiciones aprendidas al imprimir | **como antes** (con confirmación) — comportamiento ya existente, no nuevo |
| Manifiesto verificado **y** coherente con lo que se imprime | posiciones y nombres del manifiesto | **bloqueado** mientras `ARQ_OBJ_VISUAL_CANCEL_VALIDATED` sea 0 (valor entregado) |
| Manifiesto presente pero inválido (CRC, **cama distinta**, formato…) o incoherente (otro archivo, `M486 T` ≠ N, índice fuera de rango, caja real fuera de la del manifiesto, ORG ≠ FL) | solo lectura con las posiciones aprendidas | **bloqueado** |

> Decisión que puedes revertir: en el caso 1 conservé el comportamiento existente (la pantalla ya cancelaba por posiciones aprendidas y ese código ya estaba en la rama estable). La puerta nueva solo protege lo **nuevo** (posiciones que vienen del manifiesto). Si prefieres bloquear también el caso 1, es un cambio de una línea en `can_act_from_screen()`.

## 2. Archivos modificados y por qué

| Archivo | Cambio | Por qué |
|---|---|---|
| `Marlin/src/feature/cancel_object.h/.cpp` | índices `int32_t` + `in_range()`, `overflow`, `declared_count`, `z_print`, `set_object_count()`; matrices `[30]`; capacidad desde `shared/…/arquis_object_limits.h` | rechazo seguro; única fuente de límites |
| `Marlin/src/gcode/feature/cancel/M486.cpp` | `value_long()` en lugar de `value_int()` | `S256` activaba el objeto 0 (truncado a `int16`/`int8`) |
| `Marlin/src/gcode/motion/G0_G1.cpp` | 1 línea: Z de la última extrusión (`G1`) | estado Pendiente/Imprimiendo/Completado (no toca el planificador) |
| `Marlin/src/lcd/extui/mks_ui/arquis_objects.h/.cpp` (**nuevos**) | lector de manifiesto en streaming, CRC-32, mapa entero, estados, etiquetas, comprobación de cajas | lógica pura, probada en el PC contra los vectores compartidos |
| `…/arquis_objects_sd.h/.cpp` (**nuevos**) | carga polled desde la SD | no interferir con miniatura/impresión |
| `…/arquis_files.cpp` | arranca el cargador tras la miniatura; lo completa de forma síncrona al pulsar Imprimir | que el manifiesto esté listo antes de imprimir |
| `…/arquis_objects_view.h/.cpp` | vista de mapa + lista de 30 objetos | requisito principal |
| `…/draw_cancel_object.cpp` | controlador: origen de datos, confianza, puerta | seguridad |
| `shared/object_protocol/…`, `cura/…`, `tests/…`, `herramientas/…`, `docs/…` | vectores nuevos, parser de referencia endurecido, pruebas, simulador, medición | verificación |

Ninguno de `platformio.ini`, `ini/`, `Configuration*.h`, el planificador, el *stepper* ni la lectura de G-code/miniaturas existente.

## 3. Pruebas ejecutadas

| # | Prueba | Tipo | Resultado |
|---|---|---|---|
| 1 | Motor M486 **real** de Marlin (código del repositorio compilado en el PC): barrido de 65 536 valores `int16` + 12 de 32 bits, motor estable vs motor de esta rama | SIMULACIÓN | estable: 8 170 `P` y 32 746 `S` ajenos; **rama: 0 y 0**; 0–29 correctos |
| 2 | Motor real: cada uno de los 30 objetos, G-code ARQUIS sintético y **G-code real de Cura**, trazas idénticas al modelo | SIMULACIÓN | OK |
| 3 | Parser de manifiesto del firmware (C++ real, ASan+UBSan) vs parser de referencia: **64 vectores compartidos × 4 troceados** (1 B … 100 kB) | SIMULACIÓN | idénticos (ok + conjunto de errores) |
| 4 | Fuzz: 450 mutaciones con CRC roto (+4 500 de una sesión de exploración), **6 000 mutaciones estructurales con CRC válido** (llegan a todas las validaciones semánticas), bytes aleatorios | SIMULACIÓN | 0 divergencias, 0 errores de memoria/UB |
| 5 | Mapa de cama y estados: **391 vectores** (aritmética entera) | SIMULACIÓN | idénticos bit a bit |
| 6 | G-code real de Cura 5.11 (2 archivos): el firmware lee su manifiesto; con la cama de la máquina (300×300) lo **rechaza** (`E_BED_MISMATCH`, sin tabla) | SIMULACIÓN | OK |
| 7 | Comprobación en ejecución: cajas que Marlin aprendería del G-code real vs manifiesto: exceso ≤ 0,5 mm (margen 6 mm); manifiesto con Y reflejado → **bloquea** | SIMULACIÓN | OK |
| 8 | Vista LVGL 6.1.1-MKS **real**, ARM 32 bits en qemu: toques en lista, páginas, zoom, mapa (marcador de 6 mm = 22 px), botón bloqueado, redibujado mínimo, 600 ciclos sin fugas | SIMULACIÓN | **226 OK / 0 fallos** (todas las pantallas del simulador) |
| 9 | `arquis_objects*.cpp`, `arquis_files.cpp`, `draw_cancel_object.cpp`… contra firmas de Marlin (`arm-none-eabi-g++ -fsyntax-only`, cabeceras *stub*, `cancel_object.h` real) | LECTURA | 17/17 compilan (esto **no** es compilar Marlin) |
| 10 | Pruebas de Cura: 232 OK / 2 omitidas (Python 3.12); 234 OK con los G-code completos | SINTÉTICA/SIMULACIÓN | OK |
| 11 | Pruebas existentes de lógica (`herramientas/pruebas`) y capacidad (`tests/capacity/run.sh`), adaptadas al nuevo límite 30 (los índices 30 y 31 dejan de ser válidos: cambio **deliberado**) | SIMULACIÓN | OK |
| — | Compilación completa del firmware, ejecución en la placa, lectura real de la SD, tiempos de redibujado, tacto real | **FÍSICA** | **sin hacer** |

## 4. Memoria: antes y después

Línea base **real** de la Etapa 6 (compilación de PlatformIO del propietario): **Flash 406 800 B = 85,6 %**, **RAM 82 372 B = 62,8 %**.
Δ medido compilando los módulos afectados con `arm-none-eabi-g++ -Os` (Cortex-M4) **antes (rama `main`) y ahora** (`tests/firmware/measure_size.sh`; objetos, no enlace):

| Módulo | ANTES text+data / bss | AHORA text+data / bss |
|---|---|---|
| `arquis_objects` (lector, nuevo) | – | 4 385 / 1 324 |
| `arquis_objects_sd` (nuevo) | – | 570 / 17 |
| `arquis_objects_view` | 2 854 / 406 | 4 590 / 570 |
| `draw_cancel_object` | 1 072 / 185 | 2 168 / 279 |
| `arquis_files` | 605 / 16 | 653 / 16 |
| `cancel_object` + `M486` | 495 / 267 | 656 / 254 |
| **Total** | **5 026 / 874** | **13 022 / 2 460** |

**Δ Flash ≈ +8,0 KB; Δ RAM estática ≈ +1,6 KB** ⇒ **proyección: Flash ≈ 414 800 B ≈ 87,3 %** (criterio < 90 %), **RAM ≈ 83 960 B ≈ 64,1 %** (criterio < 70 %). Es una **proyección** sobre la base real, no una medición del firmware enlazado: la cifra real sale de `pio run` (apartado 6). Estructura real: `ArqObjRec` = 16 B, `ArqObjStore` (30 objetos, 30 nombres) = 1 136 B; estado del lector ≈ 190 B. (La estimación anterior de 14 B/1 054 B no incluía el número de copia, necesario para «Llavero 3».)
Heap de LVGL (16 KB, medido con LVGL real en ARM 32 bits): la pantalla de objetos usa **1 860 B de pico con 0, 3, 9, 16 y 30 objetos**; con 30 objetos, zoom ×4 y el diálogo de confirmación abiertos: **5 328 B usados, 11 056 B libres** (criterio ≥ 8 KB); 600 ciclos crear/destruir sin fugas. La vista anterior ya usaba 6 objetos; ya no copia el modelo.

## 5. Riesgos pendientes
1. **Sin compilación ni prueba en la placa.** Los módulos compilan contra *stubs* de las firmas de Marlin, no contra Marlin; puede haber errores de compilación (macros, `ENABLED()`) que solo `pio run` revela.
2. **Lectura real de la SD sin probar.** `openFileRead()` llama a `abortFilePrintNow()`: el cargador se niega a abrir nada si hay archivo abierto o impresión en curso, pero esa guarda solo se ha razonado leyendo el código. Tiempo de la carga (33 lecturas de 512 B) sin medir; si Imprimir se pulsa antes de acabar, se completa síncronamente.
3. **Orientación física `ORG=FL` sin confirmar** (por eso la cancelación visual con manifiesto está bloqueada).
4. **Integración con Cura sin validar en Cura real** (`cura/VALIDACION_EN_CURA.md`).
5. **Soportes, brim y torres** son `NONMESH`: no se cancelan con su objeto (limitación conocida).
6. **Impresión de un objeto cada vez (`SEQ=1`)**: el estado «Completado» usa el orden del último objeto activo; no probado con G-code real secuencial.
7. **Margen de 6 mm** de la comprobación en ejecución: calibrado con 2 G-code reales (exceso ≤ 0,5 mm); un perfil con anchos de extrusión o *brims* atípicos podría bloquear de más (falla del lado seguro).
8. **`Z_MAX_POS 450`** en `Configuration.h` frente a una cama de 300 × 300: no lo he tocado; solo lo anoto (el perfil de máquina de las pruebas lo refleja tal cual).
9. El defecto de `value_int()` truncando también puede existir en otros comandos de Marlin (no revisados; fuera de alcance).
10. `T`: un G-code ajeno con `M486 T40` se acota a 30; los objetos 30–39 se imprimen sin poder cancelarse (seguro).

## 6. Instrucciones exactas para probar en tu impresora

> Nada de esto se ha ejecutado aún. Hazlo con la impresora **fría y sin filamento** hasta el paso 4.

### 6.1 Compilar y medir (PC con PlatformIO)
```powershell
git fetch origin claude/dreamy-allen-gc05as
git checkout claude/dreamy-allen-gc05as
pio run -e mks_robin_nano_v3_1_usb_flash_drive_msc 2>&1 | Tee-Object -FilePath build.log
```
Anota **las líneas `RAM:` y `Flash:`** de PlatformIO y compáralas con la base (RAM 62,8 %, Flash 85,6 %). **Detente** (no flashees) si Flash ≥ 90 %, RAM ≥ 70 % o hay errores de compilación; en ese caso envíame `build.log`. Después sigue `docs/arquis/GUIA_INSTALACION.md` §3–§4 (la carpeta `assets` es la de `sd_assets\`; esta etapa no añade recursos).

### 6.2 Pruebas en la impresora (tras flashear)
| # | Prueba | Resultado esperado |
|---|---|---|
| T1 | Imprime un G-code **antiguo** (sin M486, sin manifiesto) | Igual que antes; el botón «Objetos» no aparece |
| T2 | Elige un G-code con `M486` pero **sin manifiesto** | Miniatura normal; en la impresión, «Objetos» muestra posiciones que van apareciendo; cancelar pide confirmación (comportamiento anterior) |
| T3 | Genera con Cura (perfil **300 × 300**, script ARQUIS M486) un G-code de 3 piezas y otro de 4 copias; elígelos | Miniatura normal y sin retraso apreciable; al imprimir, «Objetos» muestra **nombres** («pinon 1…4») y posiciones desde el principio, el aviso **«Cancelación no validada»** y el botón **«Cancelación bloqueada»** (deshabilitado) |
| T4 | Con T3 en marcha: pulsa filas, marcadores, `<` `>`, `x1/x2/x4` | La fila y el marcador se resaltan a la vez; la lista salta a la página del marcador; `x2/x4` centra el elegido |
| T5 | G-code exportado con el perfil **450 × 450** (los de muestra) | Aviso **«Cama del G-code distinta»**, posiciones aprendidas de solo lectura, botón bloqueado |
| T6 | Por USB (host/terminal) en plena impresión de T3: `M486 P256`, `M486 P30`, `M486 S300`, `M486 T200` | **Ningún** objeto aparece cancelado (`x`); la impresión sigue igual |
| T7 | Imprime `python3 cura/tools/make_orientation_gcode.py --bed 300x300` | La pieza de 20 mm debe quedar en la esquina **frontal izquierda**; en la pantalla, abajo-izquierda («X0 Y0»). Anota si coincide |
| T8 | Mide el tiempo entre elegir el archivo y ver el botón Imprimir activo, y entre Imprimir y empezar | Sin diferencia perceptible; anota cualquier retraso |

### 6.3 Habilitar la cancelación visual (solo después de T7 **y** `cura/VALIDACION_EN_CURA.md`)
En `Marlin/src/lcd/extui/mks_ui/draw_cancel_object.cpp` cambia `#define ARQ_OBJ_VISUAL_CANCEL_VALIDATED 0` por `1`, recompila y haz la prueba de correspondencia física: 5 piezas en esquinas y centro y 4 copias idénticas; cancela **una** desde la pantalla y comprueba que se detiene **exactamente esa** y que las demás terminan. Si algo no coincide, vuelve a `0` y dime qué pasó.

## 7. Qué no se ha hecho
No se ha tocado la rama estable, `platformio.ini`, `ini/` ni el planificador. No se ha ejecutado nada en Cura real, en la placa ni en la impresora. No se ha invertido ningún eje ni habilitado la cancelación visual con manifiesto.
