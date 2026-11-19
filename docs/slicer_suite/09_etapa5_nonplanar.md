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

Herramienta: `cura/tools/nonplanar_demo.py`. Pruebas: `cura/tests/test_nonplanar.py` (22) y `cura/tests/test_nonplanar_surface.py` (12, lectura de superficies; ver §9).

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

## 9. Actualización 2026-11-19 (2): pieza de referencia, tu boquilla y lectura de superficies

**Pieza de referencia** (`img/nonplanar_referencia_anillo.webp`): un anillo con agujero cuya superficie ondula; en las pendientes se ven líneas paralelas a los bordes. Es el objetivo real, y es distinto de mi ejemplo (placa con onda en una sola dirección). Dos diferencias que el motor **todavía no cubre**: (1) el **agujero** obliga a interrumpir las trayectorias, retraer y saltar; (2) las zonas altas de una pieza así no se pueden imprimir en el aire: hay que saber **cómo se imprime** (¿una lámina que crece desde una base plana? ¿el anillo se apoya en algo?). Necesito el **STL** de la pieza y cómo la imprimes para decidir el esquema de capas.

**Tu boquilla** (`img/boquilla_volcano_propietario.webp`): estilo Volcano, con bloque calefactor Volcano. La boquilla sobresale más que en un V6 y el bloque es mayor, así que **no sirven los valores por defecto** del modelo de cabezal. No asumo ninguna cota: hay que medirlas con calibre (§ tabla).

| Medida (con calibre, en mm) | Parámetro del motor |
|---|---|
| Altura del cono visible, de la punta al hexágono | pendiente máxima: `head.nozzle_clearance_deg` (ángulo del cono respecto a la vertical → pendiente tolerable = 90° − ese ángulo) |
| Distancia de la punta a la cara inferior del bloque calefactor | `head.envelopes[0]` altura sobre la punta |
| Radio (o semiancho) del bloque calefactor | `head.envelopes[0]` radio |
| Altura sobre la punta y radio del ducto de ventilación | `head.envelopes[1]` |
| Diámetro real del filamento | `filament_d` |

**Lectura de superficies desde un modelo (autorizada).** Nuevo módulo `engines/nonplanar_surface.py` y opción `--analyze-stl` de la herramienta: lee un STL (binario o ASCII), proyecta sus triángulos sobre una rejilla XY y obtiene la altura de la cara superior e inferior por celda, mide pendiente, relieve y espesor, detecta **agujeros** y avisa si la pieza **no es una superficie 2,5D** (voladizos o piezas apiladas). Genera una vista previa en SVG. **Solo analiza: no genera trayectorias ni G-code desde el modelo.**

```bash
python3 cura/tools/nonplanar_demo.py --enable-experimental --analyze-stl anillo.stl --preview analisis.svg
```
- **Pruebas** (`test_nonplanar_surface.py`, 12): STL binario y ASCII equivalentes; archivos truncados, vacíos o con valores no finitos se rechazan; un anillo ondulado **generado en la prueba** (no es tu pieza) se mide con la geometría correcta (área, relieve, pendiente, espesor, agujero central); una superficie demasiado inclinada o con relieve excesivo da ERROR; placas apiladas se detectan como no 2,5D. Son pruebas de cálculo.
- **Limitaciones:** solo STL (no 3MF), en milímetros y tal como está (no lo coloca en la cama); resolución limitada por la celda (por defecto 1 mm, la pendiente puede quedar subestimada con celdas grandes).
- **Lectura desde la escena de Cura: no implementada.** Exige validar en Cura real la API de la malla (`getMeshData`, vértices e índices, transformación al mundo), como en la Etapa 3: primero un volcado de diagnóstico ejecutado por ti dentro de Cura. Hasta entonces, el camino es exportar el STL.

**Siguiente paso técnico (no iniciado):** generar capas conformes y trayectorias desde la superficie leída, con agujeros (viajes, retracción y salto en Z). Antes necesito tu STL y cómo se imprime la pieza.

## 10. Cabezal derivado de los dibujos de la boquilla y del bloque Volcano, y análisis del G-code de la flor

**Origen de los datos** (`img/boquilla_laton_m6x1_dibujo.png`, `img/bloque_volcano_dibujo.png`): dibujos del fabricante. **No son medidas con calibre de tu hotend.** Perfil resultante: `cura/profiles/nonplanar_arquis_volcano.json`, con `head.confirmed: false`.

| Dato | Valor | Cómo sale |
|---|---|---|
| Boquilla | latón M6×1, 23 mm total, rosca 14 mm, hexágono de 6 mm, salida Ø0,4 | Dibujo |
| Bloque | Volcano 20 × 20 × 11,5 mm (alto 20 mm) | Dibujo |
| Parte de la boquilla sin rosca | 23 − 14 = **9 mm** (hexágono + cono) | Cálculo |
| **Sobresale de la cara inferior del bloque** | **9 mm** | **Suposición:** rosca entera dentro del bloque y hexágono apoyado en su cara inferior. Si no está enroscada a fondo, sobresale más |
| Volumen del bloque (cilindro equivalente) | radio **11,5 mm**, empieza **9 mm** sobre la punta | Media diagonal √(10² + 5,75²); la orientación de 20 y 11,5 en X/Y no se conoce, así que se toma lo conservador |
| Hexágono de la boquilla | radio 3,5 mm, empieza 2,5 mm sobre la punta | Estimado sobre el dibujo (cono de ~3 mm) |
| Pendiente tolerable del cono | **40°** (con margen de 5° → 35°) | **Estimación sobre el dibujo**, no medida |
| Ducto de ventilación | **sin derivar** (valores por defecto: radio 25 mm, 2 mm sobre la punta) | No sale de los dibujos |

