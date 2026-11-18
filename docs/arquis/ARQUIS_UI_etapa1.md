# ARQUIS UI — Etapa 1: sistema visual

Estado: **demostración visual para revisión**. No se ha tocado ninguna pantalla existente ni la lógica de Marlin.
El firmware **no se ha compilado completo** en esta etapa (ver "Compilación").

## Qué se creó
| Archivo (en `Marlin/src/lcd/extui/mks_ui/`) | Contenido |
|---|---|
| `arquis_theme.h/.cpp` | Paleta ARQUIS, geometría 480×320, estilos estáticos (sin heap), tipografía |
| `arquis_widgets.h/.cpp` | Componentes: tarjeta/botón de navegación, botón, chip de estado, tarjeta de temperatura, cabecera, barra de progreso |
| `arquis_icons.h/.c` | 21 iconos propios 40×40 (máscara de 4 bits, 800 B c/u, en Flash). **Generado** |
| `arq_font_text16.c`, `arq_font_num36.c`, `arq_font_num20.c` | Tipografía Inter (SIL OFL): texto Latin-1 (á é ñ ¿ ¡ °) y cifras grandes. **Generado** |

Herramientas (en `herramientas/arquis_ui/`): `arquis_iconos.py` (SVG → iconos), `arquis_fuentes.sh` (fuentes), `sim/` (simulador PC con LVGL real).

## Decisiones de diseño
1. **Un componente = un objeto LVGL.** En LVGL 6.1.1-MKS cada objeto cuesta ~340 B del heap de **16 KB** (el `lv_obj_t` trae un `char mks_pic_name[255]`). Un diseño con panel+icono+etiquetas separados agotaba el heap con solo el Dashboard. Cada tarjeta dibuja fondo, icono, textos y punto de estado con `lv_draw_*` en su propio callback.
2. **Iconos tintables, dentro del firmware.** Máscaras alpha de 4 bits teñidas en tiempo de ejecución (azul, blanco, verde, ámbar, rojo, gris con un mismo icono). No dependen de la SD ni de `pic_manager` (límite de 100 imágenes) y no tienen fondo pintado (adiós a los cuadros de otro color).
3. **Tipografía propia en Flash**, sin la fuente china de la SPI-flash para el texto ARQUIS. Texto Latin-1 completo, cifras grandes dedicadas.
4. **Sin animaciones.** `LV_USE_ANIMATION` sigue en 0; el progreso se actualiza por valor.
5. **Redibujo mínimo.** Los `set`/`update` comparan con el valor anterior; el estado "pulsado" invalida solo la tarjeta.
6. Dos colores añadidos fuera de la paleta base (propuestos): **ámbar #F5A524** (calentando) y **rojo #E5484D** (error / acción peligrosa).

## Medidas (ARM de 32 bits, mismas estructuras que la placa)
Heap LVGL (16 384 B; tras `lv_init` ya hay 1 508 B usados):
| Componente | Heap |
|---|---|
| Botón / tarjeta / chip / barra / tarjeta de temperatura | 372–380 B (1 objeto) |
| Cabecera con Atrás | 752 B (2 objetos) |
| **Dashboard completo (boceto)** | **5 312 B (33 %)** |
| Hoja de 21 iconos (peor caso de prueba) | 10 628 B (65 %) |

Flash/RAM del código nuevo (compilado para Cortex-M4 con `arm-none-eabi-gcc 13.2 -Os`):
| | Flash | RAM estática |
|---|---|---|
| Tema + componentes | 3,5 KB | 2,5 KB |
| 21 iconos | 17,0 KB | 0 |
| 3 fuentes | 16,7 KB | 0,1 KB |
| **Total (cota superior)** | **37,2 KB** | **2,6 KB** |

El enlazador descarta lo que ninguna pantalla use (`--gc-sections`): hasta que se conecte una pantalla, el firmware no crece. Referencia anterior: Flash 80,4 % (381 864 de 475 136 B), RAM 57,4 %; con todo el código nuevo en uso, Flash ≤ ~88 %.

## Pruebas realizadas (simulador PC, LVGL 6.1.1-MKS real, ejecutado en ARM de 32 bits con qemu-user)
- Compilación de los componentes con `-Wall -Wextra` sin advertencias propias.
- Renderizado con el **mismo búfer parcial 480×14** que el firmware.
- Eventos: `PRESSED`/`RELEASED`/`CLICKED` llegan al callback con el `mks_obj_id` correcto; toque real vía `indev` dentro del botón dispara `CLICKED`, fuera no; actualizar con el mismo valor no redibuja.

## NO probado
Compilación completa de Marlin, arranque, aspecto real en la TS35 (colores, brillo), táctil real, impresión. El simulador no emula el panel ni el SPI.

## Compilación
El entorno de desarrollo no llega al registro de PlatformIO ni al zip de LVGL, así que el `.bin` completo debe compilarse en tu PC. Los archivos nuevos son autónomos (solo dependen de LVGL) y no cambian ninguna pantalla existente.

## Capturas
`docs/arquis/ui/`: `dashboard_listo_x2.png`, `dashboard_calentando_x2.png`, `componentes_x2.png`, `iconos_x2.png`, `botones_pulsados.png`.
Regenerar: `herramientas/arquis_ui/sim/build_sim.sh`.
