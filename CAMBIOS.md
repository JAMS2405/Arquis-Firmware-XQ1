# Arquis XQ1 — Marlin 2.1.2.10 + TFT_LVGL_UI

Base: Marlin 2.1.2.10 (estable). Portado desde tu 2.1.2.1 con merge a 3 vias:
solo se reaplicaron TUS cambios sobre la config de fabrica nueva.

## Tus ajustes conservados
Placa Robin Nano V3.1, TMC2209 (32 microsteps), steps {160,160,800,770}, cama 300x300x450,
PID hotend/cama, endstops invertidos, INVERT_X_DIR, PROBE_MANUALLY + bilinear, Input Shaping
(X 30Hz/0.40, Y 22.5Hz/0.70), BABYSTEPPING, CANCEL_OBJECTS, preheat PLA/ABS/NYL, idioma es, SD, TS35 + tactil.
Nota: Marlin renombro DEFAULT_Kp/Ki/Kd -> DEFAULT_KP/KI/KD (ya adaptado).

## Nuevo
- TFT_LVGL_UI activo (TFT_CLASSIC_UI desactivado), MKS_WIFI_MODULE conservado
- CUSTOM_MACHINE_NAME "Arquis XQ1" / autor Arquis
- LIN_ADVANCE (usa tu ADVANCE_K 0.013 - recalibrar)
- S_CURVE_ACCELERATION
- ADVANCED_PAUSE_FEATURE (M600 / cambio de filamento)
- POWER_LOSS_RECOVERY (recuperar impresion tras corte)
- PRINTCOUNTER (estadisticas)
- Logo de arranque: assets/bmp_logo.bin (placeholder "ARQUIS XQ1"; usa herramientas/logo_a_bin.py con tu logo real)

## Pasos
1. Respalda EEPROM actual: M503 (copia la salida).
2. Descomprime Arquis-XQ1_Marlin-2.1.2.10.zip, abre la carpeta en VS Code + PlatformIO, Build
   (env mks_robin_nano_v3_1_usb_flash_drive_msc). Descargara la libreria LVGL y los assets solos.
3. Copia firmware .bin (carpeta .pio/build/.../Robin_nano35.bin segun pantalla) a la SD y reinicia.
4. Copia la carpeta assets (SD_assets_LVGL.zip) a la RAIZ de la SD. Debe estar la primera vez que arranca.
5. Tras flashear: M502, M500 (valores de fabrica nuevos) y recalibrar tactil; reajustar PID (M303) y K de LIN_ADVANCE.


## Pantallas nuevas (codigo en Marlin/src/lcd/extui/mks_ui)
- Miniatura que se llena: preview_fill_update() en draw_printing.cpp. Lo impreso a todo color, lo pendiente atenuado, linea azul = altura. Se basa en el % de avance del archivo.
- Cancelar objeto: draw_cancel_object.cpp (mapa de cama + fichas + boton con confirmacion). Boton "Quitar objeto" en Opciones (durante la impresion).
- Posicion de cada objeto: se aprende en G0_G1.cpp + feature/cancel_object.* mientras imprime. Requiere G-code con M486 (Cura: plugin/ PrusaSlicer/Orca: "Label objects" en formato Marlin M486).
- Tope: 16 objetos en pantalla.

## Tema Arquis (colores e iconos)
- Paleta: fondo azul noche #0B1220, acento cian #00B4FF (como el logo), ambar #FFB020 para seleccion/detalles.
- Iconos: SD_assets_LVGL.zip trae los 137 iconos recoloreados (violeta->cian/azul, rosa->ambar, fondo igual al de la UI, sin cuadros).
- Codigo: draw_ui.h / draw_ui.cpp (fondo, botones, barra de progreso, lineas) y draw_cancel_object.cpp.
- Para cambiar la paleta: editar herramientas/tema_arquis.py (+ LV_COLOR_BACKGROUND en draw_ui.h) y regenerar:
    python tema_arquis.py <assets_originales> <assets_salida>
- IMPORTANTE: firmware y assets deben ser del mismo tema (si no, se ven cuadros de otro color alrededor de los iconos).

## Pruebas realizadas (17-nov-2026)
- COMPILA: env mks_robin_nano_v3_1_usb_flash_drive_msc, Marlin 2.1.2.10, PlatformIO + ststm32 12.1.1, arm-none-eabi-gcc 13.2.
  Resultado: Robin_nano_v3.bin = 382 KB. Flash 80.4 % (381864 de 475136 bytes), RAM 57.4 % (75232 de 131072 bytes).
  Nota: se compilo con GCC 13 del sistema; PlatformIO usa GCC 9.2 oficial en tu PC. Compila alli de nuevo antes de flashear.
