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

## v7: ARQUIS UI etapa 1 (cierre) + etapa 2 (Dashboard funcional)
- Nuevo Dashboard (`ARQUIS_UI` en Configuration.h) que sustituye la pantalla principal; la original sigue disponible comentando esa línea. Informe completo, medidas y limitaciones: `docs/arquis/ARQUIS_UI_etapas_1_2.md`.
- Nuevo `sd_assets/bmp_logo.bin` (splash con el logo oficial). Para verlo: carpeta `assets` en la SD + arranque (recarga la flash de la placa).

## v8: auditoría previa a la Etapa 3
- Correcciones de seguridad: "Mover" y "Ajustes" deshabilitados con impresión activa; `arq_nav_go()` nunca abre el explorador (card.mount) con un trabajo activo; foco de perilla y grupos corregidos; diálogos cerrados al salir; fuente `num20` eliminada. Informe: `docs/arquis/AUDITORIA_pre_etapa3.md`.
- Nuevo: `herramientas/arquis_ui/auditar_flash.py` (mide Flash/RAM y reparto por módulo tras `pio run`).

## v9 — Primera compilacion real (PC del usuario)
- `arquis_data.cpp`: `print_job_timer.duration()` devuelve `millis_t` (segundos) en Marlin 2.1.2.10, no `duration_t`. Quitado `.value`.
- Causa: mi cabecera stub `herramientas/arquis_ui/stub/module/printcounter.h` tenia la firma equivocada y la comprobacion previa dio un falso OK. Stub corregido.

## v10 — Primera compilacion completa correcta (PC del usuario)
- `pio run -e mks_robin_nano_v3_1_usb_flash_drive_msc`: SUCCESS. RAM 59.5 % (77996 B), Flash 84.8 % (403152 de 475136 B, quedan 71984 B).
- Unica advertencia propia: `event_handler` sin usar en `draw_ready_print.cpp` con ARQUIS_UI activo -> envuelto en `#if DISABLED(ARQUIS_UI)` (el original queda intacto al desactivar ARQUIS_UI).

## v11 — Etapa 3: supervisión de la impresión
- Nueva pantalla de supervisión ARQUIS (`arquis_print*.cpp/.h`); con `ARQUIS_UI` desactivado, `draw_printing.cpp` queda idéntico al original.
- Botón **Inicio** (solo cambia de pantalla), Pausar/Reanudar coherente con el estado real (P7), Cancelar con confirmación, Objetos, Opciones.
- **C3** resuelto: fin de impresión detectado desde cualquier pantalla (aplazado si no es Dashboard/supervisión) con diálogo informativo.
- Porcentaje con el mismo cálculo que la barra original (descuenta la miniatura), división por cero evitada.
- Widgets nuevos: tarjeta de avance (JOB), lista de datos (LIST), diálogo informativo; iconos nuevos `objects` y `sliders`.
- Documento: `docs/arquis/ARQUIS_UI_etapa_3.md`. Estado: simulador OK; compilación completa y pantalla real pendientes de validar.

## v12 — Compilación con las Etapas 3, 4 y 5A + Etapa 5B-1
- Primera compilación completa con las Etapas 3, 4 y 5A (PC del usuario, espacio de trabajo en `C:\pio_arquis`): SUCCESS. RAM 60,8 % (79 628 B), Flash 90,3 % (428 944 de 475 136 B, quedan 46 192 B). Las pantallas de las Etapas 3, 4 y 5A funcionaron en la impresora.
- Corregido: `arquis_files.cpp` usaba `sel_id` sin declararlo (el `draw_ui.h` real no lo declara; el de prueba sí). Buffers de tiempo de `arquis_tools.cpp` ampliados.
- Etapa 5B-1: lista de parámetros ARQUIS para las ~15 pantallas de Ajustes → Parámetros y Memoria, teclado numérico con Cancelar y validación, tabla de traducción al español (`arquis_params.cpp`). Documento: `docs/arquis/ARQUIS_UI_etapa_5.md`. Estado: simulador y firmas OK; compilación completa y pantalla real pendientes de validar.

