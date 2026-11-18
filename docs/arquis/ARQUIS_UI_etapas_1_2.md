# ARQUIS UI — Etapa 1 (sistema visual) y Etapa 2 (Dashboard funcional)

**Estado:** código escrito y verificado en el PC. **El firmware completo NO se ha compilado ni probado en la impresora.**
Rama: `claude/nice-gauss-y90u12`. `main` intacta. La pantalla principal original sigue en el código (`#else` de `draw_ready_print.cpp`).
Activar/desactivar: `#define ARQUIS_UI` en `Configuration.h` (comentarlo restaura la pantalla original MKS).

## 1. Qué se hizo
**Etapa 1 (cierre)**: logotipo oficial (splash + marca de encabezado), iconos de estado, iconos de ventilador, estados de temperatura, diálogo de confirmación, foco para la perilla, pruebas de memoria.
**Etapa 2**: Dashboard conectado a datos reales de Marlin, acceso contextual a Imprimir, ventiladores, información de impresión.

## 2. Archivos
Creados en `Marlin/src/lcd/extui/mks_ui/` (separación pedida: identidad / componentes / navegación / lectura / acciones):
| Capa | Archivos |
|---|---|
| Identidad visual | `arquis_theme.h/.cpp`, `arquis_icons.h/.c` (generado), `arquis_logo.h/.c` (generado), `arq_font_text16.c`, `arq_font_num36.c`, `arq_font_num20.c` (generados) |
| Componentes | `arquis_widgets.h/.cpp` |
| Lógica pura (probada) | `arquis_logic.h/.cpp` |
| Lectura de datos (solo lectura) | `arquis_data.h/.cpp` |
| Navegación | `arquis_nav.h/.cpp` |
| Pantalla | `arquis_dashboard_view.h/.cpp` (solo LVGL), `arquis_dashboard.h/.cpp` (conexión con Marlin y acciones) |

Modificados (cambios mínimos, nada de control de la máquina):
- `Marlin/Configuration.h`: `#define ARQUIS_UI` (+ ya existente: `ARQUIS_TFT_BYTE_SWAP` desactivado).
- `Marlin/src/lcd/extui/mks_ui/draw_ready_print.cpp`: delega en el Dashboard bajo `#if ENABLED(ARQUIS_UI)`; el código original queda en el `#else`. `lv_temp_refr()` y `lv_clear_ready_print()` también delegan. **`draw_ui.cpp` no se tocó**: `GUI_RefreshPage()` sigue llamando a `lv_temp_refr()` cada segundo.

Herramientas (`herramientas/arquis_ui/`): `arquis_iconos.py`, `arquis_logo.py`, `arquis_fuentes.sh`, `sim/` (simulador, pruebas, verificación de API), `stub/`.
Recursos de SD: `sd_assets/bmp_logo.bin` (splash nuevo). Los demás `bmp_*.bin` no cambian (el Dashboard no los usa).

## 3. Integración con Marlin (funciones reales usadas, ninguna inventada)
| Dato / acción | Origen real |
|---|---|
| Hotend | `thermalManager.wholeDegHotend(0)`, `degTargetHotend(0)`, histéresis `TEMP_HYSTERESIS` |
| Cama | `thermalManager.wholeDegBed()`, `degTargetBed()`, `TEMP_BED_HYSTERESIS` |
| Ventilador de capa / otros | `thermalManager.fanSpeedPercent(n)` (`HAS_FAN0`, `HAS_FAN1`, `FAN_COUNT`) |
| Ventilador de disipador | solo si hay `E0_AUTO_FAN_PIN`: Marlin no expone su estado; se muestra según su regla (`EXTRUDER_AUTO_FAN_TEMPERATURE`/`_SPEED`). **En tu configuración no hay pin → no se muestra.** Sin RPM. |
| Imprimiendo / pausada | `printingIsActive()`, `printingIsPaused()` |
| Error | `IsStopped()`, `marlin_state == MF_KILLED` |
| Archivo y % | `card.isFileOpen()`, `card.longest_filename()`, `card.percentDone()` (solo si es un archivo de SD) |
| Tiempo transcurrido | `print_job_timer.duration()` |
| Tiempo restante | misma fórmula de Marlin (`elapsed·(100−p)/p`), **solo** con ≥2 % de avance y ≥60 s; si no, "--:--" |
| Nombre de la máquina | `MACHINE_NAME` (`CUSTOM_MACHINE_NAME`) |
| Imprimir (sin trabajo) | `lv_draw_print_file()` / `lv_draw_media_select()` (igual que el botón original) |
| Imprimir (con trabajo) | `lv_draw_printing()` con los mismos indicadores que `draw_return_ui()` |
| Mover / Temp. / Herram. / Ajustes / Ventiladores | `lv_draw_move_motor()`, `lv_draw_preHeat()` (`uiCfg.curTempType`), `lv_draw_tool()`, `lv_draw_set()`, `lv_draw_fan()` |

Navegación: se reutilizan `lv_screen_create()`, `disp_state` y `disp_state_stack` (PRINT_READY_UI); no hay segundo sistema.

