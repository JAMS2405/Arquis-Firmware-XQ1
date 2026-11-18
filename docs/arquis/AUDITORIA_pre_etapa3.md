# ARQUIS UI — Auditoría técnica previa a la Etapa 3

Rama `claude/nice-gauss-y90u12`. `main` intacta. Etapa 3 **no** iniciada.
Decisiones visuales autorizadas e incorporadas: azul en el indicador "Enfriando"; "Herram." oculto durante una impresión; botón "Inicio" (se hará en la Etapa 3, ver C3).

## 0. Resumen
| | Resultado |
|---|---|
| Compilación completa de Marlin (`mks_robin_nano_v3_1_usb_flash_drive_msc`) | **NO realizada.** Ver §7: la plataforma, la toolchain ARM (GCC 9.2.1) y CMSIS se instalaron, pero el core Arduino STM32 (zip del fork `rhapsodyv/Arduino_Core_STM32`) lo bloquea la política de GitHub del entorno. **El firmware NO está validado.** |
| Archivos ARQUIS compilados con el GCC 9.2.1 de PlatformIO (Cortex-M4, `-Wall -Wextra`) | Sin errores ni advertencias propias |
| Verificación contra firmas de Marlin | Compila contra *stubs* con las firmas reales. **No equivale a compilar Marlin.** |
| Simulador LVGL real (ARM 32 bits) | Todas las pruebas pasan (ver §6) |
| Lógica pura | 43 / 43 |
| Errores críticos | 3 (2 corregidos, 1 pendiente para la Etapa 3) |
| Margen de Flash estimado | ≈ 61 KB libres (87 %); ≈ 49 KB en el peor caso |

## 1. Archivos revisados
**Nuevos (ARQUIS):** `arquis_theme`, `arquis_widgets`, `arquis_logic`, `arquis_data`, `arquis_nav`, `arquis_dashboard`, `arquis_dashboard_view` (.h/.cpp), `arquis_icons`, `arquis_logo` (.h/.c, generados), `arq_font_text16.c`, `arq_font_num36.c` (generados).
**Modificados:** `Marlin/Configuration.h` (+`#define ARQUIS_UI`), `draw_ready_print.cpp` (delegación bajo `#if ENABLED(ARQUIS_UI)`; el original queda en el `#else`).
**Leídos de Marlin para contrastar:** `MarlinCore.cpp/.h`, `module/temperature.h`, `module/printcounter.h`, `sd/cardreader.h/.cpp`, `lcd/marlinui.h`, `Configuration*.h`, y de MKS: `draw_ui.cpp/.h`, `draw_ready_print`, `draw_printing`, `draw_tool`, `draw_set`, `draw_move_motor`, `draw_preHeat`, `draw_fan`, `draw_print_file`, `draw_media_select`, `tft_lvgl_configuration.cpp`, `pic_manager`, `tft_multi_language`.

Dependencias entre archivos (acíclicas): `theme → icons`; `widgets → theme, logo, logic`; `data → logic`; `dashboard_view → widgets, data`; `nav → data, draw_ui`; `dashboard → view, nav, widgets, draw_ui`. Solo `arquis_data`, `arquis_nav` y `arquis_dashboard` tocan Marlin; el resto es LVGL puro.

