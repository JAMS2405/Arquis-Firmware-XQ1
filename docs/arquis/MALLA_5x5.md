# ARQUIS XQ1 — Nivelación manual de cama 5×5 (sin sonda)

Estado: **implementado, probado en PC (lógica, interpolación real de Marlin, vistas LVGL) y compilado por el propietario.** **Probado en la impresora por el propietario (2026-11-22);** los resultados detallados de esas pruebas no se han trasladado a este repositorio. Los apartados §11 y §12 conservan la lista de qué se verificó en cada entorno.

## 1. Qué hace
Mide 25 puntos con el método de la hoja de papel desde la TS35 (menú **Herramientas → Malla 5x5**), genera una malla bilineal en Marlin, la guarda en la EEPROM y Marlin compensa Z en todos los movimientos XY (también al imprimir) sin tocar el G-code ni el slicer. No hay BLTouch, sonda ni sensor. No se usa nivelación con motores Dual Z.

## 2. Arquitectura
```
 TS35 (LVGL 6.1.1-MKS)
  arquis_mesh_view.cpp   Mapa 5x5 (1 objeto) + panel por fase. Solo LVGL.
  arquis_mesh_ui.cpp     Pantalla MESHLEVELING_UI: botones, diálogos, tarea LVGL (250 ms).
        │
  arquis_mesh.cpp        Controlador: G-code por la cola normal, lectura de la Z REAL de Marlin,
        │                comprobaciones de seguridad, ÚNICA escritura de la malla (arq_mesh_ctl_commit).
  arquis_mesh_logic.cpp  Lógica pura (sin Marlin ni LVGL): orden de puntos, límites de Z, sesión, validación, estadísticas.
        │
  Marlin: bedlevel (AUTO_BED_LEVELING_BILINEAR) · planner (compensación + fade) · settings (M500/M501 EEPROM)
```
* **No hay segundo motor de compensación**: la interpolación bilineal, la segmentación de movimientos, el fade y la EEPROM son los nativos de Marlin.
* `GUI_RefreshPage` llama a `arq_mesh_poll()`: la sesión avanza (y se **termina con seguridad**) aunque la pantalla no esté visible.
* Pantalla anterior de tornillos (`Nivelar cama`, 5 puntos) se conserva; ahora hace `G28 L0` para que la malla no interfiera al ajustar tornillos. **Se recomienda ajustar tornillos antes de medir la malla.**

### ¿Por qué no se usa el G29 manual de Marlin para medir?
Se auditó (`gcode/bedlevel/abl/G29.cpp`): (a) solo avanza, no permite repetir un punto; (b) al arrancar con otra rejilla hace `reset_bed_level()` y, si se cancela, la malla anterior no se recupera; (c) su primera orden baja a **Z=0** en el primer punto sin comprobar nada. El asistente recoge las 25 alturas **que el usuario confirma** y las entrega a Marlin en un solo paso atómico y validado. `G29` sigue disponible desde la consola; si hay un G29 manual en curso, el asistente se niega a empezar.

## 3. Geometría
Cama 300×300. `PROBING_MARGIN 15` → área de la malla X,Y = 15…285 mm, paso **67,5 mm**. Puntos `p = fila*5 + columna` (columna izquierda→derecha, fila delante→atrás). Orden de medición: serpentina por filas desde delante-izquierda (**el mismo orden que G29 manual**), puntos 1…25 en pantalla. Centro (referencia) = punto con índice 12 = (150,150).
`NOZZLE_TO_PROBE_OFFSET {0,0,0}`: la boquilla es el punto de medición, así que no hay desplazamiento respecto a las coordenadas de Marlin. (`MESH_INSET` no se usa: solo aplica a MBL/UBL.) Fuera del área medida (0–15 y 285–300 mm) Marlin mantiene la altura del borde (no extrapola).

## 4. Convención de signos (verificada en `planner.cpp`/`bbl.cpp`)
| Concepto | Definición |
|---|---|
| **Z absoluta** | Z de la máquina (sin compensación) cuando el papel roza. Mayor Z = boquilla más arriba. |
| **Corrección almacenada** (`z_values`) | `Z_abs(punto) − Z_abs(centro)`. El centro vale siempre 0,00. |
| **Desviación relativa** (lo que se muestra con la malla completa) | Igual que la corrección. |
| **Referencia de la interpolación** | El plano Z=0 de la máquina; la corrección se desvanece linealmente hasta 0 a la altura de *fade* (10 mm). |
| Marlin aplica | `Z_física = Z_ordenada + fade(Z) × corrección(x,y)` |
Positivo = en ese punto la cama está **más alta** que en el centro (la boquilla sube). Negativo = **más baja** (baja). Se aceptan ambos signos. **Por qué relativa al centro:** el Z-offset global (home offset, babystep) sigue siendo independiente y el *fade* no estira la pieza por un offset global. Consecuencia: **ajusta el Z-offset en el centro de la cama** como siempre. Desde Babystep ese ajuste ahora se guarda solo como `M206 Z` al salir de la pantalla (ver `CAMBIOS.md`, «Offset Z permanente»).