## v13 — Etapa 5B-2: diálogos y Cambio de filamento
- `draw_dialog.cpp`: los diálogos de Marlin/MKS (Memoria, pausa/M600, reanudar tras un corte, errores) se dibujan con la pantalla de mensaje ARQUIS (`arq_msg_build`); la lógica y los manejadores son los originales. Quedan con el aspecto original los diálogos con temperatura en vivo/barra de progreso.
- `draw_filament_change.cpp`: pantalla de entrada ARQUIS (temperatura + Cargar/Descargar); lógica original.
- Tabla de traducción ampliada a 134 textos. Documento: `docs/arquis/ARQUIS_UI_etapa_5.md`. Estado: simulador y firmas OK; compilación completa y pantalla real pendientes de validar.

## v14 — Compilación con la Etapa 5B (PC del usuario)
- `pio run` (espacio de trabajo en `C:\pio_arquis`): SUCCESS. RAM 61,2 % (80 160 B), Flash 91,9 % (436 588 de 475 136 B, quedan 38 548 B). Respecto a la compilación anterior (5A): +7 644 B de flash, +532 B de RAM.
- Error corregido en esta compilación: `NOZZLE_PROBE_OFFSET_UI` solo existe en el enum de MKS con `HAS_BED_PROBE`.
- Pendiente: probar 5B en la impresora; recuperar flash en la Etapa 6 (el margen ya es de ~38 KB).

## v15 — Etapa 5B-3: Más, G-code, Idioma, teclado, diálogos de progreso
- Rejilla ARQUIS para Más e Idioma (`arquis_tools.cpp`), consola de G-code, colores ARQUIS en el teclado de texto, diálogos de carga/descarga de filamento con el estilo ARQUIS (conservan sus objetos originales) y "IMPRESORA DETENIDA / Reinicia la impresora" en español.
- Sin cambios: calibración táctil, asistente de offset Z, Wi-Fi. Estado: simulador y firmas OK; compilación completa y pantalla real pendientes de validar.

## v16 — Cierre de la Etapa 5 (5B-4: Wi-Fi) y ocultar el Autoapagado sin control de alimentación
- Wi-Fi con aspecto ARQUIS: estado (`arq_wifi_build`), lista de redes (reutiliza el explorador; modelo ampliado con `hl_mask`, `plain` y mensaje de vacío) y avisos (pantalla de mensaje). Se dejan *Parámetros Wi-Fi* y *Nube MKS*.
- `arquis_controls.cpp`: el interruptor *Autoapagado* solo se muestra con `HAS_SUICIDE` (necesita `MKS_PWC`).
- Compilación previa (5B-3, PC del usuario): RAM 61,4 % (80 472 B), Flash 91,8 % (435 956 B, quedan 39 180 B).
- Documento de cierre y prueba segura: `docs/arquis/ARQUIS_UI_etapa_5.md`. Estado: simulador y firmas OK; compilación completa de 5B-4 y pantalla real pendientes de validar.

## v17 — Etapa 6 (inicio): solo español e inglés, estado "Reposo"
- `tft_multi_language.cpp`: con `ARQUIS_UI` solo se compilan inglés y español (ahorro esperado ≈ 30 KB de Flash, por confirmar con la compilación). Idioma por defecto: español. La pantalla Idioma ofrece *Español* e *Inglés*; `arq_es()` no traduce con idioma inglés.
- Nuevo estado "Reposo" (sin tarjeta SD y todo apagado; punto gris): `arq_with_standby()`; "Lista" = con tarjeta. 52 pruebas de lógica.
- Descartados por decisión del usuario: apagado automático/remoto y perilla en listas y teclados nuevos.
- Documento: `docs/arquis/ARQUIS_UI_etapa_6.md`. Estado: simulador y firmas OK; compilación completa pendiente.

## v18 — Compilación de la Etapa 6 (PC del usuario)
- `pio run`: SUCCESS (incluye las pantallas de Wi-Fi de 5B-4). **Flash 85,5 % (406 276 B, quedan 68 860 B)**; antes 91,8 % (435 956 B): **−29 680 B** al dejar solo español e inglés. RAM 61,6 % (80 708 B).
- `auditar_flash.py` debe apuntar a `Robin_nano_v3.elf` (el script de compilación renombra el `.elf`).

