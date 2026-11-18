# ARQUIS UI — Etapa 6: optimización (plan y medidas)

Estado: **iniciada**. Esta etapa mide primero y toca después: ninguna optimización se aplica sin un dato real y, cuando cambia el producto (idiomas, apagado), sin tu aprobación. No se modifican protecciones térmicas, motores, drivers ni la lógica de impresión.

## 1. Lo medido hasta ahora (compilaciones reales en tu PC, `mks_robin_nano_v3_1_usb_flash_drive_msc`)
| Compilación | Flash usada | Flash libre | RAM usada |
|---|---|---|---|
| v10 (Etapas 1–2) | 403 152 B (84,8 %) | 71 984 B | 77 996 B (59,5 %) |
| 5A | 428 944 B (90,3 %) | 46 192 B | 79 628 B (60,8 %) |
| 5B-1/2 | 436 588 B (91,9 %) | 38 548 B | 80 160 B (61,2 %) |
| 5B-3 | 435 956 B (91,8 %) | **39 180 B** | 80 472 B (61,4 %) |
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
| 5 | Funciones de dibujo de texto repetidas en 5 vistas (`draw_text_fit`, `draw_text_cx`…) → una sola copia | ≈ 1–2 KB | Seguro; bajo beneficio |
| 6 | Compilar con LTO (`-flto`) en un entorno de prueba aparte | típico 3–8 % (≈ 13–35 KB) | Experimento opcional: mide y decide; no se activa por defecto |
| 7 | Recortar `text16` a los glifos del español | ≈ 3,5 KB | No recomendado (nombres de archivo con otros acentos dejarían de verse) |
Con 1 + 5 el margen pasaría de **39 KB a ≈ 70 KB** (≈ 85 % de uso).

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

## 5. Plan de trabajo (en este orden)
1. **Medir** el reparto real de Flash con `auditar_flash.py` sobre tu ELF (pendiente: tu salida).
2. ~~Aplicar las decisiones de idiomas y Reposo/Lista~~ (**hecho**, falta tu compilación para ver el ahorro real); evaluar los puntos 4–5 de la tabla de §2.
3. Experimento LTO (opcional) en una copia del entorno; solo se adopta si compila, arranca y las pantallas se ven igual.
4. Revisión de robustez: margen de heap y pruebas de estrés de todas las pantallas nuevas en el simulador (reabrir/cerrar cientos de veces) y repaso de los nombres estáticos frente a las cabeceras de Marlin.
5. Informe final de Flash/RAM y cierre de la etapa; preparación del paquete de la Etapa 7 (entrega).

## 6. Criterio de cierre
Compila sin errores ni avisos propios; Flash < 90 % con margen para futuras funciones; todas las pantallas probadas en tu impresora sin reinicios; documentación y paquete de la SD (`sd_assets`) listos para la Etapa 7.
