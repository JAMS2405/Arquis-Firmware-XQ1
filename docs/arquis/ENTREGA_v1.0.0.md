# ARQUIS UI 1.0.0 — Notas de versión y entrega

**Producto:** interfaz táctil propia para la impresora **Arquis XQ1** (Arquis Soluciones Tecnológicas).
**Base:** Marlin 2.1.2.10 · placa MKS Robin Nano V3.1 (STM32F407VET6) · pantalla MKS TS35 480×320 · LVGL 6.1.1-MKS.
**Entorno de PlatformIO:** `mks_robin_nano_v3_1_usb_flash_drive_msc`. **Rama:** `claude/nice-gauss-y90u12` (`main` no se tocó).

## 1. Qué es
Sustituye el aspecto de Makerbase por un sistema visual propio (paleta, tipografía Inter, iconos y logo de ARQUIS) **sin quitar ninguna función de Marlin**. Todas las acciones que mueven la máquina, calientan o guardan datos son las llamadas originales de MKS/Marlin, copiadas literalmente; solo se rediseñó cómo se ven y se añadieron confirmaciones donde una pulsación puede estropear una pieza o la máquina.

## 2. Qué incluye
| Área | Pantallas |
|---|---|
| **Dashboard** | Temperaturas de hotend y cama, ventiladores, estado de la máquina (Reposo, Lista, Calentando, Temp. alcanzada, Enfriando, Imprimiendo, Pausada, Error), accesos Imprimir, Mover, Temp., Herram. y Ajustes; durante una impresión, tarjeta de avance |
| **Impresión** | Explorador de archivos con carpetas, **miniatura del G-code antes de imprimir**, supervisión (progreso, tiempos, altura Z, temperaturas, ventiladores), Pausar/Reanudar, Cancelar con confirmación, **Cancelar objeto** con mapa de la cama, Opciones |
| **Control** | Temperatura (hotend/cama, preajustes), Ventiladores, Mover (con aviso si falta Home), Extruir, Velocidad, Babystep, Home, Nivelar cama (5 puntos), Cambio de filamento |
| **Ajustes** | ~15 pantallas de **Parámetros de máquina** (pasos, aceleración, jerk, velocidades, TMC, sensibilidad de home, nivelación…), **Memoria (EEPROM)**, **Sistema y diagnóstico** (versión, placa, horas, finales de carrera y sensores en vivo), Wi-Fi (estado y lista de redes), Idioma (español / inglés), G-code |
| **Diálogos** | Confirmaciones y avisos de Marlin con el mismo estilo (pausa/M600, reanudar tras corte, errores, carga y descarga de filamento) |
| **Teclados** | Numérico propio (con **Cancelar** y validación) y teclado de texto con los colores de ARQUIS |
Recorrido con capturas: [`GUIA_USUARIO.md`](GUIA_USUARIO.md).

## 3. Cifras (compilación real en tu PC)
| | Valor |
|---|---|
| Flash | **≈ 85,5 %** (406 276 B de 475 136 B útiles; **quedan ≈ 68 KB**) |
| RAM | ≈ 61,6 % (80 708 B de 131 072 B) |
| Código ARQUIS (tras el enlazado) | ≈ 45 KB (11 % de la Flash) |
| Heap de LVGL (16 KB), peor caso medido | ≈ 7 KB usados, **≥ 9,2 KB libres** (pantalla + diálogo + perilla) |
Reparto detallado en [`ARQUIS_UI_etapa_6.md`](ARQUIS_UI_etapa_6.md).

## 4. Lo que NO se modificó (con evidencia)
`git diff origin/main...HEAD` fuera de `Marlin/src/lcd/extui/mks_ui/`, de la documentación y de las herramientas muestra **un único archivo de Marlin cambiado: `Marlin/Configuration.h`**, con dos ajustes de pantalla:
1. `#define ARQUIS_UI` (activa la interfaz; comentarlo devuelve la pantalla original de MKS);
2. `ARQUIS_TFT_BYTE_SWAP` desactivado (con él activo los colores salían rotos).
Por tanto **no cambió** ningún archivo de movimiento (`planner`, `stepper`, `motion`), temperatura (`temperature`, protecciones térmicas), drivers TMC, endstops, SD/impresión de Marlin ni ajustes de la máquina. Dentro de la carpeta de la interfaz, los archivos originales de MKS conservan su código original: los cambios son bloques protegidos con `#if ENABLED(ARQUIS_UI)` / `#else`, de modo que con `ARQUIS_UI` desactivado compila el código de siempre.

