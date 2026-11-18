# ARQUIS UI — Etapa 6: optimización (plan y medidas)

Estado: **iniciada**. Esta etapa mide primero y toca después: ninguna optimización se aplica sin un dato real y, cuando cambia el producto (idiomas, apagado), sin tu aprobación. No se modifican protecciones térmicas, motores, drivers ni la lógica de impresión.

## 1. Lo medido hasta ahora (compilaciones reales en tu PC, `mks_robin_nano_v3_1_usb_flash_drive_msc`)
| Compilación | Flash usada | Flash libre | RAM usada |
|---|---|---|---|
| v10 (Etapas 1–2) | 403 152 B (84,8 %) | 71 984 B | 77 996 B (59,5 %) |
| 5A | 428 944 B (90,3 %) | 46 192 B | 79 628 B (60,8 %) |
| 5B-1/2 | 436 588 B (91,9 %) | 38 548 B | 80 160 B (61,2 %) |
| 5B-3 | 435 956 B (91,8 %) | 39 180 B | 80 472 B (61,4 %) |
| **6 (solo español e inglés + Reposo)** | **406 276 B (85,5 %)** | **68 860 B** | 80 708 B (61,6 %) |
(Límite de Flash 475 136 B: la placa reserva 48 KB para el cargador.) La RAM no preocupa (61 %); el heap de LVGL (16 KB) tiene margen: peor caso medido ≥ 9,4 KB libres.

**Coste del código ARQUIS** (compilado aquí con GCC para Cortex-M4, `-Os`, **cota superior** sin eliminar lo que nadie referencia):
| Parte | Flash |
|---|---|
| Código de vistas y pegamento (18 archivos) | ≈ 44 KB |
| Iconos (los 41 definidos) | ≈ 26 KB |
| Fuentes Inter (`text16` + `num36`) | ≈ 15 KB |
| Logo | ≈ 1,5 KB |
| **Total (cota superior)** | **≈ 85,6 KB** |
Seis recursos no los referencia nada (`arq_icon_check/folder/stop/warn`, `arq_logo20_blue/green`): el enlazador (`--gc-sections`) debería descartarlos; se confirma con el mapa de tu compilación.

## 2. Candidatos de Flash (revisados con lo aprendido en la Etapa 5)
| # | Candidato | Ahorro | Estado |
|---|---|---|---|
| 1 | Idiomas de MKS: hoy se compilan 7 (en, es, fr, it, ru, s_cn, t_cn); la interfaz ARQUIS está en español. Dejar solo en + es | **≈ 29–30 KB** (medido en la auditoría previa: 44,2 KB las tablas) | **Pendiente de tu decisión** |
| 2 | Fuente por defecto `lv_font_roboto_22` | ≈ 23 KB | **Descartado**: el teclado de texto usa los símbolos de esa fuente (borrar, flechas, aceptar) y la fuente ARQUIS no los trae |
| 3 | Módulo Wi-Fi MKS | decenas de KB | **Descartado**: tienes el módulo instalado |
| 4 | Recursos sin uso (6) | ≈ 0 si `--gc-sections` ya los quita; ≈ 5 KB si no | Verificar con el mapa |
| 5 | Funciones de dibujo de texto repetidas (`draw_text_fit`, `draw_text_cx`…): solo hay 3 copias pequeñas | < 1 KB | **Descartado**: el beneficio no compensa el riesgo de tocar vistas ya probadas |
| 6 | Compilar con LTO (`-flto`) en un entorno de prueba aparte | típico 3–8 % (≈ 13–35 KB) | Experimento opcional: mide y decide; no se activa por defecto |
| 7 | Recortar `text16` a los glifos del español | ≈ 3,5 KB | No recomendado (nombres de archivo con otros acentos dejarían de verse) |
**Medido tras aplicar el punto 1:** la Flash bajó **29 680 B** (435 956 → 406 276 B), justo lo estimado; el margen pasó de 39 KB a **≈ 69 KB (85,5 % de uso)**. La RAM subió 236 B (nuevo estado y textos).

## 3. Decisiones de producto (tomadas por ti)
| Tema | Decisión | Estado |
|---|---|---|
| Idiomas | Solo **español e inglés** | **Hecho** (ver §3.1) |
| Reposo / Lista | **Aprobado:** *Reposo* = sin tarjeta SD montada y todo apagado; *Lista* = con tarjeta y todo apagado | **Hecho** (ver §3.2) |
| Apagado automático / control remoto | **Descartado** (no se hará relé, ESP32-S3 ni OctoPrint) | Cerrado. El interruptor *Autoapagado* sigue oculto sin `MKS_PWC` |
| Perilla en listas y teclados nuevos | **Descartada** por ahora | Cerrado |
| Umbral de "Enfriando" (50 °C) | Sin respuesta | **Se mantiene en 50 °C** |