## 4. Decisiones de diseño
1. **Un componente = un objeto LVGL.** En LVGL 6.1.1-MKS cada objeto cuesta ~340–380 B del heap de 16 KB (el `lv_obj_t` lleva `char mks_pic_name[255]`). Cada tarjeta dibuja fondo, iconos, textos y punto de estado con `lv_draw_*`.
2. **Iconos tintables dentro del firmware** (máscaras alpha de 4 bits; 40 px y 20 px): sin archivos en la SD ni cuadros de otro color; el mismo icono sirve para Normal / Seleccionado / Activo / Deshabilitado / Advertencia / Error (`arq_icon_state_tint`).
3. **Colores**: azul = navegación e interacción (borde al pulsar o con foco); verde = correcto/activo (temperatura alcanzada, imprimiendo, ventilador girando); ámbar = calentando y pausa; rojo = error y acciones peligrosas; gris = apagado/desactivado. El botón Imprimir ya **no** es un bloque azul permanente: es secundario y, con trabajo activo, se convierte en la tarjeta de impresión con barra **verde** (ámbar en pausa).
4. **Con impresión activa**, "Herram." (precalentar, nivelar, cambiar filamento) se oculta; quedan Mover, Temp. y Ajustes.
5. **"Enfriando"** usa azul en el punto del estado (decisión a confirmar: es el único uso del azul como estado, no como acción).
6. **Splash**: es el logo que Marlin dibuja en crudo al arrancar (`bmp_logo.bin` en la flash SPI), sin animación: animar exigiría tocar el arranque anterior a LVGL. Muestra logo oficial sin deformar + "ARQUIS 3D".

## 5. Medidas
Heap LVGL (16 384 B; tras `lv_init` hay 1 508 B usados), medido en ARM de 32 bits:
| | Heap |
|---|---|
| Dashboard en reposo | 3 872 B (24 %) |
| Dashboard con impresión | 3 488 B |
| Diálogo de confirmación | 1 508 B (4 objetos) |
| Dashboard + diálogo (peor caso) | ≈ 5,4 KB sobre la base |

Flash/RAM del código nuevo (Cortex-M4, `arm-none-eabi-gcc 13.2 -Os`): **cota superior 43,9 KB Flash y 3,1 KB RAM** (iconos+logo 19,2 KB, fuentes 16,7 KB, código 8,1 KB). El enlazador descarta lo no usado, por lo que el valor real será menor. Con la referencia anterior (Flash 80,4 %, RAM 57,4 %) quedaría en torno a ≤ 90 % de Flash. **Debe medirse en la compilación real.**

## 6. Pruebas (cada una con su tipo)
**Simulador PC (LVGL 6.1.1-MKS real, ARM 32 bits con qemu-user, búfer 480×14, heap 16 KB):** todas pasan.
- Creación/destrucción: 300 ciclos del Dashboard en los 7 estados → heap final = base (sin fugas ni fragmentación).
- Navegación: 200 idas y vueltas Dashboard ↔ pantalla de 12 botones, alternando reposo/imprimiendo → heap = base.
- Temperaturas: 3 000 refrescos con cambios de estado → heap constante.
- Diálogos: 300 aperturas/cierres → heap constante; el velo bloquea toques; confirmar y "Seguir" funcionan.
- Táctil: cada acceso envía el id correcto; con impresión activa, tarjeta de impresión y accesos conservados.
- Perilla: recorre las 8 tarjetas, pulsar dispara la acción, 100 ciclos sin fugas.
- Redibujo: snapshot idéntico → nada que redibujar; un cambio → 2 zonas.
- Archivos (apertura/cierre): **no aplicable aún** (el explorador es de la Etapa 4).
**Lógica pura (nativo): 43/43** — temperaturas con la histéresis de Marlin, los 7 estados y sus prioridades, tiempo restante (casos límite), formato H:MM, buffers.
**Compilación ARM** (`-Wall -Wextra`, Cortex-M4): componentes, vista, lógica, iconos, logo y fuentes sin advertencias propias.
**Verificación de API de Marlin:** `arquis_data/nav/dashboard.cpp` compilan contra *stubs* que replican las firmas reales leídas en Marlin 2.1.2.10. **No es una compilación de Marlin.**

## 7. NO verificado (hay que hacerlo en tu equipo)
- Compilación completa con PlatformIO (entorno Robin Nano V3.1) → resultado real de Flash/RAM.
- Aspecto en la TS35 real (colores, nitidez de la marca a 32 px, anchos de texto).
- Táctil real, perilla real, rendimiento del refresco con SPI.
- Los estados con datos reales de impresión (el Dashboard solo se ve durante una impresión si se vuelve a él desde la supervisión: ver riesgos).
- Impresión real, pausa, error.

## 8. Riesgos y limitaciones conocidas
1. **Dashboard durante la impresión:** la pantalla de supervisión original no tiene botón para volver al Dashboard. Hasta la Etapa 3 la tarjeta de impresión y el acceso contextual solo son alcanzables en estados poco habituales (p. ej. después de volver con un trabajo en pausa). Se resolverá con el botón "Inicio" en la Etapa 3.
2. "Herram." abre el menú de herramientas original (incluye nivelación): no se ejecuta nivelación directamente desde el Dashboard. La calibración propia es la Etapa 5.
3. Sin ventilador de disipador configurado en Marlin, solo se muestran capa y Aux 1. No se puede saber qué conecta el usuario en FAN1; por eso se llama "Aux 1".
4. La marca del logo a 32 px pierde detalle del circuito (el símbolo es reconocible, el detalle fino no).
5. Fuentes Inter: cubren Latin-1 (á é í ó ú ñ ü ¿ ¡ °). Texto fuera de ese rango (p. ej. nombres de archivo con otros alfabetos) puede no dibujarse; los nombres largos se recortan con "...".
6. Memoria: 4 B persistentes tras el primer ciclo con perilla (estructura global de LVGL, no crecen).
7. Cota de Flash ~90 %: queda poco margen para Etapas 3–5; se optimizará en la Etapa 6.

## 9. Cómo reproducir
`herramientas/arquis_ui/sim/build_sim.sh` (descarga LVGL 6.1.1-MKS, compila, ejecuta pruebas y regenera capturas) y `herramientas/arquis_ui/sim/check_marlin_api.sh`.
Capturas en `docs/arquis/ui/`.