## v19 — Etapa 6: auditoría de Flash y robustez
- Auditoría real: ARQUIS UI = 45 107 B (11 % de la Flash); margen 68 336 B (14,4 %). LVGL 68 KB, HAL/USB 36 KB, G-code 20 KB, planificador 20 KB, Wi-Fi 10 KB, idiomas 5 KB.
- Robustez en el simulador: 213 comprobaciones sin fallos; peor caso (pantalla + diálogo + perilla) con ≥ 9,2 KB de heap libres.
- LTO descartado (riesgo para las rutinas de movimiento sin beneficio necesario).

## v20 — Etapa 7: entrega ARQUIS UI 1.0.0
- Versión de la interfaz (`ARQUIS_UI_VERSION` "1.0.0" en `arquis_theme.h`), visible en Ajustes > Sistema como "Interfaz ARQUIS 1.0.0".
- Nuevo `herramientas/arquis_ui/empaquetar_entrega.py`: arma `Arquis-XQ1_UI_v1.0.0` (firmware, `assets`, `LEEME.txt`, `SHA256SUMS.txt`) y el `.zip`; comprueba tamaño del `.bin`, logo, fuente y recursos que pide `pic_manager.cpp`.
- Documentación de entrega: `docs/arquis/ENTREGA_v1.0.0.md` (notas de versión, evidencia de lo no modificado, limitaciones), `GUIA_INSTALACION.md`, `GUIA_USUARIO.md`, `LISTA_VERIFICACION.md`; README actualizado.
- Evidencia: fuera de la carpeta de la interfaz, el único archivo de Marlin modificado respecto a `main` es `Configuration.h` (`ARQUIS_UI` activado, `ARQUIS_TFT_BYTE_SWAP` desactivado).


## Nivelación manual de cama 5×5 (sin sonda)
Malla bilineal 5×5 medida con papel desde la TS35 (Herramientas → Malla 5x5), guardada con M500 y activada tras cada G28 (`ENABLE_LEVELING_AFTER_G28`). Ver `docs/arquis/MALLA_5x5.md`. **Aviso:** cambiar la malla de 3×3 a 5×5 cambia el tamaño de la EEPROM: la primera vez tras flashear Marlin la descarta y vuelve a los valores de fábrica (guarda `M503` antes).


## Perfiles de máquina (varias impresoras, un solo firmware)
Los valores de cada impresora (medidas, pasos/mm, sentidos, finales de carrera, PID…) viven en `Marlin/machine/<nombre>.h` y se eligen con el entorno de PlatformIO (`pio run -e aquila_d1`). El XQ1 no cambia. Ver `docs/arquis/MAQUINAS.md`. **EEPROM:** las versiones de la nivelación 5×5 y del PID de cama cambian su tamaño: guarda `M503` antes de flashear.


## Rendimiento: pantalla de ajustes de movimiento y torres de prueba
**Ajustes → Rendimiento** (TS35): aceleraciones (M204/M201), Linear Advance K (M900), Junction Deviation (M205 J) e Input Shaping X/Y (M593) con límites, paso fino/grueso, Guardar (M500) y Restaurar. Solo con la impresora quieta. En `herramientas/torres/` hay un generador de torres de prueba (ringing por frecuencia y por aceleración, K, caudal máximo) que cambian un valor por tramo, verifican su G-code antes de escribirlo y terminan con `M501`; archivos listos para el XQ1 (PLA) en `herramientas/torres/listos/`. Ver `docs/arquis/RENDIMIENTO.md`. Probado por el propietario en la impresora (2026-11-22). Flash estimada +4,7 KB: comprueba el uso real con `pio run`.

## Velocidad real en la pantalla de impresión
Nueva fila **Veloc.** en la supervisión: velocidad real del cabezal en XY (mm/s) y la máxima alcanzada en esa impresión. Se calcula derivando la posición real de X e Y (`planner.get_axis_position_mm`, pasos ya ejecutados) en ventanas de 0,15 s, con suavizado; los saltos de posición (G92, home) se ignoran y el máximo se reinicia con cada impresión. La lista de la supervisión pasa a 5 filas compactas (18 px). Lógica pura y probada en `herramientas/arquis_ui/sim/logic_test.cpp`.
**Estado (2026-11-24):**
- **Confirmado por el propietario en la impresora:** la velocidad real se muestra correctamente durante la impresión.
- **Compilación real** (PC del propietario, `pio run -e mks_robin_nano_v3_1_usb_flash_drive_msc`, commit `a98dd6a`): SUCCESS, Flash 93,0 % (441 708 B), RAM 68,0 % (89 120 B). Aviso propio pendiente: `arquis_print_view.cpp:146`, el búfer `m[10]` es corto para todo el rango de `uint16_t`; con velocidades reales no tiene efecto y se corrige en ARQUIS UI 2.0.
- **Ejecutado por Claude** (PC, compilador nativo): `logic_test.cpp`, 83 de 83 pruebas correctas, incluidas las de `arq_speed_*`; pantalla de supervisión dibujada con LVGL 6.1.1-MKS real en el simulador.

