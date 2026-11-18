# ARQUIS UI — Etapa 5 (parte A): herramientas, calibración y sistema

Rama `claude/nice-gauss-y90u12`. `main` intacta. **Estado: simulador y comprobación de firmas OK. NO compilada con Marlin completo y NO probada en la impresora.**

## 1. Qué incluye
| Pantalla | Qué ofrece | Objetos | Heap (sim.) |
|---|---|---|---|
| **Herramientas** | Temperatura, Extruir, Mover, Home, Nivel. auto (con confirmación), Nivelar cama, Filamento*, Más* | 10 | 3,5 KB |
| **Home** | Home todo / X / Y / Z, Motores off, Motores XY off | 8 | 3,5 KB |
| **Ajustes** | Parámetros*, Memoria (EEPROM)*, Ventiladores, Sistema, Idioma* y Wi-Fi* (si están activos), Apagar (con confirmación) | 6–9 | 3,5 KB |
| **Sistema y diagnóstico** | Máquina, firmware, placa, compilación, tiempo encendida, nº de impresiones y tiempo total (PRINTCOUNTER) + sensores de hotend y cama, finales de carrera X/Y/Z en vivo, tarjeta SD, estado de Marlin | 4 | 1,6 KB |
| **Babystep** (calibración Z) | Mismos mandos que Mover con pasos 0,01 / 0,05 / 0,1 mm y el **offset Z** visible; se guarda al volver (como la original) | 12 | 5,1 KB |
| **Velocidad** | Movimiento / Extrusión en %, − / + con paso 1, 5 o 10 | 11 | 3,9 KB |
| **Nivelar cama** | Esquema de la cama con los 5 puntos en su posición real (`trammingPos`), instrucciones | 9 | 3,5 KB |
\* Estas entradas abren aún la pantalla MKS original (ver §5).

400 + 400 ciclos de crear/destruir las cuatro pantallas nuevas: sin fugas. Peor caso con diálogo abierto: >9 KB libres de 16 KB.

## 2. Seguridad
- Las llamadas a Marlin son las de las pantallas originales (`draw_tool/home/set/baby_stepping/change_speed/manuaLevel/about.cpp`), copiadas literalmente: `G28…`, `M84`, `M290`, `suicide()`, `feedrate_percentage`, `planner.flow_percentage`, `settings.save()` al salir de Babystep, el comando de nivelación guardado en la flash SPI, etc.
- **Confirmaciones nuevas** (la original no las pedía): **Nivelación automática** (recorre la cama), **Apagar** la impresora / motores (`suicide()` corta la alimentación) y, desde la Etapa 4, Cambio de filamento imprimiendo.
- Home, Motores off y los puntos de Nivelar cama siguen sin confirmación (como la original): Home mueve los ejes hacia los finales de carrera y cada punto sube a Z10, se desplaza y baja a **Z0**. Despeja la cama antes.
- Con una impresión en marcha, Herramientas y Ajustes siguen bloqueados desde el Dashboard.
- Con `ARQUIS_UI` desactivado compila el código original (los diffs solo añaden líneas).
- Diagnóstico: los finales de carrera se leen con `READ(pin) != *_ENDSTOP_INVERTING`, la misma regla de M119; los sensores se marcan "Revisar" si leen ≤ 0 °C o cerca del máximo configurado. Son comprobaciones de lectura, no tocan nada.

## 3. Hallazgo durante la integración
La comprobación con cabeceras de Marlin detectó que mi variable `settings` chocaba con el objeto global `settings` (EEPROM) de Marlin: habría dado un error en la compilación real. Renombrada. (Los demás nombres estáticos se compararon con las cabeceras de Marlin.)

## 4. Qué se comprobó y qué NO
Comprobado: dibujo y toques en el simulador con LVGL real; memoria y fugas; firmas de las llamadas de Marlin contra cabeceras de prueba (`check_marlin_api.sh`, 13 archivos).
**NO comprobado:** compilación completa de Marlin; pantalla real; los finales de carrera y sensores del diagnóstico con tu placa; los pasos de babystep con tu sonda (`probe.offset.z` solo se muestra si hay sonda); el ancho de los textos en la pantalla física.