## 5. Procedimiento
1. Ajusta los 4 tornillos (Herramientas → Nivelar cama) y limpia la boquilla; retira objetos de la cama.
2. Herramientas → **Malla 5x5**. Elige la temperatura de la cama (−/+ de 5 °C; 0 = no calentar) → **Iniciar medición** → confirmar.
3. El asistente calienta la cama, espera 30 s (dilatación), hace `G28 L0` y va al punto 1 a Z=1,0 mm (después, a *última medición + 0,6 mm*, nunca por encima de 1,0).
4. Baja Z con ▼ hasta que la hoja roce con ligera resistencia. Pasos: **1,00 / 0,10 / 0,05 / 0,01 mm** (ver límites en §8). **Confirmar** registra la Z real de Marlin. Avanza solo al siguiente punto.
5. **Repetir**: toca en el mapa un punto ya medido (anillo azul) y pulsa *Repetir*; sin selección repite el anterior. Un punto en repetición cuenta como pendiente hasta que se confirme otra vez. En la revisión final también se puede repetir cualquier punto.
6. Con los 25 puntos: pantalla de revisión (colores azul = más baja, ámbar = más alta; máx, mín, rango, desviación típica) → **Guardar**: valida, activa la malla en Marlin y ejecuta `M500`. Si la EEPROM falla, la malla queda activa pero se avisa de que NO está guardada.
7. Cancelar (o salir con ◄) en cualquier momento: se descartan las mediciones y **se conserva la última malla válida**.

## 6. Pantalla principal (sin sesión)
Mapa con la malla guardada (valores y colores), estado *Malla activa / inactiva / Sin malla válida*, estadísticas, temperatura de calibración y botones **Iniciar medición, Activar/Desact., Guardar, Cargar**. Los valores del mapa son siempre mediciones; nunca una interpolación. Los puntos pendientes muestran su número de orden.
"Guardar" ejecuta `M500` (guarda **todos** los ajustes). "Cargar" ejecuta `M501` (lee todos los ajustes) tras confirmar; antes valida la EEPROM porque `settings.load()` reinicia todo a fábrica si la EEPROM no es válida.

## 7. Comandos G-code que usa el asistente
`G90`, `G28 L0` (Home sin activar la compensación), `G1 Z.. / X.. Y.. F..` (todas por la cola normal), y las funciones de Marlin `set_bed_leveling_enabled()`, `bedlevel.set_grid()` / `z_values` / `refresh_bed_level()` (solo en `arq_mesh_ctl_commit`), `settings.save()/validate()/load()` (= M500/M501). **No usa G29, M420 ni M421.** Desde consola siguen disponibles `M420 V` (ver malla), `M420 S1/S0`, `M420 Z<fade>`, `M500/M501/M502`, `G29 A` (abortar un G29 manual).

## 8. Seguridad
* No arranca con una impresión activa/pausada, Marlin detenido o un G29 manual en curso; la sesión se **aborta** si aparece alguna de estas condiciones, o si algo activa la compensación a mitad de medición. Con impresión en curso el abortado es "silencioso" (no toca Z, temperatura ni velocidad de la impresión).
* Homing obligatorio al empezar (`G28 L0`); se comprueba `all_axes_homed()`.
* Z: mínimo absoluto **−2,00 mm** (`ARQ_MESH_Z_MIN`), máximo 10 mm. Los **pasos gruesos (>0,10 mm) no pueden bajar de Z=0,50 mm** (cerca del papel solo pasos finos). Los puntos siguientes parten de última medición + 0,6 mm.
* El límite software **mínimo de Z** de Marlin se baja a −2,00 mm **solo durante la sesión** (necesario para Z negativa) y se recalcula al terminar, cancelar o fallar. (`SET_SOFT_ENDSTOP_LOOSE` por sí solo no basta: los `G1` los recorta `apply_motion_limits()`, que ignora ese indicador, y la boquilla no bajaba de Z=0.)
* Límite físico: si el final de carrera Z queda a más de 2 mm por encima de la cama (la boquilla sigue alta en Z=0 y también en −2), hay que bajar el final de carrera Z mecánicamente.
* No se acepta una confirmación mientras la máquina se mueve ni si la Z real difiere de la mostrada (>0,005 mm): se resincroniza y hay que confirmar de nuevo.
* La malla solo se entrega si están los 25 puntos, son finitos y |corrección| ≤ 2,00 mm. Si algo falla tras escribir, se restaura la malla anterior.
* Al terminar/cancelar: Z a 10 mm, se restaura la temperatura anterior de la cama y el estado anterior de la compensación.
* No se activa una malla inexistente (Marlin lo impide: `set_bed_leveling_enabled(true)` exige rejilla definida) ni con otra geometría (se comprueba que sea la 5×5 esperada).

