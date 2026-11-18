# ARQUIS UI — Etapa 4: archivos y controles

Rama `claude/nice-gauss-y90u12`. `main` intacta. **Estado: simulador y comprobación de firmas OK. NO compilada con Marlin completo y NO probada en la impresora.**

## 1. Qué incluye
| Pantalla | Archivo(s) | Objetos LVGL | Heap (simulador) |
|---|---|---|---|
| Temperatura | `arquis_ctrl_view.cpp` + `arquis_controls.cpp` | 11 | 5,1 KB |
| Ventiladores | idem | 9–10 | 3,9 KB |
| Mover | idem | 12 | 5,1 KB |
| Extrusión | idem | 9 | 3,5 KB |
| Opciones | idem | 6–9 | 3,5 KB |
| Explorador de archivos | `arquis_files_view.cpp` + bloque ARQUIS de `draw_print_file.cpp` | 5 | 1,9 KB |
| Confirmar impresión | `arquis_files.cpp` (+ 4 líneas en `draw_dialog.cpp`) | 5 | ≈ 1,5 KB |
| Cancelar objeto | Etapa 3 (corregida, v12) | 6 | 1,9 KB |

Peor caso medido (Opciones + diálogo de confirmación): 7 KB usados, **9,4 KB libres de 16 KB**. 500 + 500 ciclos de crear/destruir las cinco pantallas de control y 300 + 300 del explorador: sin fugas ni fragmentación.

Componentes nuevos (`arquis_widgets`): tarjeta seleccionable compacta (PICK), valor grande (BIG), subtítulo en los accesos (TILE), anillo azul de selección, cambio de tipo de botón. Iconos nuevos: más, menos, 4 flechas, apagar, filamento (40 px) y carpeta, archivo, flecha (20 px).

## 2. Qué cambia respecto a las pantallas originales
- **Temperatura:** dos tarjetas (Hotend, Cama) con temperatura actual, objetivo y estado; la seleccionada se edita a la derecha: − / + (paso 1, 5 o 10 °C elegido con un toque, ya no hay que ir ciclando), preajustes PLA / ABS (valores de `Configuration.h`) y Apagar.
- **Ventiladores:** ahora se pueden controlar **todos** los ventiladores (Capa y Aux 1); antes solo el 0. − / + cambian de **5 en 5 %** (antes de 1 en 1) y hay atajos Apagar / 50 % / 100 %.
- **Mover:** cruz XY, columna Z, pasos 0,1 / 1 / 10 mm elegidos con un toque, posición de los tres ejes en vivo y **estado "referenciado / sin referenciar"** (dato real de Marlin). No se añadió Home.
- **Extrusión:** si el hotend no alcanza la temperatura mínima, **Extruir y Retraer quedan grises (sin toque)** y se indica "Hotend frío (mín. N°)" — antes no pasaba nada sin explicación. Acceso directo a Temperatura. El contador de mm se pone a 0 al entrar.
- **Opciones:** rejilla de accesos: Temperatura, Filamento, Ventiladores, Autoapagado (verde cuando está activo, con su estado escrito), Velocidad y Babystep (imprimiendo) o Extruir y Mover (sin imprimir/pausada, igual que la original), Objetos (si el G-code define objetos).
- **Cambio de filamento durante la impresión:** ahora pide confirmación ("se pausará la impresión y el cabezal se moverá…"). Es la única confirmación nueva; la secuencia posterior es la original.
- **Explorador:** lista de 6 filas con icono carpeta/archivo, nombre (recortado con "…") y flecha en las carpetas; botones de página; cabecera con el nombre de la carpeta; mensajes "Sin tarjeta SD" / "Carpeta vacía". Atrás sube de carpeta y, en la raíz, vuelve al Dashboard. **No muestra las miniaturas de cada archivo** que dibujaba la original (cada una requería abrir y leer el archivo y 2 objetos por fila).
- **Confirmar impresión:** nombre del archivo y botones Cancelar / Imprimir. Los botones llaman a los **mismos manejadores originales** (`btn_ok_event_cb` / `btn_cancel_event_cb` de `draw_dialog.cpp`): la lógica que inicia la impresión no se tocó ni se copió.

