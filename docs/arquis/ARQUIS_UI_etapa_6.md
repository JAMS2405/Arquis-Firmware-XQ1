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

## 3. Decisiones de producto pendientes (tuyas)
1. **Idiomas:** ¿dejo solo español e inglés (ahorra ≈ 30 KB)? La pantalla *Idioma* mostraría 2 opciones en lugar de 7. Es reversible con una línea.
2. **Reposo / Lista:** hoy es un solo estado ("Lista"). Propuesta con dato real: *Reposo* = sin tarjeta SD montada; *Lista* = con tarjeta y todo apagado.
3. **"Enfriando":** hoy aparece con un calentador apagado por encima de 50 °C. ¿Lo bajo a 40 °C (más cercano a "seguro de tocar")?
4. **Apagado automático / remoto** (relé con MKS PWC, ESP32-S3 o OctoPrint): ver la conversación; depende de tu elección y del hardware.
5. **Perilla:** dijiste que no te afecta. ¿La damos por descartada (las listas y teclados nuevos no la usan) para no gastar flash en ello?

## 4. Comprobaciones que necesitan tu impresora
- **Cama marcando ≈ 45 ° estando apagada:** con la impresora fría y recién encendida, ¿qué marca el Dashboard? (si es la lectura real del termistor, es un tema de hardware/configuración, no de la interfaz; no se toca la configuración térmica).
- Prueba segura de la Etapa 5 (5B-3: G-code, Idioma, diálogos de carga; 5B-4: Wi-Fi) — ver `ARQUIS_UI_etapa_5.md` §7.

## 5. Plan de trabajo (en este orden)
1. **Medir** el reparto real de Flash con `auditar_flash.py` sobre tu ELF (pendiente: tu salida).
2. Aplicar las decisiones 1–3 que apruebes (idiomas, Reposo/Lista, umbral) y los puntos 4–5 de la tabla; comprobar con el simulador, las firmas y tu compilación.
3. Experimento LTO (opcional) en una copia del entorno; solo se adopta si compila, arranca y las pantallas se ven igual.
4. Revisión de robustez: margen de heap y pruebas de estrés de todas las pantallas nuevas en el simulador (reabrir/cerrar cientos de veces) y repaso de los nombres estáticos frente a las cabeceras de Marlin.
5. Informe final de Flash/RAM y cierre de la etapa; preparación del paquete de la Etapa 7 (entrega).

## 6. Criterio de cierre
Compila sin errores ni avisos propios; Flash < 90 % con margen para futuras funciones; todas las pantallas probadas en tu impresora sin reinicios; documentación y paquete de la SD (`sd_assets`) listos para la Etapa 7.
