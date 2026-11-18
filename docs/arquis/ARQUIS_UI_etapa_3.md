# ARQUIS UI — Etapa 3: supervisión de la impresión

Estado: **compila en el simulador y pasa sus pruebas; NO está validada en el firmware completo ni en la pantalla real.**
(Ver "Qué está y qué no está comprobado" al final.)

## Qué se hizo
Pantalla de supervisión propia (sustituye a la de MKS cuando `ARQUIS_UI` está activo; con `ARQUIS_UI` desactivado el código original
queda idéntico, verificado línea a línea contra el original).

| Elemento | Datos |
|---|---|
| Cabecera | marca + nombre, **estado** (Imprimiendo / Pausada / Pausando / Reanudando / Preparando / Interrumpida / Error), botón **Inicio** |
| Tarjeta de avance | nombre del archivo (recortado con "…"), barra, **% grande**, tiempo transcurrido y restante |
| Lista de datos | Hotend, Cama (con punto de estado: gris/ámbar/verde/azul), Ventilador, Altura Z |
| Miniatura | zona x 2..202, y 36..236 **reservada**: la miniatura y su relleno por avance siguen siendo los de la interfaz original |
| Botones | **Pausar/Reanudar**, **Cancelar** (siempre con confirmación), **Objetos** (cancelar objeto), **Opciones** |
| Diálogos | Cancelar impresión (peligro, 2 botones) · Impresión finalizada (informativo, tiempo total) |

Todos los valores son reales (Marlin / `thermalManager` / `card`). El tiempo restante usa la fórmula de Marlin y se muestra `--:--`
si no hay datos suficientes (nunca se inventa).

## Cambios de comportamiento (todos pedidos o derivados de la auditoría)
1. **Botón Inicio**: SOLO cambia de pantalla (`arq_print_clear()` + `lv_draw_ready_print()`). La impresión sigue su curso.
2. **P7 (icono pausa/reanudar incoherente)**: el botón se deriva del estado real (`uiCfg.print_state`): Pausar → Pausando (deshabilitado) →
   Reanudar → Reanudando (deshabilitado). Un trabajo iniciado desde un host (sin controlar por la interfaz) no muestra pausa.
3. **C3 (fin de impresión solo detectado en la pantalla de supervisión)**: `arq_print_poll()` detecta el fin desde cualquier pantalla
   (`marlin_state == MF_SD_COMPLETE` dura solo hasta que Marlin encola M1001, por eso se anota en cuanto aparece) y, si la pantalla
   activa no es el Dashboard ni la supervisión, **aplaza** el aviso hasta volver a una de ellas.
4. **Porcentaje**: usa el mismo cálculo que la barra original (descuenta la miniatura incrustada del G-code), para que coincida con el
   relleno de la miniatura. Para trabajos que no inició esta interfaz usa el porcentaje de Marlin. Sin tamaño de archivo no se calcula
   (evita una división por cero que ya existía en `setProBarRate`).
5. Con un diálogo abierto no se redibuja la miniatura; al cerrarlo ("Seguir imprimiendo") se pide su repintado igual que `draw_return_ui()`.
6. Mientras se carga la miniatura (`gcode_preview_over`) todos los toques se ignoran, como en la pantalla original.

## Seguridad: qué llamadas hacen cosas en la máquina
No hay lógica de impresión propia. Las ÚNICAS llamadas con efecto en la máquina están en `arquis_print.cpp` y son **las mismas, en el
mismo orden, que las de la pantalla original** (`draw_printing.cpp` y `draw_dialog.cpp`):

| Acción | Llamadas | Equivalente original |
|---|---|---|
| Pausar | `card.pauseSDPrint(); stop_print_time(); uiCfg.print_state = PAUSING;` | `ID_PAUSE` |
| Reanudar | `uiCfg.print_state = RESUMING;` (y `REPRINTED` tras corte de energía) | `ID_PAUSE` |
| Cancelar (tras confirmar) | `wait_for_heatup = false; stop_print_time();` → Dashboard → `uiCfg.print_state = IDLE; card.abortFilePrintSoon();` | OK de `DIALOG_TYPE_STOP` |
| Fin de impresión | `stop_print_time()`; si `finish_power_off`: `M1001` + `M81` | rama de fin en `setProBarRate` |

`printer_state_polling()` (sin tocar) sigue completando las pausas y reanudaciones. No se llama a calentadores, motores, planificador,
ni se encola G-code propio. Inicio, Objetos y Opciones solo navegan.

## Memoria y Flash
- Heap LVGL (16 KB): supervisión = 9 objetos, **pico 3,5 KB**; peor caso (supervisión + diálogo + perilla) **7,2 KB usados, 9,2 KB libres**.
- 300 ciclos crear/destruir y 200 aperturas de diálogo: sin fugas.
- Flash estimada (compilación ARM -Os de los archivos nuevos): ≈ 6 KB (código 4,4 KB + 2 iconos nuevos 1,6 KB). Quedaban 71 984 B libres → ≈ 65 KB.
- RAM estática nueva: ≈ 0,5 KB.