## 5. Etapa 5, parte B
**Probado en tu impresora (parte A):** todo funcionó correctamente. Primera compilación completa con la parte A: Flash 90,3 % (428 944 de 475 136 B; quedan unos 46 KB), RAM 60,8 %. Para compilar en tu PC hubo que mover el espacio de trabajo de PlatformIO a `C:` (la unidad `F:` dejaba las librerías incompletas): `$env:PLATFORMIO_WORKSPACE_DIR = "C:\pio_arquis"`.

### 5B-1. Parámetros de máquina y teclado numérico (hecho, sin probar en la impresora)
- **Qué cambia:** las ~15 pantallas de *Ajustes → Parámetros* (Configuración de la máquina, Motores, Pasos por mm, Aceleración, Jerk, Velocidades máximas, Corriente TMC, Modo silencioso, Sensibilidad de home, Ajustes avanzados, Nivelación, Posiciones de nivelación, Offset boquilla-sonda, Filamento, Perilla, Posición de pausa y **Memoria/EEPROM**) usan una lista ARQUIS de un solo objeto: **1,1–1,5 KB de heap** por pantalla (las originales usaban unos 7 KB).
- **Cómo:** no se reescribió la lógica de ninguna pantalla. Se redibujan los ayudantes originales (`lv_screen_create`, `lv_big_button_create`, `lv_screen_menu_item*`) cuando la pantalla es de parámetros; cada pantalla conserva sus manejadores, ids y llamadas a Marlin. Con `ARQUIS_UI` desactivado compila el código original.
- **Español:** MKS no trae este menú en español (solo inglés, chino y ruso). Hay una tabla de 92 textos (`arquis_params.cpp`, ~3 KB de flash) que traduce cada texto por el campo de la tabla de idioma al que pertenece. Si cambias el idioma a otro (p. ej. chino) el texto se muestra sin traducir y la tipografía ARQUIS no tiene esos caracteres.
- **Teclado numérico nuevo:** visor grande con el nombre del parámetro, teclas 1-9, 0, `.`, `-`, ← (borrar un dígito), **Borrar**, **Cancelar** (el original no tenía: solo podías salir confirmando) y **Aceptar**, que se deshabilita si el valor no es un número válido (p. ej. solo "-" o "."). La lógica de lectura/escritura (`disp_key_value`, `set_value_confirm`, `M500`) es la original.
- **Corrección de un fallo del original:** el teclado permitía escribir 11 caracteres en un buffer de 11 bytes sin dejar el `\0` final; ahora el máximo es 10.
- **Limitación conocida:** la lista y las teclas no responden a la perilla (como el explorador de archivos); Volver y Siguiente/Anterior sí.

### 5B-2. Diálogos de Marlin/MKS y Cambio de filamento (hecho, sin probar en la impresora)
- **Diálogos** (`draw_dialog.cpp`): el código original sigue decidiendo el tipo de diálogo, los textos, los botones y los manejadores; al final se leen esos textos y botones y se vuelven a dibujar como pantalla ARQUIS (título *Confirmar* o *Aviso*, mensaje centrado, Cancelar a la izquierda y Aceptar a la derecha). Cubre: Guardar / Leer / Restaurar valores de fábrica de la Memoria, mensajes de pausa y cambio de filamento (M600: estacionar, esperar, insertar, cargar, purgar, reanudar, "¿Purgar más o continuar?"), "calentamiento completo", "carga/descarga completada", reanudar desde el punto de corte (corte de luz), errores de archivo/comunicación y Wi-Fi. **1,5 KB de heap.**
- **Rojo = peligro:** *Detener impresión* y *Restaurar valores de fábrica* llevan el botón Aceptar en rojo.
- **Los diálogos con temperatura en vivo o barra de progreso** (calentando para cargar/descargar y cargando/descargando filamento) **siguen con el aspecto original**: sus objetos los actualiza otro código cada segundo y no quise arriesgar un cierre inesperado. Es lo siguiente a rediseñar.
- **Textos en español:** 42 mensajes de diálogo más en la tabla de traducción (ahora 134 textos).
- **Cambio de filamento** (`draw_filament_change.cpp`): pantalla de entrada con la temperatura del hotend y dos botones grandes **Cargar / Descargar** (el selector de extrusor solo aparece con varios extrusores). Los ids, el manejador y toda la lógica (calentar a la temperatura de filamento, avisos, M600) son los originales. **2 KB de heap.**
- **Flash estimada:** los archivos nuevos de 5B suman unos 9 KB (vista 4,1 KB + tabla de textos y pegamento 4,7 KB); el teclado original queda excluido, así que el aumento real es algo menor. Se confirma con `auditar_flash.py`.
- **Limitación conocida:** las perillas (encoder) no operan los diálogos nuevos ni las listas (los botones sí, en Cambio de filamento).

