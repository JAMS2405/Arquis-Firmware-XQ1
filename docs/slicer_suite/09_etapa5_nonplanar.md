# Etapa 5 — ARQUIS Non-Planar Engine (experimental)

Autorizada por el propietario el 2026-11-19 (se salta la Etapa 4, aplazada). Rama `claude/dreamy-allen-gc05as`; `main`, `platformio.ini`, `ini/`, el firmware y el planificador **no se han tocado**.

> **Leyenda de pruebas** (la de `07_etapa3_firmware.md`): **[CÁLCULO]** verificación numérica en el PC · **[MODELO]** G-code leído con el modelo de Marlin del repositorio (`marlin_model.py`) · **[LECTURA]** comprobado en el código fuente · **[FÍSICA]** en la impresora.
> **Hoy no existe ninguna prueba [FÍSICA] de esta etapa.** Nada de lo que sigue demuestra que la impresora pueda ejecutar la pieza.

## 1. Qué es y qué no es

Es una línea de desarrollo **independiente**: genera trayectorias en las que Z cambia mientras la boquilla se mueve en XY (capas no horizontales) y comprueba que sean ejecutables con movimientos `G1 X Y Z` convencionales de Marlin 2.1.2.10, **sin modificar el firmware**. No es Vase Mode ni Spiralize Outer Contour de Cura.

| Sí hace | No hace (limitaciones) |
|---|---|
| Genera **una pieza de prueba pequeña** (30 × 20 mm): 5 capas planas y 8 capas onduladas (onda senoidal de 0,6 mm de amplitud y 20 mm de longitud) | **No lee STL ni la escena de Cura.** La superficie es un campo de alturas analítico |
| Calcula pendiente, espesor de capa, velocidad, aceleración de Z, extrusión y colisiones con un **modelo** del cabezal | **No está conectado a Cura**: ni el plugin ni los scripts lo importan (una prueba lo exige) |
| Genera una **vista previa** (SVG) y un código; solo exporta con ese código devuelto | **No imprime una pieza completa**; no transforma G-code existente |
| Verifica el G-code **ya generado** leyendo su texto, de forma independiente | No cubre soportes, perímetros, relleno variable, retracciones entre islas ni ajustes de material |
| Se ejecuta solo con `--enable-experimental` | Las dimensiones del cabezal **no están medidas** (ver §4) |

## 2. Arquitectura

Un solo módulo, `cura/plugins/ARQUISPlugin/engines/nonplanar_engine.py` (Python puro, sin Cura), con una función por etapa en el orden del PROMPT MAESTRO:

| # | Etapa | Función | Qué hace |
|---|---|---|---|
| 1 | Análisis geométrico | `analyze_geometry` | Pendiente y espesor de capa en una rejilla de 1 mm sobre la huella |
| 2 | Zonas compatibles | `select_zones` | Marca las zonas con pendiente ≤ límite y espesor admisible. **Hoy solo informa**: si alguna zona es incompatible, el plan falla (`ZONA`) en lugar de imprimir con huecos |
| 3 | Trayectorias | `generate_trajectories` | Base plana (líneas alternando X/Y) y capas onduladas en líneas X; entre capas, un paso en Z sin extrusión |
| 4 | Altura Z | `adjust_z` | Comprueba la desviación (tope duro 2,0 mm), el rango 0–450 mm y la altura de desvanecimiento de la nivelación; **no altera Z en silencio** |
| 5 | Velocidad | `adjust_speed` | Mínimo entre la velocidad del tipo, el tope del eje Z (90 %) y el caudal máximo; limita la aceleración de Z |
| 6 | Extrusión | `adjust_extrusion` | E = sección × longitud 3D × flujo / sección del filamento (sección «rectángulo con extremos redondos») |
| 7 | Límites | `verify_limits` | Cama y margen, pendiente, espesor, velocidad por eje, caudal, velocidad del extrusor |
| 8 | Colisiones | `detect_collisions` | Mapa de alturas de lo ya impreso; la punta no puede quedar bajo material y ningún volumen del cabezal puede contener material más alto |
| 9 | Vista previa | `render_preview` | SVG: vista superior por pendiente, perfil lateral por capas, resumen y hallazgos |
| 10 | Exportación | `export_gcode` + `verify_gcode` | Solo con todas las barreras (§3); el texto resultante se vuelve a verificar |