## Pruebas
- Simulador con LVGL real en ARM-32 (qemu): **todas pasan**, incluyendo: ningún objeto invade la zona de la miniatura; refrescos que no
  tocan sus píxeles; id correcto de cada botón; botones deshabilitados en "preparando"/"pausando"/trabajo no controlado; diálogo
  informativo; sin fugas.
- Lógica pura: 43 de 43.
- Comprobación de firmas contra cabeceras de prueba (no es una compilación de Marlin): OK.
- Capturas: `docs/arquis/ui/print_*_x2.png`.

## Qué está y qué no está comprobado
| Comprobado | NO comprobado (necesita tu PC / tu impresora) |
|---|---|
| Vista en LVGL real (simulador) | **Compilación completa de Marlin** con estos archivos |
| Lógica y memoria | Pantalla TS35 real: colores, miniatura, táctil |
| Firmas de las llamadas a Marlin (contra cabeceras de prueba) | Pausa / reanudar / cancelar / fin de impresión reales |
| Código original intacto con `ARQUIS_UI` desactivado | Detección del fin de impresión desde el Dashboard (depende del momento exacto de `MF_SD_COMPLETE`) |

## Prueba segura en la impresora (sin calentar ni mover nada)
1. Genera un G-code que no calienta ni mueve: en PowerShell
   `"; prueba ARQUIS UI" | Set-Content prueba_ui.gcode; 1..600 | ForEach-Object { "G4 S1" } | Add-Content prueba_ui.gcode`
   (10 minutos de pausas de 1 s) y cópialo a la SD.
2. Imprimir → debe verse la supervisión con miniatura por defecto, % subiendo y tiempo.
3. **Pausar**: ATENCIÓN, la pausa original mueve Z y el cabezal a la posición de pausa configurada; despeja la cama y haz Home antes.
4. Reanudar, Inicio (vuelve al Dashboard sin interrumpir; la tarjeta de impresión del Dashboard devuelve a la supervisión), Cancelar → confirmar.
5. Repite sin cancelar hasta el final para ver el aviso "Impresión finalizada" (también con la impresora en el Dashboard).

## Pendiente / riesgos conocidos
- **Opciones** abre la pantalla ORIGINAL de MKS (velocidad, flujo, ventilador, babystep, filamento, mover, extruir…). Mover y extruir siguen
  disponibles durante la impresión como en la interfaz original; se decidirá en la Etapa 4 (restyle de Opciones, Objetos, Temp.).
- Las pantallas de Temperatura, Ventiladores, Opciones y Cancelar objeto siguen con el aspecto MKS hasta la Etapa 4.
- Se quitaron de la supervisión los accesos directos a temperatura/ventilador/Z de la pantalla original: se llega a ellos desde
  Opciones (o Temp. del Dashboard).
- La perilla funciona en esta pantalla solo si el encoder está habilitado en los ajustes (igual que el Dashboard).

## Correcciones tras la primera prueba real (v12)
Prueba en la impresora: supervisión, pausa, avance y cancelación correctos. Dos fallos, ya corregidos en el código (pendientes de validar en la pantalla real):

1. **Miniatura sobre el diálogo.** `preview_fill_update()` pinta directo en la pantalla (fuera de LVGL) cada vez que cambia el porcentaje; con el diálogo abierto, sus filas aparecían encima. `setProBarRate()` ya no la llama mientras hay un diálogo (`arq_print_dialog_open()`); al cerrar con "Seguir imprimiendo" se repinta completa.
2. **Objetos → pantalla negra y reinicio.** La pantalla original "Cancelar objeto" creaba ~70 objetos LVGL (≈26,6 KB medidos en el simulador con LVGL real; heap = 16 KB). Nueva implementación con 6 objetos fijos (`arquis_objects_view.cpp`): cabecera con Atrás (2), mapa de la cama (1), fichas + estado (1) y botón de acción (1).
   - Pico de memoria 1,9 KB con 0, 3, 9 o 16 objetos; peor caso con diálogo abierto: 11 KB libres de 16 KB. 600 ciclos crear/destruir sin fugas.
   - Cancelar un objeto: botón → diálogo de confirmación "Cancelar objeto N" (Volver / Cancelar objeto). Restaurar: directo.
   - Mapa: verde = imprimiendo, gris = en espera, rojo con X = cancelado, anillo azul = seleccionado. Las posiciones se aprenden al imprimir cada objeto (antes de eso solo se elige con las fichas).
   - Limitación heredada: la interfaz muestra hasta 16 objetos; si el G-code define más, lo indica ("Se muestran 16 de N").
   - La perilla no recorre mapa y fichas (solo táctil). El botón "Quitar objeto" del menú Opciones sigue con el aspecto MKS hasta la Etapa 4.
3. "Hizo un home al cancelar": es `EVENT_GCODE_SD_ABORT "G28XY"` de Configuration_adv.h, no de la interfaz.

Capturas: `docs/arquis/ui/obj_*_x2.png`.
