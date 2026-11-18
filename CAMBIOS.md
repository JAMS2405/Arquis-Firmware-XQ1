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

## No verificado
No se pudo compilar en el entorno de trabajo (PlatformIO sin acceso a su registro). Si falla el build, pega el error.

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