### 3.1 Idiomas
- Con `ARQUIS_UI` activo se compilan solo `LANG_ENGLISH` y `LANG_SPANISH` (`tft_multi_language.cpp`: los bloques de chino simplificado y tradicional, ruso, francés e italiano quedan bajo `#if DISABLED(ARQUIS_UI)`; con `ARQUIS_UI` desactivado compila todo como antes). **Ahorro esperado ≈ 30 KB de Flash** (medido en la auditoría previa: 44,2 KB las tablas de 7 idiomas); la cifra real sale de tu compilación.
- **Idioma por defecto: español** (antes el arranque forzaba inglés). La tabla de traducción propia (134 textos) sigue completando lo que MKS no trae en español.
- **Pantalla Idioma:** ahora tiene 2 opciones, *Español* e *Inglés*. Las pantallas ARQUIS están escritas en español; el idioma afecta a los textos que vienen de las tablas de MKS (con *Inglés* se muestran sin traducir).

### 3.2 Reposo y Lista
- Nuevo estado `ARQ_PS_STANDBY` ("Reposo", punto gris) en `arquis_logic.cpp`: solo cambia el estado "todo apagado" y **sin** tarjeta SD (`!card.isMounted()`). Calentando, imprimiendo, pausada, enfriando y error se muestran igual con o sin tarjeta.
- Pruebas: 52 de 52 de lógica (9 nuevas) y captura `dash_8_reposo_x2.png`.

## 5. Resultado de la auditoría de Flash (`auditar_flash.py` sobre tu `Robin_nano_v3.elf`)
Flash usada **406 800 B (85,6 %)**, margen **68 336 B**; RAM 82 372 B (62,8 %, con el heap y la pila reservados). Secciones: `.text` 298 312 B, `.rodata` 104 480 B, `.data` 3 484 B.
| Módulo (por nombre de símbolo) | Flash |
|---|---|
| LVGL (núcleo y widgets) | 68 394 B |
| **ARQUIS UI** (todo lo que queda tras `--gc-sections`) | **45 107 B** (11 % del total) |
| HAL / USB / FatFs / STM32 | 35 774 B |
| G-code | 19 802 B |
| Planificador / movimiento | 19 727 B |
| Wi-Fi MKS | 10 324 B |
| Temperatura | 5 894 B |
| Idiomas MKS (ya solo español e inglés) | 5 301 B |
| Resto (Marlin, librerías, fuentes) | ≈ 250 KB |
**Lectura:** los iconos y recursos que no usa nada ya los descarta el enlazador (el código ARQUIS real, 45 KB, es la mitad de mi cota superior de 85 KB). Lo más grande que queda son bloques de MKS/LVGL que se necesitan: tipografías por defecto de LVGL (≈ 19 + 9 KB, las usan el teclado y las pantallas que quedan de MKS), el heap de LVGL (16 KB de RAM), el búfer de imágenes (14 KB de RAM) y la fuente del aviso de error. No hay nada grande evidentemente sobrante.

## 6. Robustez (simulador con LVGL 6.1.1 real, ARM de 32 bits, heap de 16 KB)
**213 comprobaciones, 0 fallos**; 52 pruebas de lógica. Heap usado por pantalla (el heap total es de 16 384 B y LVGL no tolera quedarse sin él):
| Pantalla | Heap |
|---|---|
| Dashboard (7 estados + Reposo) | 3,5–3,9 KB |
| Supervisión de impresión (pico) | 3,5 KB |
| Temperatura / Mover / Babystep | 5,1 KB |
| Ventiladores, Velocidad, Extrusión, Opciones, Nivelar cama | 3,5–3,9 KB |
| Explorador de archivos | 1,9 KB |
| Cancelar objeto (0 a 16 objetos) | 1,9 KB |
| Parámetros (4 variantes) | 1,1–1,5 KB |
| Teclado numérico, diálogos, consola G-code | 1,5 KB |
| Cambio de filamento, Wi-Fi (estado) | 1,9–2,0 KB |
| Sistema y diagnóstico | 1,6 KB |
**Peor caso medido** (pantalla + diálogo + perilla): ≈ 7,0 KB usados, **≥ 9,2 KB libres**. Todas las pantallas se abren y cierran cientos de veces sin fugas (heap final = heap inicial). La pantalla anterior de Cancelar objeto necesitaba unos 26 KB: por eso se reinició; ya no.

## 7. Experimento LTO: no se hace
El margen ya es del 14 % (68 KB) y el objetivo de la etapa (Flash < 90 % con margen) está cumplido. LTO recompila **todo Marlin** (incluidas las rutinas de interrupción del planificador y los motores) con otra generación de código: es un riesgo sin beneficio necesario, y esta etapa no puede tocar movimiento ni protecciones. Se podría reconsiderar si en el futuro faltara espacio.

## 8. Estado de la etapa
| Objetivo | Estado |
|---|---|
| Flash < 90 % con margen | **Cumplido** (85,6 %, 68 KB libres) |
| Compila sin errores ni avisos propios | **Cumplido** (solo queda el aviso de Marlin sobre los jumpers DIAG) |
| Todas las pantallas sin fugas ni reinicios en el simulador | **Cumplido** |
| Solo español e inglés | **Hecho** |
| Reposo / Lista | **Hecho** en código y simulador; **falta comprobarlo en la impresora** (con y sin tarjeta SD) |
| Pantallas 5B-3 y 5B-4 en la impresora (G-code, Idioma, diálogos de carga, Wi-Fi) | **Pendiente** de tu prueba |
| Cama marcando 45 ° con todo apagado | **Pospuesto** (hardware: tocar la cama, multímetro, tipo de termistor) |
| Apagado automático / remoto, perilla | **Descartados** |