## 2. Integración (resultado)
| Punto | Resultado |
|---|---|
| Marlin 2.1.2.10 | Macros y funciones usadas verificadas en el código: `TEMP_HYSTERESIS`/`TEMP_BED_HYSTERESIS` (Configuration.h:590/596, dentro de `#if`: se añadió valor por defecto 3), `printingIsActive/Paused` (`MarlinCore.cpp:340/345`), `card.*`, `print_job_timer.duration()`, `thermalManager.*`, `ui8_to_percent`, `HAS_FAN0/1`, `FAN_COUNT`, `PIN_EXISTS`. Compilación real: pendiente (§7). |
| LVGL 6.1.1-MKS | Compilado y ejecutado contra la librería real. Usa `lv_obj_set_event_cb_mks` (parche MKS) y `lv_group`. |
| Pila de pantallas | Se reutilizan `lv_screen_create`, `disp_state`, `disp_state_stack`. `lv_clear_cur_ui()` (draw_ui.cpp:905) llama a `lv_clear_ready_print()` → `arq_dashboard_clear()`: cualquier salida "externa" (diálogos de error, calibración táctil) pasa por nuestro limpiador. `GUI_RefreshPage` no se modificó. |
| Eventos táctiles | Se actúa al soltar (`LV_EVENT_RELEASED`), igual que el resto de MKS. Borrar la pantalla dentro del callback es el mismo patrón que usa MKS; tras el callback no se accede a la tarjeta. |
| Perilla / grupos | La perilla está activa por defecto (`gCfgItems.encoder_enable = true`). Corregidos 2 fallos (P4, P5). |
| Objetos LVGL | Máx. 10 en el Dashboard (+4 con diálogo). Todo se libera al borrar la pantalla; 300 ciclos sin fugas. |
| Datos periódicos | `GUI_RefreshPage` → `lv_temp_refr()` 1×/s; el snapshot se compara y solo se redibuja lo que cambia. |

## 3. Seguridad durante la impresión
**Qué puede hacer el código ARQUIS sobre la máquina:** nada. Búsqueda automática en `arquis_*` de `queue`, `inject`, `process_subcommands`, `gcode.`, `thermalManager.set*`, `planner.`, `stepper.`, `quickstop`, `kill`, `settings.`, `card.start/pause/stop/abort/mount/release/open/close`, `print_job_timer.start/stop/pause/reset`, asignaciones a `marlin_state`/`print_state`, `WRITE/OUT_WRITE`, M-codes: **0 coincidencias** (solo comentarios y la *comparación* `marlin_state == MF_KILLED`).
Escrituras a variables ajenas: solo `uiCfg.curTempType` y `flash_preview_begin/default_preview_flg`, las mismas banderas de interfaz que ya cambia la UI original.
Las funciones `lv_draw_*` de destino se revisaron una a una: `lv_draw_move_motor`, `preHeat`, `fan`, `tool`, `set`, `media_select` no ejecutan comandos al dibujarse. **Excepción:** `lv_draw_print_file()` ejecuta `card.mount()` (`cardreader.cpp:448`), que cierra el directorio raíz y reinicia la SD.
| Riesgo | Antes | Ahora |
|---|---|---|
| Abrir el explorador (card.mount) con una impresión en curso | Evitado solo por una lectura de estado al tocar | **Triple protección**: lectura de Marlin al tocar, la tarjeta cambia de función, y `arq_nav_go()` redirige a la supervisión si hay trabajo |
| **Mover** durante la impresión (desplazar ejes) | Accesible | **Deshabilitado** (gris, sin toque ni foco) |
| **Ajustes** durante la impresión (contiene "apagar motores" = M84) | Accesible | **Deshabilitado** |
| Herram. (nivelación inyecta G-code, cambio de filamento) | Oculto | Oculto + bloqueado también en el manejador |
| **Temp.** / tarjetas Hotend, Cama, Ventiladores | Accesibles | Siguen accesibles: abren la misma pantalla que la UI original ofrece durante la impresión; modificar una temperatura es una acción explícita en esa pantalla, nunca por navegar. |
Botón "Inicio" (Etapa 3): debe llamar **solo** a `lv_draw_ready_print()` (cambio de pantalla).