### 5B-3. Más, G-code, Idioma, teclado de texto, diálogos de progreso y aviso de error (hecho, sin probar en la impresora)
- **Más** (`draw_more.cpp`): rejilla ARQUIS con **G-code** y, si tu configuración los define (`CUSTOM_MENU_MAIN`; hoy no), los accesos personalizados con la misma acción original (`queue.inject`).
- **G-code** (`draw_gcode.cpp`): botón *Escribir un comando G-code* (abre el teclado de siempre) y panel de resultado. Mismo manejador e ids que el original; el resultado se lee de `public_buf` al dibujar (igual que antes: se ve al volver de enviar el comando).
- **Idioma** (`draw_language.cpp`): lista con los 7 idiomas, el activo marcado en verde. Hace lo mismo que la original (guarda en la flash SPI con `update_spi_flash()` y recarga las tablas con `disp_language_init()`). La interfaz ARQUIS está en español: este ajuste solo cambia los textos de las pantallas de MKS que aún no tienen traducción propia (y esos tienen tabla ES). Chino y ruso no se verán con la tipografía ARQUIS. Solo aparece en Ajustes si tu configuración lo habilita.
- **Teclado de texto** (`draw_keyboard.cpp`): misma lógica (es el teclado de LVGL); solo cambian los colores (teclas gris azulado con borde azul al pulsar, campo de texto oscuro). Se conservan los símbolos (borrar, flechas, aceptar) porque la tipografía ARQUIS no los tiene.
- **Diálogos con barra de progreso** (calentando para cargar/descargar y cargando/descargando filamento): se conservan sus objetos (`tempText1` y `filament_bar`, que actualiza `filament_dialog_handle`) y solo se les cambia el estilo; el mensaje queda arriba en un panel compacto y el botón *Cancelar* abajo (neutro: cancelar no es la acción principal). El texto "Extrusion1: 187 / 210" lo compone el código original y sigue en inglés.
- **Aviso de error "PRINTER HALTED"** (`draw_error_message.cpp`): se dibuja directamente en el TFT sin LVGL (a propósito: debe funcionar aunque falle la interfaz). Solo se traducen sus dos líneas, "IMPRESORA DETENIDA / Reinicia la impresora", sin acentos porque esa fuente no los tiene. El mensaje de error de Marlin que aparece encima sigue en inglés.
- **No se tocan:** la calibración táctil (dibuja las cruces directamente en el TFT y es un ajuste de hardware) y el asistente de offset Z (solo existe con sonda; tu configuración usa `PROBE_MANUALLY`). Tampoco las pantallas de Wi-Fi.

**La Etapa 5 queda cerrada** salvo lo anterior y las limitaciones ya indicadas: las listas, los teclados y los diálogos nuevos no responden a la perilla (los botones de cabecera sí), y las pantallas con texto de tablas de MKS dependen de mi tabla de traducción (134 textos).

## 6. Prueba segura
1. Dashboard → Herram.: comprueba cada acceso y que **Atrás** vuelve al Dashboard.
2. **Sistema:** deben verse tus datos reales; mueve un final de carrera con la mano y comprueba que cambia (y que Z "Activado" = el sensor activo).
3. **Velocidad / Babystep:** solo desde una impresión en marcha (Opciones) o con el hotend frío; el offset Z debe coincidir con `M851`.
4. **Nivelar cama (con la cama despejada):** el primer punto hace Home; después cada punto sube, se desplaza y baja a Z0.
5. **Ajustes → Apagar:** debe pedir confirmación; pulsa Volver.