## 3. Seguridad: qué llamadas actúan sobre la máquina
Todas son las de las pantallas originales, con los mismos límites; se copiaron literalmente en `arquis_controls.cpp`:
| Acción | Llamadas |
|---|---|
| Temperatura + / − / Apagar / PLA / ABS | `temp_hotend[].target`, `temp_bed.target`, `setTargetHotend/Bed`, `start_watching_*`, con los topes `HEATER_0_MAXTEMP − (WATCH_TEMP_INCREASE + TEMP_HYSTERESIS + 1)` y equivalente de la cama. **Protecciones térmicas sin tocar.** |
| Ventilador | `thermalManager.set_fan_speed(índice, 0‥255)` |
| Mover | `queue.inject("G91\nG1 <eje><dist> F<vel>\nG90")` solo si `!queue.ring_buffer.full(3)` |
| Extruir / Retraer | `queue.inject("G91\nG1 E<±paso> F<vel>\nG90")` solo si `hotEnoughToExtrude()` (además de estar deshabilitado el botón) |
| Autoapagado | `gCfgItems.finish_power_off` + `update_spi_flash()` |
| Cambio de filamento | `card.pauseSDPrint(); stop_print_time(); print_state = PAUSING` y `lv_draw_filament_change()` (como la original) |
| Imprimir archivo | manejadores originales de `draw_dialog.cpp` |
Con `ARQUIS_UI` desactivado, los cinco archivos `draw_preHeat/fan/move_motor/extrusion/operation.cpp`, `draw_print_file.cpp` y `draw_dialog.cpp` compilan **exactamente** el código original (todo lo original está bajo `#else`/`DISABLED`; el diff de Git solo contiene líneas añadidas).
Con una impresión en marcha el Dashboard sigue bloqueando Mover y Ajustes, y no se abre el explorador (`card.mount()`).

## 4. Qué se comprobó y qué NO
| Comprobado | NO comprobado |
|---|---|
| Vistas con LVGL real en ARM-32, heap de 16 KB: dibujo, toques (cada control envía su id), botones deshabilitados, memoria, fugas | **Compilación completa de Marlin** (nunca se ha compilado esta etapa) |
| Firmas de Marlin usadas (contra cabeceras de prueba y las cabeceras reales de `cancel_object.h`, `draw_print_file.h`) | Pantalla TS35 real: colores, tamaños táctiles, legibilidad |
| Código original intacto con `ARQUIS_UI` desactivado (el diff no borra líneas) | Comportamiento real: temperaturas, movimientos, extrusión, paginación con tu SD, carpetas anidadas, iniciar una impresión desde el explorador |
Flash: las vistas nuevas miden unos 11 KB de código (Thumb, `-Os`) y los iconos nuevos unos 7 KB; el código de glue añade algunos KB más. El código original de las pantallas sustituidas deja de compilarse con `ARQUIS_UI` activo, pero **no lo medí**: la cifra real solo sale de compilar y ejecutar `auditar_flash.py`. Antes de la etapa quedaban unos 65 KB libres.

## 5. Prueba segura en la impresora (sin calentar ni mover mucho)
1. **Archivos:** Imprimir → debe listar tu SD; entra en una carpeta, sube página/baja página, Atrás (sube de carpeta y, en la raíz, vuelve al Dashboard). Toca un archivo → "¿Imprimir este archivo?" → Cancelar vuelve a la lista. (No pulses Imprimir si no quieres imprimir.)
2. **Temperatura:** cambia de Hotend a Cama; pulsa +/− y comprueba que el objetivo cambia de 1, 5 o 10; **Apagar** lo deja en 0. No calientes si no quieres: basta con el 0.
3. **Ventiladores:** 50 %, 100 %, Apagar, +/−; comprueba que también cambia Aux 1 y que el Dashboard lo refleja.
4. **Mover:** con la cama despejada: estado "Ejes sin referenciar" hasta hacer Home; mueve 0,1 mm y comprueba el sentido de cada flecha (X+, Y+, Z+). Cuidado con los límites: igual que la original, no hay tope de seguridad propio si no se hizo Home.
5. **Extrusión en frío:** debe mostrar "Hotend frío" y los dos botones grises.
6. **Opciones** (solo se llega desde la supervisión, durante una impresión de prueba `G4 S1` o pausada): comprueba los accesos y el interruptor Autoapagado. Probar **Filamento** solo con la cama despejada: pausa la impresión y mueve el cabezal.

## 6. Riesgos conocidos / pendiente
- La ruta de Opciones → Velocidad, Babystep y Cambio de filamento abre aún las pantallas MKS originales (**Etapa 5**), igual que Herramientas y Ajustes.
- Las cuatro pantallas con paso/estado (Temperatura, Mover, Extrusión y Opciones) dependen de que `uiCfg.*` tenga los valores por defecto de Marlin (`stepHeat`, `move_dist`, `extruStep`…), que se inician en `draw_ui.cpp`.
- La perilla (encoder) recorre los botones, pero en el explorador y en Cancelar objeto no recorre las filas/objetos (un solo objeto LVGL por lista).
- Sin miniaturas en la lista de archivos (ver §2).