- La compilacion detecto y se corrigieron 4 incompatibilidades de la config con LVGL:
  * LCD_BED_LEVELING -> DESACTIVADO (LVGL no lo soporta; LVGL trae sus propias pantallas de nivelacion).
  * ADVANCED_PAUSE_FEATURE exige EMERGENCY_PARSER, HOST_ACTION_COMMANDS y HOST_PROMPT_SUPPORT -> ACTIVADOS.
  * PARK_HEAD_ON_PAUSE -> ACTIVADO (estacionar el cabezal al pausar / cambiar filamento).
- Las funciones nuevas (pantalla cancelar objeto, relleno de miniatura, boton Quitar objeto, aprendizaje de posiciones) estan dentro del binario.
- Pruebas de logica en PC con el codigo real: herramientas/pruebas/run_tests.sh
  * Relleno de miniatura: 11 actualizaciones (incluye retroceso y repintado total) -> 0 pixeles incorrectos.
  * Cancelar objeto: cajas XY, entrelazado por capas, cancelar/restaurar, limites (0..31), reset -> OK.
- La SD trae todos los iconos que el firmware exige al arrancar (120 de 120; el unico "faltante" es una diferencia de mayusculas del propio MKS).

## NO probado (necesita la impresora)
Arranque, aspecto real de la pantalla, tactil, y comportamiento durante una impresion real. Primera prueba sugerida: archivo corto con 2-3 objetos (M486) y la boquilla sin filamento / cama fria.

## Iconos y logo propios (v2)
- `bmp_logo.bin`: tu logo (A azul/verde con circuitos) sobre fondo navy, 480x320.
- 21 iconos `bmp_*.bin` (117x140) redibujados en estilo línea: trazo azul #25ABE2, acentos verdes #4CB050, nodos de circuito. Fuentes SVG en `herramientas/iconos_svg`, generador `herramientas/iconos_a_bin.py`.
- Solo cambian archivos de la SD (`assets`); el firmware compilado no cambia.

## Valores calibrados de la impresora (v3)
- Pasos/mm por defecto: X160 Y160 Z791.74 E748.72 (tomados de la pantalla de la impresora en funcionamiento).
- Corriente TMC2209: X/Y/Z 650 mA, E0 700 mA.
- Motivo: al cambiar de versión de Marlin la EEPROM se descarta y carga estos valores por defecto.

## v4: correccion de color + resto de iconos
- **Colores invertidos en pantalla** (fotos del usuario: fondo verde oliva, logo azul/verde al reves). Causa: el orden de los 2 bytes de cada pixel llegaba invertido al panel con el .bin compilado aqui. Correccion en un solo punto (`tft_spi.h`, opcion `ARQUIS_TFT_BYTE_SWAP` en Configuration.h) que cubre LVGL, logo, miniatura y colores del firmware. Si compilas tu con PlatformIO oficial y los colores salen mal, comenta esa linea.
- **109 iconos nuevos** (`herramientas/iconos_extra.py`): mas/menos, dentro/fuera, extrusor 1/2, ventilador 0/50/100 %, velocidades, pasos de movimiento/temperatura/porcentaje, flechas X/Y/Z, origen X/Y/Z/todos, apagar, archivos/SD/USB, nivelacion 1-5, idiomas, pausa/parar/reanudar, atras/paginas y los 10 iconos pequenos de estado.
- Sin rediseñar (no los usa casi nadie): interruptores enable/disable, marcos blank, back70x40, arrow, X_mode.

## v5: byte swap desactivado
- Con ARQUIS_TFT_BYTE_SWAP activo, un build propio (PlatformIO oficial) mostraba TODO con colores rotos, incluido el logo Makerbase interno. LVGL de Makerbase usa LV_COLOR_16_SWAP=0, asi que no hace falta intercambiar bytes: la linea queda comentada en Configuration.h.
- El logo y los iconos se copian a la memoria flash de la placa desde la SD: pon la carpeta `assets` (contenido de sd_assets) en la raiz de la SD junto al firmware y arranca una vez.

## v6: ARQUIS UI etapa 1 (sistema visual)
- Nuevos archivos `arquis_theme.*`, `arquis_widgets.*`, `arquis_icons.*`, `arq_font_*.c` en `mks_ui/`. No modifican pantallas ni lógica existentes. Detalle y medidas: `docs/arquis/ARQUIS_UI_etapa1.md`.
- IMPORTANTE (assets): el build de PlatformIO descarga los iconos ORIGINALES de Makerbase en `.pio/build/<entorno>/assets`. No copies esa carpeta a la SD: usa `sd_assets` de este repo renombrada a `assets`.