## Flujo de extrusión: botón y pantalla propia en Herramientas
Nuevo botón **Flujo** en Herramientas con su pantalla (`FLOW_UI`, `arq_flow_*` en `arquis_tools.cpp` / `arquis_tools_view.cpp`): % de extrusión (M221) en grande, − / +, pasos 1/5/10 % y preajustes 90/100/110 %. Hace lo mismo que la tarjeta *Extrusión* de la pantalla de Velocidad (`planner.flow_percentage` + `refresh_e_factor`, con los límites `MIN/MAX_EXT_SPEED_PERCENT`), sin tocar el núcleo de Marlin. La rejilla de accesos (`arq_opts_build`) admite ahora hasta 12 tarjetas: con 8 o menos queda 4 × 2 como siempre; con 9 a 12 pasa a 4 × 3. Simulador: pantalla, rejilla de 9 y pruebas de toque (los avisos de heap que ya fallaban al compilar con el compilador del PC siguen igual).
**Estado (2026-11-24):**
- **Confirmado por el propietario en la impresora:** el control de flujo (M221) funciona.
- **Compilación real** (PC del propietario, commit `a98dd6a`): SUCCESS (mismas cifras que la velocidad real).
- **Ejecutado por Claude** (simulador con LVGL 6.1.1-MKS real, compilador nativo de 64 bits): los 8 toques de la pantalla Flujo (Atrás, 90/100/110 %, pasos 1/5/10 %, −) devuelven el identificador correcto. La comprobación de heap marca 5 352 B frente al límite de 5 200 B. **No es concluyente:** en 64 bits los objetos de LVGL ocupan más que en la placa (ARM de 32 bits); **pendiente de medir en ARM de 32 bits**.
- **Pendiente:** la guía de usuario indica el acceso a Flujo durante la impresión por *Inicio → Herramientas*, pero Herramientas está bloqueada con un trabajo activo. Hoy el acceso durante la impresión es *Opciones → Velocidad → Extrusión*. Se corrige en ARQUIS UI 2.0 (botón Flujo en la supervisión).

## XQ1: Z a 5 mm/s y aceleración 100 (prueba)
`Marlin/machine/xq1.h` fija `DEFAULT_MAX_FEEDRATE` Z = 5 y `DEFAULT_MAX_ACCELERATION` Z = 100 (antes 45 y 150 desde `c5b5cfd`) por una Z que no subía al imprimir. `tests/machines/test_profiles.py` admite ahora los valores que declara `xq1.h`.
**Estado (2026-11-24): PENDIENTE DE VALIDACIÓN FÍSICA.** El propietario todavía no ha confirmado que, con estos valores, Z suba correctamente al imprimir. No se considera configuración estable. Cualquier etiqueta de recuperación que contenga este commit (por ejemplo, una sobre `a98dd6a`) lo incluye necesariamente, así que deberá indicar en su descripción que este ajuste de Z está pendiente de validación.

## Fase 3: optimización de memoria (F3-01 a F3-08)
Commits independientes en la rama `feature/arquis-firmware-unico-perfiles`:
- buffer de la velocidad máxima;
- texto del PID;
- fuentes `num36` (2 bpp) y `text16` (español);
- iconos a 2 bpp;
- sin Nube MKS ni QRCode;
- solo español;
- **USB solo CDC** (sin MSC ni host, en las dos máquinas).

Medido: de 441 764 a 400 472 B de Flash y de 89 120 a 76 668 B de RAM. Se conservan USB CDC/COM, Wi-Fi y microSD.