**Inconsistencia en el dibujo de la boquilla:** rotula «cuerpo 15 mm» y «rosca 14 mm», pero el largo total es 23 mm (15 + 14 ≠ 23). Uso 23 y 14 (quedan 9 mm sin rosca). Si lo que sobresale del bloque fuera otro valor, **cambia el cálculo**.

**Lo único imprescindible que falta medir con calibre:**
1. **Distancia de la punta a la cara inferior del bloque** (sustituye el 9 mm supuesto).
2. **Ducto de ventilación:** altura de su punto más bajo sobre la punta y distancia al eje de la boquilla.
3. Diámetro real del filamento (hoy 1,75 mm sin medir).
Con eso se corrige el perfil y se pone `head.confirmed: true`.

### G-code de la flor (`Non-planar_flower.gcode`, FullControl para una Bambu)
Solo lectura con la nueva opción `--check-gcode`; **no se modifica y no se da por apto para imprimir en tu máquina**. El archivo no se ha incorporado al repositorio (es de terceros).

| Medida | Resultado |
|---|---|
| Extrusión no plana | 2022 movimientos, Z 0,32–3,92 mm, ~100 × 52 mm, velocidad 8,3 mm/s, extrusión constante (0,133 mm de filamento por mm de trayecto), sin retracciones |
| Pendiente máxima | **29,1°** (media 12,6°) |
| Velocidad en Z que pide | **4,05 mm/s** (límite del firmware: 5): cabe, con poco margen. La aceleración de Z no se ha podido evaluar |
| Con el cabezal derivado y `max_slope_deg = 30` | Sin colisión de la punta ni del bloque ni del hexágono (modelo). Solo salta el ducto, que es el valor **sin medir** |
| Con `max_slope_deg = 15` (por defecto) | ERROR de pendiente: esta pieza exige subir el límite de proceso a ≥ 30°, y eso depende de que el cono tolere de verdad esos 29° (estimado en 40°, sin medir) |

**Órdenes del archivo que NO son para tu impresora** (el verificador las marca): `G92 X0 Y0` (desplaza el origen +20 mm: la pieza queda en X 30–130, Y 32–84), `M106 P2` (ventilador auxiliar que tu firmware puede no tener), `M900 K0` (apaga el Linear Advance de tu firmware hasta reiniciar), `G91`, `G21`, `M220`, `M221`, y viajes `G0 F8000` con Z (Marlin los limita a 5 mm/s). Además: bloque de cebado de 100 mm a Z 0,32, `M109 S210` y cama a 50 °C (material de otra máquina). **No lo imprimas tal cual.**

### Correcciones del verificador hechas con este archivo
El análisis detectó dos fallos de mi comprobación de colisiones, ya corregidos y con pruebas: (1) la punta se comparaba con celdas de 3 mm, lo que daba falsos positivos al imprimir junto a una pared más alta; ahora usa una rejilla de 0,5 mm; (2) las líneas largas solo marcaban sus extremos como impresos; ahora se marca todo el recorrido. Los volúmenes pequeños (radio ≤ 6 mm) se comprueban con la rejilla fina.

```bash
python3 cura/tools/nonplanar_demo.py --enable-experimental --config cura/profiles/nonplanar_arquis_volcano.json --check-gcode archivo.gcode
```

## 11. Medidas del propietario con calibre (2026-11-19)

Tres fotos con calibre digital (lecturas visibles 17,98 / 26,5 / 17,69 mm; **no consta qué mide cada una**, así que no las uso) y la conclusión del propietario: **de la punta de la boquilla a la cara inferior del bloque hay 7,3 mm, y los ductos de aire de capa quedan a la altura del bloque.** Las fotos muestran el bloque con **manguito de silicona** naranja.

| Parámetro | Antes (dibujos) | Ahora |
|---|---|---|
| Punta → cara inferior del bloque | 9 mm (suposición 23 − 14) | **7,3 mm (medido)** |
| Ducto de ventilación, altura sobre la punta | por defecto, sin medir (2 mm) | **7,3 mm** (a la altura del bloque, según el propietario) |
| Ducto, radio | por defecto (25 mm) | **25 mm, conservador, no medido** (solo importa si la pieza tiene más de 7,3 mm de relieve) |

**Discrepancia que queda anotada:** el dibujo de la boquilla (23 mm, rosca 14 mm) daría 9 mm; lo medido es 7,3 mm. Probablemente la boquilla montada no es exactamente la del dibujo, o el manguito de silicona o el roscado cambian la cota. Se usa lo medido.

**Lo que sigue siendo estimación** (`head.confirmed` sigue en `false`; lo cambia el propietario): la pendiente que tolera el cono (40°, sobre el dibujo), el hexágono (radio 3,5 mm a 2,5 mm de la punta), el radio del ducto y el filamento de 1,75 mm. Para la **pieza de prueba por defecto** (pendiente máxima 10,7°) la estimación del cono no es decisiva, porque está muy por debajo de cualquier valor razonable; sí lo es para piezas más empinadas como la flor (29°).

Resultado con el perfil actualizado sobre el G-code de la flor (`max_slope_deg = 30`): ver el apartado de verificación; ya no salta el ducto (la flor tiene 3,6 mm de relieve frente a 7,3 mm de holgura).