Herramienta: `cura/tools/nonplanar_demo.py`. Pruebas: `cura/tests/test_nonplanar.py` (17).

## 3. Barreras de seguridad (todas obligatorias)

1. **Desactivado por defecto:** `NPConfig.enabled = False`; `build_plan` lanza `NotEnabled`; la herramienta no hace nada sin `--enable-experimental`.
2. **Vista previa con código:** la exportación exige el código (12 caracteres) que imprime la vista previa; el código cambia si cambia **cualquier** parámetro o trayectoria.
3. **Cabezal confirmado:** sin `head.confirmed = true` no se exporta.
4. **Temperaturas explícitas:** `bed_temp` y `nozzle_temp` vienen vacías; el motor no asume el material.
5. **Cero hallazgos `ERROR`** (límites, colisiones, pendiente, espesor, caudal, desviación).
6. **Solo órdenes ya presentes en este firmware:** `G0 G1 G28 G29 G90 G92 M82 M84 M104 M106 M107 M109 M140 M190 M420`; sin arcos, sin modo relativo, sin cambiar aceleraciones ni jerk, sin guardar en EEPROM. Las líneas de inicio personalizadas se validan contra esa lista.
7. **Verificación independiente del texto** (`verify_gcode`): marca experimental, órdenes permitidas, límites de la máquina, velocidad por eje ≤ firmware, E sin retroceso, y que E total y número de movimientos coincidan con el plan.
8. **El archivo lleva en su primera línea** `;ARQUIS_NONPLANAR EXPERIMENTAL`.

## 4. Lo que impone tu firmware (leído en `Marlin/Configuration.h`) [LECTURA]

| Dato | Valor | Consecuencia |
|---|---|---|
| `DEFAULT_MAX_FEEDRATE` Z | **5 mm/s** | Una pendiente de ~10° a 20 mm/s pide 3,5 mm/s en Z; más pendiente obliga a **bajar la velocidad** (el motor la baja) |
| `DEFAULT_MAX_ACCELERATION` Z | **100 mm/s²** | Limita cuán rápido puede cambiar la pendiente; el motor mantiene la estimación ≤ 80 % del límite |
| Planificador | `block->millimeters = √(x²+y²+z²)` (`planner.cpp`) | La velocidad `F` se aplica a la longitud XYZ del movimiento |
| `AUTO_BED_LEVELING_BILINEAR`, fade 10 mm | activo | La compensación se desvanece a 10 mm; el archivo **no activa la malla** (usa `custom_start_lines`, p. ej. `M420 S1`) |
| `JUNCTION_DEVIATION` (no `CLASSIC_JERK`) | activo | El jerk de Z no interviene; el planificador frena además en cada esquina |
| `LIN_ADVANCE` | activo | **No verificado** cómo se comporta con movimientos con Z variable |

Una prueba compara estas constantes del motor con `Configuration.h`: si cambias el firmware, la prueba falla.

## 5. Resultado del ejemplo por defecto [CÁLCULO / MODELO]

| Dato | Valor |
|---|---|
| Pieza | 30 × 20 mm, Z máx 3,2 mm, 13 capas (5 planas + 8 onduladas) |
| Pendiente máxima | 10,7° (límite efectivo 15°) |
| Espesor de capa | 0,125–0,275 mm (admisible 0,08–0,30 mm) |
| Velocidad máxima en el eje Z | 3,66 mm/s (firmware: 5) |
| Aceleración de Z estimada | 80 mm/s² (límite 100; es el tope de diseño del 80 %: **el motor ya redujo velocidad para llegar ahí**) |
| Velocidad mínima | 16,2 mm/s |
| Caudal máximo | 2,26 mm³/s |
| Segmentos / archivo | 19 350 movimientos, ~0,9 MB |
| Tiempo | ~14 min (estimación sin aceleraciones; será mayor) |
| Pruebas | 17 en `test_nonplanar.py`; suite de Cura completa en verde |