## Fase 4: firmware único para Arquis XQ1 y Aquila D1 (versión de pruebas)
**Un solo `Robin_nano_v3.bin` para las dos impresoras.** El modelo se elige en un **asistente del primer arranque** y después en **Ajustes → Máquina**. Se guarda en un registro ARQUIS de la EEPROM: 2 copias con CRC, aparte de los datos de Marlin, cuya estructura no cambia.

**Qué cubre el perfil:** medidas, lógica de finales, sentido de los motores (bit `GCONF.shaft` del TMC2209, verificado por UART; `stepper.cpp` no se toca), pasos, velocidades, aceleraciones, corrientes e Input Shaping.

**Seguridad:**
- Sin un perfil válido y comprobado (finales y motores probados) no se acepta ningún movimiento.
- `M502` restaura los valores del modelo elegido.
- `M9050 R` vuelve al asistente.

Se elimina el entorno `aquila_d1`.

**Medido:** Flash 421 204 B (88,6 %), RAM 77 752 B (59,3 %).

**Pruebas:**
- `tests/machines`: 51 + 56;
- simulador del asistente: 23;
- malla, PID, rendimiento, torres y lógica: sin fallos.

**Estado: NO probado en la impresora.**
- Lista de pruebas: `docs/arquis/PRUEBAS_FISICAS_ARQUIS.md`.
- Instalación: `docs/arquis/INSTALACION_FIRMWARE_UNICO.md`.
- Detalle técnico: `docs/arquis/FASE4_IMPLEMENTACION_FINAL.md`.

Los valores de Z del XQ1 (5 mm/s y 100 mm/s²) siguen **pendientes de validación física**.

## Fase 5: correcciones P1–P5 de la auditoría (versión de pruebas)
Rama `claude/pensive-cray-ib96kb`, sobre `fdff0cd`. Código en `0bae5fc`; solo cambian los 5 archivos `arquis_machine*`.
- **P1:** al cambiar de modelo se carga siempre la mecánica de fábrica del nuevo. «Conservar» solo se permite si se confirma que los datos son de ese modelo; si no, se rechaza explicando por qué. El M206 solo se conserva en ese caso.
- **P2:** «Ajuste Z» pasa a «Babystep» / «offsets M206». Las pruebas ya no borran la malla. Antes de borrar una malla de otras medidas, la pantalla avisa.
- **P3:** «Otros parámetros (MKS)» pide Guardar o Descartar. Descartar deshace las pruebas sin `M501`. Al volver se relee. No se mueve con cambios de Máquina sin guardar.
- **P4:** simulador con el pegamento real y sin archivos externos. `check_marlin_api.sh` falla si no se ejecuta y cubre 32 archivos. Nueva prueba del pegamento (`tests/machines/glue_test.cpp`).
- **P5:** antes de un movimiento manual se lee `GCONF` de los drivers invertidos que mueve la orden. Si no se confirma, no se mueve, se recupera la configuración, se verifica y se pide repetir. La primera orden de cada trabajo se verifica entera. Durante la impresión se vigilan los drivers invertidos.

**Medido:** Flash 423 828 B (89,2 %), RAM 78 108 B (59,6 %), +2 624 B y +356 B respecto a la Fase 4.

**Pruebas:**
- `tests/machines`: 51 + 90 + 69;
- simulador de máquina: 82;
- `check_marlin_api`: 32/32;
- el resto, sin cambios y sin fallos.

**Verificación:** la recompilación de `fdff0cd` coincide byte a byte con el BIN de la Fase 4, salvo las rutas y la fecha.

**BIN de pruebas:** `firmware_pruebas/fase5`. **Estado: NO probado en la impresora.** Detalle: `docs/arquis/FASE5_CORRECCIONES_AUDITORIA.md`.

## Manual de uso completo
`docs/arquis/MANUAL_DE_USO.md`: árbol de todas las pantallas, qué hace cada una, valores de fábrica, dónde se ajusta cada cosa (y si se guarda con M500), puesta a punto en orden y problemas frecuentes. Incluye Malla 5×5, PID, Rendimiento y perfiles. Se genera también en PDF con capturas de la interfaz: `docs/arquis/Manual_de_uso_ARQUIS_XQ1.pdf` (`herramientas/manual/generar_pdf.sh`; requiere pandoc, Node con Playwright/Chromium y las fuentes Inter y DejaVu).