## 4. Dashboard — estados y datos reales
Los 7 estados se verifican con la lógica real (43 pruebas) y se renderizan en el simulador (`docs/arquis/ui/dash_1…7`, `dash_5b`). Los datos provienen de las funciones de Marlin de la tabla del informe anterior.
| Estado pedido | Cómo se determina |
|---|---|
| Lista (reposo) | Sin trabajo, calentadores apagados y ≤ 50 °C |
| Calentando | Algún calentador activo por debajo de objetivo − histéresis |
| Temp. alcanzada | Calentadores activos dentro de ± histéresis (3 °C) |
| Enfriando | Calentador apagado > 50 °C, o por encima del objetivo + histéresis (punto azul) |
| Imprimiendo | `printingIsActive()` |
| Pausada | `printingIsPaused()` |
| Error | `IsStopped()` o `MF_KILLED` |
**Pregunta abierta:** pides "Reposo" **y** "Lista". Hoy son un solo estado ("Lista"). Propuesta con dato real: **Reposo** = sin tarjeta SD montada (`!card.isMounted()`), **Lista** = con tarjeta y todo apagado. ¿Apruebas?
Ventiladores: el porcentaje es el **comandado** por Marlin (`fanSpeedPercent`), igual que la UI original; no hay RPM. En tu configuración solo existen FAN0 (capa) y FAN1 (Aux 1); no hay `E0_AUTO_FAN_PIN`, por lo que "Disipador" no se muestra.

## 5. Problemas encontrados
### Críticos
- **C1 (corregido)** — `card.mount()` alcanzable desde el Dashboard durante una impresión (ver §3).
- **C2 (corregido)** — "Mover" y "Ajustes" (con M84) accesibles con impresión activa.
- **C3 (PENDIENTE, condiciona la Etapa 3)** — La lógica de **fin de impresión** de MKS vive en `setProBarRate()` y solo se ejecuta si `disp_state == PRINTING_UI` (draw_printing.cpp:~311-360): detener el contador (`stop_print_time`), cerrar la pantalla, mostrar el diálogo de fin y el **apagado automático** (`M1001`/`M81` si `finish_power_off`). Si el usuario está en el Dashboard cuando termina la impresión, **nada de eso ocurre**: el contador sigue, no hay diálogo y no se apaga. El botón "Inicio" lo convierte en un caso real. Solución propuesta (Etapa 3): extraer esa rama a una función `lv_print_finished()` y llamarla también desde el refresco del Dashboard cuando `marlin_state == MF_SD_COMPLETE` con trabajo activo. Mínima y sin alterar la ruta original.
### Medios (todos corregidos salvo P6, P7)
- P4: el anillo de foco azul salía **siempre** en la primera tarjeta con uso táctil → ahora solo aparece al mover la perilla.
- P5: al pasar de reposo a imprimiendo las tarjetas nuevas quedaban **fuera del grupo de la perilla** → se re-registran.
- P3: un diálogo en la capa superior sobreviviría a un cambio de pantalla → se cierra en `arq_dashboard_clear()`.
- P6 (riesgo): `printingIsPaused()` incluye `IS_SD_PAUSED()` = *archivo abierto y sin imprimir*. Si alguna ruta dejara un archivo abierto (no encontré ninguna), el Dashboard mostraría "Pausada". Es la definición de Marlin.
- P7 (Etapa 3): `lv_draw_printing()` elige el icono pausa/reanudar con `uiCfg.print_state`; si la impresión se inició desde un host, valdrá IDLE y mostraría "reanudar" aunque esté imprimiendo.
### Menores
- Fuente `num20` sin uso eliminada (−1,5 KB). `TEMP_HYSTERESIS` con valor por defecto. Comprobaciones de `nullptr` en componentes (ver nota).
- **Nota de memoria:** LVGL 6.1.1 **no tolera quedarse sin heap** (falla dentro de `lv_obj_create`/`lv_obj_del`, lo comprobé). La defensa es el margen: peor caso (Dashboard con impresión + diálogo + perilla) = 6,98 KB usados, **9,4 KB libres de 16 KB**.