Lo que verifican las pruebas: que el motor está desactivado por defecto y no conectado a Cura; que sus límites son los de `Configuration.h`; que el volumen extruido coincide con la geometría (±15 %); que los **errores se detectan** (desviación excesiva, pendiente, borde de cama, espesor, colisión del cabezal, límite del cono); que la exportación exige cada barrera; que el G-code resultante solo usa órdenes permitidas, no pasa de los límites y **el modelo de Marlin del repositorio lo ejecuta** (E físico = plan − retracción); y que `verify_gcode` detecta un texto manipulado.

Vista previa de ejemplo: `img/nonplanar_vista_previa.png`.

## 6. Qué NO está probado (riesgos)

- **Ninguna prueba física.** La seguridad frente a colisiones se comprueba contra un **modelo** del cabezal con valores **sin medir** (§7). Si tu bloque calefactor, tu ducto o tu cono son distintos, el resultado no sirve.
- La aceleración de Z se **estima** con velocidad constante por segmento; en esquinas es pesimista, pero **no se ha medido** si el eje Z real (791,74 pasos/mm, 100 mm/s²) sigue la onda sin perder pasos.
- **Compatibilidad de la nivelación:** el efecto de la malla bilineal sobre Z con movimientos segmentados **no se ha verificado** en la placa.
- **`LIN_ADVANCE`** con Z variable: sin verificar.
- El modelo de extrusión (sección «rectángulo con extremos redondos», filamento de 1,75 mm **sin medir**) es una aproximación; el espesor de capa varía ±0,075 mm, por lo que habrá variaciones de acabado.
- **Calidad:** no hay perímetros ni relleno variable; la pieza es una prueba de movimiento, no de calidad.
- El coste de la detección de colisiones crece con la pieza (O(muestras × celdas)): válido para este ejemplo, **no** para una impresión completa.

## 7. Cómo usarlo (cuando decidas probarlo)

```bash
python3 cura/tools/nonplanar_demo.py --init-config nonplanar.json
```
1. **Mide tu hotend** y edita `nonplanar.json`: `head.envelopes` (radio y altura sobre la punta del bloque calefactor y del ducto), `head.nozzle_clearance_deg` (pendiente máxima que tolera el cono), `filament_d`, y las temperaturas `bed_temp` y `nozzle_temp`. Cuando estén medidos, `head.confirmed: true`.
2. Si usas malla guardada, añade `"custom_start_lines": ["M420 S1"]` (comprueba que es lo que ya usas en tu inicio).
3. Genera la vista previa y revísala (ábrela en un navegador):
```bash
python3 cura/tools/nonplanar_demo.py --enable-experimental --config nonplanar.json --preview vista.svg
```
4. Solo si no hay `ERROR`, exporta con el código que imprimió:
```bash
python3 cura/tools/nonplanar_demo.py --enable-experimental --config nonplanar.json --export prueba_nonplanar.gcode --accept CODIGO
```
5. **Primera ejecución física, en seco:** sin filamento cargado y con el hotend caliente, la mano en el paro de emergencia, observa que Z sigue la onda sin ruidos ni pérdida de pasos y que la boquilla no roza nada. Después, una impresión de prueba. **Cualquier cambio en la configuración cambia el código y hay que repetir la vista previa.**

## 8. Pendientes y decisiones para el propietario

1. **Medidas del cabezal** (§7 paso 1): sin ellas no se exporta.
2. **Material y temperaturas** para la pieza de prueba.
3. **Referencia visual:** el PROMPT MAESTRO menciona una pieza impresa con trayectorias curvas y capas no horizontales como referencia. **No tengo esa imagen**: la onda senoidal es un ejemplo mío. Si la pieza de referencia es otra geometría (superficie superior ondulada, cúpula, etc.), dímelo o pásame una foto y adapto el campo de alturas.
4. **Siguiente paso técnico** (no iniciado, requiere autorización): leer superficies desde un modelo real (STL/escena de Cura). Eso exige validar la API de Cura en el programa real, como en la Etapa 3.
5. Prueba física en seco y, si procede, impresión de la pieza (§7 paso 5), con su resultado documentado aquí.