## 5. Cambios de comportamiento respecto a la interfaz de MKS
- **Nuevas confirmaciones:** cancelar impresión, cancelar un objeto, cambiar filamento con una impresión en marcha (pausa y mueve el cabezal), nivelación automática, apagar la impresora / motores, restaurar valores de fábrica.
- **Con una impresión en marcha:** *Herram.* se oculta en el Dashboard, y *Mover* y *Ajustes* quedan deshabilitados (mover ejes o desactivar motores arruinaría la pieza).
- **Teclado numérico:** ahora tiene Cancelar (el original solo permitía salir confirmando); *Aceptar* se deshabilita con un valor no numérico; se corrigió un fallo del original (permitía escribir sin dejar el fin de cadena).
- **Estados:** *Reposo* (sin tarjeta SD y todo apagado) y *Lista* (con tarjeta y todo apagado); *Enfriando* con calentador apagado por encima de 50 °C.
- **Autoapagado:** el interruptor solo aparece si la placa tiene control de alimentación (`MKS_PWC`); en tu configuración no lo tiene y no se muestra.
- **Idiomas:** solo español e inglés (los demás se quitaron para liberar ≈ 30 KB de Flash). Idioma por defecto: español.
- **Cancelar objeto:** rediseñada con 6 objetos fijos; la versión anterior agotaba el heap de LVGL y reiniciaba la placa.
- **Aviso de error "PRINTER HALTED"** en español (sin acentos): se dibuja a propósito sin LVGL, para que funcione aunque falle la interfaz.

## 6. Cómo se verificó
| Nivel | Qué se hizo | Resultado |
|---|---|---|
| Simulador (LVGL 6.1.1 real, ARM de 32 bits, heap 16 KB) | Dibujo, toques, memoria y fugas de todas las pantallas; 213 comprobaciones; 52 pruebas de lógica de estados | 0 fallos |
| Firmas de Marlin | Compilación de los archivos nuevos contra cabeceras de prueba y las tablas de idioma reales (`check_marlin_api.sh`, 15 archivos) | OK (no sustituye a la compilación real) |
| Compilación completa con PlatformIO | En tu PC, en cada etapa | SUCCESS |
| Impresora real | Probado por el usuario etapa a etapa (3, 4, 5A, 5B-1/2, y 5B-3/5B-4 + Reposo/Lista de la 6) | Correcto |
Esta compilación (1.0.0) solo añade la línea *Interfaz ARQUIS 1.0.0* en Sistema respecto a la última probada.

## 7. Limitaciones conocidas y pendientes
- **Lectura de la cama a ≈ 45 °C al encender, todo apagado.** La interfaz muestra tal cual `thermalManager.wholeDegBed()`. El hotend marca bien (≈ 28 °C). Hace falta tocar la cama en frío y medir el termistor con un multímetro (≈ 100 kΩ a 25 °C si es el NTC 100 kΩ que indica `TEMP_SENSOR_BED 1`); si no coincide, el arreglo es `TEMP_SENSOR_BED` en `Configuration.h` (configuración térmica: solo con tu aprobación). **Pospuesto por el usuario.**
- **Perilla (encoder):** no opera las listas, teclados ni diálogos nuevos (los botones de cabecera sí). Descartado por decisión del usuario.
- **Apagado automático y control remoto** (MKS PWC, ESP32-S3, OctoPrint): descartados por decisión del usuario. Hallazgo: el firmware ya mete en la cola de Marlin el G-code que llega por el módulo Wi-Fi MKS.
- **Pantallas con aspecto de MKS a propósito:** calibración táctil (dibuja en el TFT), *Parámetros Wi-Fi* y *Nube MKS*; asistente de offset Z (solo existe con sonda).
- **Textos:** los de las tablas de MKS dependen de la traducción propia (134 textos); el texto "Extrusion1: 187 / 210" del diálogo de calentamiento lo compone el código original y sigue en inglés. En chino o ruso no se verían con la tipografía de ARQUIS (esos idiomas se quitaron).
- **Recurso ausente de origen:** `bmp_mov_changespeed.bin` figura en la lista de MKS pero tampoco está en sus recursos; no se usa con ARQUIS UI.
- **Aviso de compilación de Marlin** sobre los jumpers DIAG (`SENSORLESS_HOMING` desactivado): es un recordatorio de hardware (con finales de carrera físicos los jumpers DIAG de los drivers deben estar quitados). Se puede silenciar con `DIAG_JUMPERS_REMOVED` solo si ya los quitaste.
- **Compilación en Windows:** si PlatformIO falla con `TMCStepper.h` en la unidad `F:`, usa `PLATFORMIO_WORKSPACE_DIR` en otra unidad (ver la guía de instalación).

## 8. Reversibilidad
Comentar `#define ARQUIS_UI` en `Configuration.h` y recompilar devuelve la interfaz original de MKS, **incluidos todos sus idiomas** (el recorte a español e inglés solo se aplica con `ARQUIS_UI` activo). Para volver a un firmware anterior basta con flashear su `.bin` guardado.

## 9. Contenido del repositorio relevante
| Ruta | Qué es |
|---|---|
| `Marlin/src/lcd/extui/mks_ui/arquis_*` | Interfaz ARQUIS (vistas puras de LVGL y pegamento con Marlin) |
| `Marlin/src/lcd/extui/mks_ui/draw_*.cpp` | Pantallas originales de MKS con bloques `ARQUIS_UI` |
| `sd_assets/` | Carpeta `assets` para la SD (logo de arranque, iconos, fuente) |
| `herramientas/arquis_ui/sim/` | Simulador y pruebas (`build_sim.sh`, `check_marlin_api.sh`) |
| `herramientas/arquis_ui/auditar_flash.py` | Reparto de Flash/RAM tras compilar |
| `herramientas/arquis_ui/empaquetar_entrega.py` | Arma el paquete para la SD |
| `docs/arquis/` | Documentación por etapa, guías y capturas |
| `CAMBIOS.md` | Historial de cambios |