## 6. Pruebas
**Simulador (LVGL real, ARM 32 bits, heap 16 KB, búfer 480×14):** pasan todas.
- 300 ciclos crear/destruir (7 estados), 200 idas y vueltas con otra pantalla, 3 000 refrescos, 300 diálogos: heap final = base, sin fragmentación.
- Seguridad: con trabajo activo "Mover" y "Ajustes" no reciben toques; al terminar vuelven a funcionar.
- Táctil: 8 accesos con id correcto; velo del diálogo bloquea toques; confirmar y "Seguir" correctos.
- Perilla: 8 tarjetas (5 con trabajo activo), foco inicial invisible, anillo al mover, 100 ciclos sin fugas.
- Redibujo: snapshot idéntico → nada; un cambio → 2 zonas.
**Lógica (nativo): 43/43.** **Compilación ARM con GCC 9.2.1 de PlatformIO:** sin advertencias propias.
**No ejecutado:** compilación de Marlin, ejecución en TS35, táctil/perilla reales, impresión.

## 7. Compilación — qué se intentó y qué falta
Para compilar se abrieron (con tu aprobación) los hosts de PlatformIO/GitHub en el entorno. Resultado: `ststm32@12.1.1` ✔, `toolchain-gccarmnoneeabi 1.90201` (GCC 9.2.1) ✔, `framework-cmsis` ✔, **core Arduino STM32 ✘** (`github.com/rhapsodyv/Arduino_Core_STM32/archive/39f37d6d6a.zip` → 403; adjuntar ese repositorio fue denegado y no busqué otra vía). Sin ese core Marlin no compila aquí. Los hosts añadidos al entorno `Default` puedes quitarlos en Ajustes del entorno.
### Instrucciones para compilar en tu PC (Windows, VS Code + PlatformIO)
```bash
cd F:\Arquis-Firmware-XQ1
git fetch origin
git checkout claude/nice-gauss-y90u12
git pull
pio run -e mks_robin_nano_v3_1_usb_flash_drive_msc 2>&1 | tee build.log
python herramientas\arquis_ui\auditar_flash.py
```
1. Debe terminar en `[SUCCESS]` y mostrar `RAM:` / `Flash:`; el binario es `.pio\build\mks_robin_nano_v3_1_usb_flash_drive_msc\Robin_nano_v3.bin`.
2. `auditar_flash.py` imprime Flash/RAM reales, margen libre, los 40 símbolos mayores y el reparto por módulo (LVGL, ARQUIS, idiomas, WiFi...). **Mándame esa salida y `build.log`.**
3. Para medir el coste exacto de ARQUIS UI: compila una vez más con `//#define ARQUIS_UI` comentado en `Configuration.h` y compara.
4. Si hay errores, pega las últimas 40 líneas de `build.log`.
Prueba en la impresora (primera vez): SD con `Robin_nano_v3.bin` en la raíz y la carpeta `assets` copiada de **`sd_assets` del repo** (no la del build). Con **calentadores apagados, cama fría, sin filamento**. Comprobar: arranque con logo nuevo, Dashboard, que los toques abren Mover/Temp./Herram./Ajustes/Imprimir, que la perilla navega, y que los valores de temperatura y ventilador coinciden con la pantalla original (`M105`, `M106 S128`).

## 8. Flash y RAM
**Código ARQUIS (compilado con GCC 9.2.1, Cortex-M4, -Os):**
| Parte | Flash |
|---|---|
| Código (tema, componentes, lógica, vista, datos, nav, dashboard) | 8,5 KB |
| Fuentes Inter (`text16` 11,2 KB + `num36` 4,0 KB) | 15,2 KB |
| Iconos y logo **que el Dashboard referencia** | 8,2 KB |
| **Total realista** | **≈ 31,9 KB** |
| Cota superior (todos los iconos, aunque no se usen) | 42,8 KB |
RAM estática: 3,0 KB (2,6 KB de estilos). Heap LVGL: Dashboard 3,9 KB; con diálogo 5,4 KB; peor caso total 7,0 KB de 16 KB.
**Referencia anterior (sin ARQUIS UI, GCC 13, sin verificar en tu PC):** Flash 381 864 B (80,4 %), RAM 75 232 B (57,4 %) sobre 475 136 B / 131 072 B.
**Estimación con ARQUIS UI:** Flash ≈ 414 KB → **87 %, ≈ 61 KB libres** (peor caso 89,5 %, ≈ 49 KB libres). **Es una estimación hasta medir con tu compilación.**

