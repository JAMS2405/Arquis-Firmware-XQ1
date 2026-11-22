# G-code de inicio y fin universales (firmware Arquis)

Archivos listos para copiar y pegar:
* `docs/arquis/gcode/inicio.gcode`
* `docs/arquis/gcode/fin.gcode`

Sirven para **cualquier impresora con este firmware** (XQ1, Aquila D1…) y **cualquier laminador**, porque:
* **No usan variables del laminador** (ni `{...}` ni `[...]`), así que no hay que adaptar la sintaxis. Tampoco llevan comentarios con esos símbolos, porque el laminador los intentaría sustituir.
* **No llevan medidas de cama fijas.** La purga va en la esquina delantera izquierda (X10 Y10, 80 mm) y el aparcado usa `G27`, que calcula el punto según el tamaño de cada máquina.
* **No llevan temperaturas.** Cura, PrusaSlicer, SuperSlicer y OrcaSlicer las insertan solos antes del G-code de inicio (cama primero, luego hotend). Si el laminador no lo hace (p. ej. Simplify3D), añade al principio del inicio, con su variable:
  ```
  M140 S<temp cama capa 1>
  M190 S<temp cama capa 1>
  M104 S<temp hotend capa 1>
  M109 S<temp hotend capa 1>
  ```
  Variables habituales: Cura `{material_bed_temperature_layer_0}` / `{material_print_temperature_layer_0}`; Prusa/Super `[first_layer_bed_temperature]` / `[first_layer_temperature]`; Orca `[bed_temperature_initial_layer_single]` / `[nozzle_temperature_initial_layer]`; Simplify3D `[bed0_temperature]` / `[extruder0_temperature]`.

## Qué hace el inicio
1. Fija mm, coordenadas absolutas, velocidad y flujo al 100 % y apaga el ventilador.
2. `G28` (Home). Con `ENABLE_LEVELING_AFTER_G28` el firmware **activa solo la malla 5×5** guardada. Si no hay malla válida, no la activa y sigue sin problemas.
3. Línea de purga de 80 mm en X10 Y10 y sube a Z2.

**Nunca pongas `G29`:** con `PROBE_MANUALLY` inicia una medición manual sin interfaz y la impresión se queda esperando.

## Qué hace el fin
Retrae 2 mm, sube 20 mm y aparca (`G27 P2`), apaga hotend, cama y ventilador, y suelta X, Y y el extrusor (`M84 X Y E`). Z queda sujeto a propósito.

## Dónde pegarlos
* **Cura:** Preferencias → Impresoras → Ajustes de máquina → *Start G-code* / *End G-code*.
* **PrusaSlicer / SuperSlicer:** Ajustes de impresora → G-code personalizado → *Start G-code* / *End G-code*.
* **OrcaSlicer:** Ajustes de impresora → G-code de máquina → *Machine start G-code* / *Machine end G-code*.
* **IdeaMaker:** Impresora → Ajustes → G-code (inicio y fin).
* **Simplify3D:** Proceso → Scripts → *Starting Script* / *Ending Script* (añade las temperaturas, ver arriba).

## Requisitos del laminador
* Extrusión **absoluta** (la predeterminada de Cura y Prusa). Si el laminador usa extrusión relativa (`M83`), el inicio sigue valiendo porque termina con `G92 E0`.
* El origen de la cama debe ser la esquina delantera izquierda (0,0), como en el XQ1 y la Aquila D1.

## Estado
Escrito y revisado contra el firmware (`G27`, `ENABLE_LEVELING_AFTER_G28`, `PROBE_MANUALLY`). **No probado aún en la impresora:** haz una primera impresión vigilando la purga y que la malla se active (`M420 V` en consola).