## 9. EEPROM
Nativa de Marlin (I2C 4 KB): `M500` guarda rejilla + 25 valores; `M501` los recupera. **Marlin 2.1 con bilinear NO guarda si la compensación estaba activa**: tras reiniciar queda **desactivada**. Por eso se activa `ENABLE_LEVELING_AFTER_G28`.
> ⚠ **Primera vez tras flashear:** cambiar la malla de 3×3 a 5×5 cambia el tamaño de los datos de la EEPROM; Marlin la considera incompatible, la descarta y arranca con los valores de fábrica de `Configuration.h` (pasos, PID, offsets, etc.). Antes de flashear guarda `M503`; después de flashear reaplica tus ajustes y haz `M500`, y mide la malla.

## 10. Activación durante la impresión
`ENABLE_LEVELING_AFTER_G28` (nativo): tras **cada G28** la compensación se activa si hay una malla válida. Cualquier G-code inicial con `G28` (Cura) la activa; si el usuario hizo Home antes desde la pantalla, ya está activa. Si no hay malla válida no se activa. `G28 L0` homea sin activarla. No se ejecuta nunca un G29 (no hay sonda). **El G-code inicial del slicer:**
```
G28            ; home (activa la malla guardada si es válida)
M420 S1        ; opcional y inocuo (M420 V para verlo en consola); si ya está activa no hace nada
```
**No pongas `G29` en el G-code inicial** (con PROBE_MANUALLY inicia una medición manual sin interfaz). Los perfiles de Cura no se modifican. La tarjeta *Nivel. auto* (que inyectaba el G29 guardado) ahora abre este asistente.
Cambiar la compensación (Activar/Desact.) está bloqueado durante una impresión.

## 11. Cambios de configuración (`Marlin/Configuration.h`)
| Opción | Antes | Ahora |
|---|---|---|
| `GRID_MAX_POINTS_X/Y` | 3 | **5** |
| `NOZZLE_TO_PROBE_OFFSET` | {10,10,0} | **{0,0,0}** |
| `PROBING_MARGIN` | 10 | **15** |
| `MANUAL_PROBE_START_Z` | 0,2 | **1,0** |
| `ENABLE_LEVELING_AFTER_G28` | desactivado | **activado** |
Sin cambios: `PROBE_MANUALLY`, `AUTO_BED_LEVELING_BILINEAR`, `EEPROM_SETTINGS`, fade 10 mm, `SEGMENT_LEVELED_MOVES` (5 mm), `LCD_BED_LEVELING` desactivado. Código: 8 archivos nuevos `arquis_mesh*.{h,cpp}`; modificados `arquis_tools.cpp` (tarjeta, `G28L0` en tornillos, fila de diagnóstico) y `draw_ui.cpp` (poll, dibujo y limpieza de `MESHLEVELING_UI`, que ya existía vacío en el enum).

## 12. Limitaciones y pruebas pendientes
* Sin sonda: la precisión depende del tacto con el papel (±0,02–0,05 mm). Solo se mide a la temperatura elegida; una cama que cambia de forma en frío/caliente necesita medirse caliente.
* La malla cubre 15–285 mm; fuera, altura del borde. No compensa torsión del eje X ni el Z-offset (se ajusta aparte, en el centro).
* Recuperación: si el asistente falla/cancela, la malla anterior sigue; si no, `M502`+`M500` restablecen fábrica y `M420 S0` desactiva la compensación; `G29 A` aborta un G29 manual.
* **Nada de esto se ha probado en la impresora.** Pendiente físico: homing + `G28 L0`, movimientos a los 25 puntos, Z negativa con límites aflojados, tacto de los pasos, `M500`/reinicio/`M501`, compensación real al imprimir (primera capa), impresión desde SD con G28 inicial, pausa/reanudar con malla, cancelación a mitad, comportamiento del heap en la TS35 real y el borrado de la EEPROM de la primera vez.