## 9. Optimizaciones (ordenadas por beneficio/riesgo; ninguna aplicada sin tu aprobación salvo `num20`)
| # | Optimización | Ahorro Flash | Riesgo / dependencias verificadas |
|---|---|---|---|
| 1 | **Idiomas MKS**: se compilan 7 idiomas (en, es, fr, it, ru, s_cn, t_cn). Medido compilando `tft_multi_language.cpp` para Cortex-M4: **44,2 KB** (cadenas 38,8 KB + código ~5 KB). Conservar solo en+es | **≈ 29–30 KB** | Requiere un `#if` en `tft_multi_language.*` y `draw_language.cpp`; el idioma por defecto está en `gCfgItems`. Decisión de producto (futuras impresoras). |
| 2 | **Fuente por defecto de LVGL** `lv_font_roboto_22` (**22,2 KB**) + `unscii_8` (1,2 KB): solo existen porque `LV_FONT_DEFAULT` apunta a ellas en el `lv_conf.h` de la librería. Marlin no las referencia directamente (verificado con grep). Apuntar `LV_FONT_DEFAULT` a `arq_font_text16` y desactivar ambas | **≈ 23 KB** | Exige parchear `lv_conf.h` de la librería descargada desde un `extra_scripts` de PlatformIO (como ya hace `download_mks_assets.py`). Probar en tu PC. |
| 3 | `MKS_WIFI_MODULE` (activo en `Configuration.h:3191`; `wifi_module.cpp` 2 090 líneas + `wifi_upload.cpp` 688 + QRCode) | **no medido** (esperable decenas de KB) | Solo si la impresora no lleva módulo WiFi. `auditar_flash.py` lo medirá. |
| 4 | Fuente `num20` sin uso | 1,5 KB | **Aplicada** (sin dependencias: 0 usos) |
| 5 | Recortar `text16` a los glifos del español (de Latin-1 completo) | ≈ 3,5 KB | Los nombres de archivo con otros acentos (ç, ä...) dejarían de verse. No recomendado aún. |
| 6 | Mover los iconos ARQUIS a la Flash SPI | ≤ 8 KB | **No recomendado:** el decodificador de LVGL carga cada icono alfa en el heap de 16 KB, `pic_manager` limita a 100 imágenes (`PIC_MAX_CN`) y 9 KB/imagen, y exige actualizar la lista `assets[]`. Costo/beneficio negativo. |
| 7 | Imágenes de Makerbase | 0 en Flash MCU | Ya viven en la Flash SPI (16 MB): `bmp_*.bin`, logo, fuente china `gb2312_puhui32`. No ocupan la Flash del MCU. |
| 8 | Funciones sin uso | — | `-Wl,--gc-sections` ya descarta iconos/funciones no referenciados; `auditar_flash.py` listará lo que quede. |
Margen tras 1+2: **≈ 115 KB libres (≈ 76 %)**.

## 10. Cambios recomendados / pendientes de tu decisión
1. Aprobar o no los estados **Reposo** vs **Lista** (§4).
2. Medir con tu compilación antes de decidir optimizaciones 1–3.
3. Etapa 3 deberá incluir (C3): hook de fin de impresión desde el Dashboard; (P7) icono pausa/reanudar coherente; botón "Inicio" = solo `lv_draw_ready_print()`; cierre de diálogos en cada `lv_clear_*`.
