# Etapa 5 — ARQUIS Non-Planar Engine (experimental)

> **Estado: CERRADA por el propietario el 2026-11-22**, tras pruebas hechas por él en la impresora. Los resultados detallados no están registrados en este documento; las frases «no existe ninguna prueba [FÍSICA]» de abajo describen el estado **anterior** al cierre y se conservan como histórico.

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

Herramienta: `cura/tools/nonplanar_demo.py`. Pruebas: `cura/tests/test_nonplanar.py` (34), `cura/tests/test_nonplanar_surface.py` (12, lectura de superficies; ver §9) `cura/tests/test_nonplanar_sheet.py` (12, lámina en espiral; ver §13) `cura/tests/test_nonplanar_warp.py` (ondulado con volumen; ver §14) y `cura/tests/test_probe_cube.py` (probeta; ver §15).

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

**Lo que sigue siendo estimación** (`head.confirmed` sigue en `false` en esta fecha; ver §15, donde queda confirmado por delegación): la pendiente que tolera el cono (40°, sobre el dibujo), el hexágono (radio 3,5 mm a 2,5 mm de la punta), el radio del ducto y el filamento de 1,75 mm. Para la **pieza de prueba por defecto** (pendiente máxima 10,7°) la estimación del cono no es decisiva, porque está muy por debajo de cualquier valor razonable; sí lo es para piezas más empinadas como la flor (29°).

Resultado con el perfil actualizado sobre el G-code de la flor (`max_slope_deg = 30`): ver el apartado de verificación; ya no salta el ducto (la flor tiene 3,6 mm de relieve frente a 7,3 mm de holgura).

## 12. No hay STL: la referencia es G-code generado por código. Adaptador de G-code externo

El propietario aclara (2026-11-19) que **no existe un STL**: la pieza de referencia es un G-code que se descarga, generado por código con FullControl. Por tanto la lectura de STL (§9) queda como utilidad secundaria, y el camino es **adaptar ese tipo de G-code a la impresora**, con las mismas barreras que el resto del motor.

**Cómo está hecha la flor** (medido sobre el G-code): **una sola trayectoria en espiral** de 17,5 vueltas, del centro (radio 0,7 mm) a un radio de ~15 mm, con cinco pétalos, Z de 0,32 a 3,92 mm, a 8,3 mm/s y extrusión constante. Cada vuelta se apoya en la anterior (una lámina que crece hacia afuera), así que nada se imprime en el aire. Es la misma técnica que el anillo ondulado de la foto de referencia.

**Qué hace `adapt_external_gcode`** (`--adapt-gcode`): conserva la **trayectoria** (el tramo entre `END OF STARTING PROCEDURE` y `START OF ENDING PROCEDURE`) y sustituye lo demás.
- **Se conserva:** cada coordenada y cada E (con 6 decimales). Comprobado contra el original: traslado único de +20 mm en X y en Y, diferencia máxima de Z de 0,00005 mm, extrusión total 170,21 mm.
- **Se aplica como traslado:** el `G92 X0 Y0` del original se convierte en un desplazamiento explícito (+20, +20 mm), y el `G92` desaparece. La pieza queda en X 30–130, Y 32–84.
- **Se quita:** cabecera y final de la Bambu (`M106 P2`, `M900 K0`, `G91`, `G21`, `G92`, purga en el aire para su cortador de filamento, `G0 Z50`).
- **Se pone:** cabecera y final propios, con tu temperatura de cama y de hotend y tu ventilador (**obligatorios**: el adaptador no asume el material), `G28`, tus líneas de inicio (`custom_start_lines`), y un final con retracción y subida de Z.
- **Se limita:** la velocidad de Z al 90 % del máximo de tu firmware (4,5 mm/s), por movimiento.
- **Se rechaza** (no se adivina): archivo sin los marcadores de FullControl, extrusión absoluta (`M82`), arcos (`G2`/`G3`), `M900` u otra orden dentro de la pieza, palabras no soportadas.

**Barreras de exportación** (las mismas que §3): motor activado, cero hallazgos `ERROR`, `head.confirmed`, código de vista previa devuelto, y el texto resultante verificado otra vez (`verify_gcode`). Sin cabezal confirmado **no se exporta**: comprobado con la flor.

**Resultado con la flor** (cabezal Volcano medido, `max_slope_deg = 30`, temperaturas de prueba; **solo cálculo**):

| Comprobación | Resultado |
|---|---|
| Pendiente máxima | 29,1° frente al límite efectivo de 30° (**margen de solo 0,9°**; depende de que el cono tolere de verdad ≥ 35°, que es una estimación) |
| Velocidad en Z | 4,05 mm/s (límite 5; tope del adaptador 4,5) |
| Aceleración de Z estimada | máx. 29,6 mm/s² (límite 100), mediana 5 mm/s² |
| Colisiones (punta, hexágono, bloque, ducto) | ninguna según el modelo |
| Tiempo de la espiral | ~2,5 min sin aceleraciones |
| Avisos | **nivelación**: el archivo no activa tu malla tras el `G28` y la primera capa va a Z 0,30 mm; añade tu orden (p. ej. `M420 S1`) en `custom_start_lines` |

Vista previa de ejemplo: `img/nonplanar_flor_adaptada_vista_previa.png`.

**Antes de imprimir (pendientes del propietario):**
1. **Purga/cebado:** la primera línea de la pieza es un cebado de 100 mm a Z 0,32; el adaptador **no** hace purga en el aire. Pon tu purga habitual en `custom_start_lines`, o asegúrate de que la boquilla está cebada.
2. **Nivelación:** tu orden en `custom_start_lines` (el archivo no la activa).
3. `head.confirmed: true`, `bed_temp`, `nozzle_temp` y `fan_speed` en tu configuración.
4. Genera la vista previa, revísala y devuelve su código:
```bash
python3 cura/tools/nonplanar_demo.py --enable-experimental --config nonplanar.json --adapt-gcode flor.gcode --preview vista.svg
python3 cura/tools/nonplanar_demo.py --enable-experimental --config nonplanar.json --adapt-gcode flor.gcode --export flor_arquis.gcode --accept CODIGO
```
5. **Primera ejecución en seco** (sin filamento, hotend caliente, mano en el paro): observa que Z sigue la onda sin ruidos ni pérdida de pasos y que la boquilla no roza nada; solo después, una impresión.

**Limitaciones:** solo entiende G-code de FullControl con esos marcadores; no cambia la geometría (si la flor no cabe o es demasiado empinada, lo dice, no la modifica); la seguridad frente a colisiones es del **modelo** del cabezal, no de tu hotend; todo sin prueba física.

## 13. Generador propio: lámina en espiral desde tus STL (autorizado 2026-11-19)

Nuevo módulo `engines/nonplanar_sheet.py` y opciones `--sheet-stl` / `--sheet-demo`. Aplica **la técnica de la flor** a un modelo tuyo: una lámina de una sola capa que crece desde un punto de arranque hacia afuera en espiral, con cada vuelta apoyada en la anterior.

**Qué imprime y qué no:** imprime la **cara superior** del STL como una lámina; **no imprime el volumen** del modelo (ni paredes, ni relleno, ni la cara inferior). Es para superficies tipo hoja, cúpula, flor, pétalo.

**Qué formas admite:** las que se recorren como una espiral desde un punto: el punto de arranque debe estar dentro del material y desde él cada dirección debe cruzar el material **una sola vez** (forma «estrellada», sin agujeros entre el centro y el borde). Un hueco de hasta 1 mm se toma como ruido del borde del STL. **Un anillo con agujero NO se admite**: se rechaza con un mensaje que explica por qué. La estrategia para anillos está pendiente y necesita el G-code de referencia del anillo.

**Qué hace, paso a paso:**
1. Lee el STL (§9), proyecta la cara superior sobre una rejilla de 0,5 mm y toma como **arranque el punto más bajo** (se puede cambiar con `--center X,Y`, en coordenadas del STL).
2. Mide el radio del borde en 720 direcciones y calcula la espiral (`r = ρ·R(θ)`, con tantas vueltas como haga falta para el ancho de línea).
3. Baja la superficie para que su punto más bajo quede a la altura de la primera vuelta (0,3 mm) y la coloca con el centro en la cama (`place_x`, `place_y`; por defecto 150, 150).
4. Calcula velocidad (límite de Z al 90 %), extrusión (según el espaciado local entre vueltas) y comprueba límites, colisiones con tu cabezal y aceleración de Z.
5. Escribe el G-code con el mismo envoltorio y **las mismas barreras de exportación** que el adaptador (§12): vista previa con código, cabezal confirmado, temperaturas y ventilador explícitos, cero errores, y verificación independiente del texto.

**Qué rechaza o marca como ERROR** (probado): anillo o forma no estrellada; arranque fuera del material; **arranque en el aire** (el punto de arranque queda más de 0,3 mm sobre la cama, p. ej. una cúpula cuyo centro es lo más alto); **pendiente entre vueltas** mayor que el límite (la vuelta nueva no se apoya, o el cono roza); **espaciado** inferior al 60 % del ancho de línea (forma muy alargada: sobreextrusión); superficie demasiado pequeña; fuera de la cama; colisión con el bloque o el ducto; velocidad o aceleración de Z fuera del firmware.

**Dos fallos míos encontrados al probar, ya corregidos:** el radio del borde salía cuantizado a 0,25 mm y metía saltos de ±0,04 mm en Z (aceleración de Z falsa de 80 mm/s²; ahora ~32); y el borde dentado de un STL rasterizado se confundía con un agujero (tolerancia de 1 mm).

**Cómo usarlo con tu STL:**
```bash
python3 cura/tools/nonplanar_demo.py --enable-experimental --config nonplanar.json --sheet-stl mi_pieza.stl --preview vista.svg
python3 cura/tools/nonplanar_demo.py --enable-experimental --config nonplanar.json --sheet-stl mi_pieza.stl --export mi_pieza_arquis.gcode --accept CODIGO
```
Sin STL, para ver el flujo: `--sheet-demo` (flor paramétrica de 13 mm de radio y 1,4 mm de relieve, **solo de prueba**). Parámetros en el JSON: `sheet_w` (ancho de línea, 0,8 mm), `sheet_t` (espesor y altura de la primera vuelta, 0,3 mm), `place_x`/`place_y`, `max_slope_deg` y los de §7. Requisitos del STL: milímetros, eje Z hacia arriba, forma estrellada desde el punto más bajo.

**Antes de imprimir** (los mismos del adaptador, §12): purga habitual y orden de nivelación en `custom_start_lines`, `head.confirmed`, `bed_temp`, `nozzle_temp`, `fan_speed`, vista previa revisada y primera ejecución en seco.

**Limitaciones:** solo la superficie superior (2,5D); formas estrelladas; sin soportes ni borde; ancho de línea y espesor únicos; el espaciado entre vueltas varía con el radio del borde (se compensa en la extrusión, no en el ancho real); la unión entre vueltas **no está validada físicamente**; todo sin prueba en la impresora. La pendiente que admite depende de la estimación del cono (`head.nozzle_clearance_deg`).

Vista previa de ejemplo: `img/nonplanar_lamina_flor_parametrica.png`.

## 14. Línea principal: ondulado de un G-code normal con volumen (repositorio `makertum/non-planar-layer-fdm`)

El propietario aclara (2026-11-19) lo que busca: **crear volumen** con capas no planas para que las piezas sean **más rígidas**, y aporta el repositorio `makertum/non-planar-layer-fdm`. Es una técnica distinta de la lámina en espiral (§13, que queda como utilidad secundaria): un **post-procesador** que deforma el G-code **normal** de una pieza con paredes y relleno.

**La técnica** (descrita en su README; leí también su script para entenderla): trocear cada movimiento en segmentos cortos y sumar a Z una onda que crece con la altura, `z' = z + rampa(z)·f(x,y)`, compensando la extrusión según el espesor local de cada capa. La tracción entre capas (lo débil en FDM) pasa a ser esfuerzo cortante. La pieza conserva su forma en planta; las capas quedan onduladas y entrelazadas.

**Licencia y autoría:** el repositorio es AGPL-3.0 (Moritz Walter, 2016). **Esta implementación es propia**: misma idea, fórmulas escritas aquí, ningún fragmento de su código copiado. Diferencia deliberada: mi onda tiene **media cero** (la suya, `−A/2 + …`, desplaza la pieza hacia abajo en promedio y comprime las capas).

**Implementación:** `engines/nonplanar_warp.py` + `tools/nonplanar_warp.py` (desactivado por defecto, no conectado a Cura).
- **Desplazamiento:** `f` = `wave` (dos senos en X e Y, pico a pico = amplitud), `radial`, o una **expresión propia** (x, y, r, A, L) evaluada con una lista blanca de nodos (el original usaba `eval` de Perl): se rechazan atributos, llamadas arbitrarias, índices y `__import__` (13 casos probados). Rampa de entrada (`z_in`, `ramp`) y, opcional, de salida (`z_out`, cara superior plana).
- **Extrusión:** `E' = E·(1 + (dz(z) − dz(z−h))/h)`, con `h` el espesor nominal de cada capa leído del propio G-code. Funciona con E absoluta (M82, la de Cura) y relativa (M83).
- **Respeta tu archivo:** solo toca la pieza (de `;LAYER:` a la última `;TIME_ELAPSED:`); tu inicio y tu final quedan intactos, las marcas `;ARQUIS` y el manifiesto con su CRC también, y el banner de código va después de ellas (dentro de los primeros 512 B).
- **Al terminar la pieza** inserta una **subida segura** (`G0 Z…`, solo sube, hasta la cresta más alta + 0,5 mm) *antes* de tu final: tu `G91; G1 Z5` es relativo y habría partido de una posición ondulada más baja (detectado por una prueba: acababa 0,18 mm más bajo).

**Lo que añade frente al script original** (el original no comprueba nada de esto) y que **bloquea la exportación**: pendiente máxima de la superficie (cono del cabezal), espesor de capa admisible (0,08–0,30 mm con boquilla 0,4), velocidad de Z limitada al 90 % del firmware por segmento, límites de cama y de Z, holgura frente al bloque, el ducto y el hexágono, conservación del volumen (E total ±8 %), y una **verificación independiente del texto resultante**. Además: vista previa con código, `head.confirmed`, y cero errores.

**Prueba con tu G-code real** (6 piñones, 246 065 líneas, 4 mm de altura) con el perfil Volcano, amplitud 1 mm, λ 20 mm, `z_in` 0,8, rampa 2 mm [CÁLCULO]:

| Medida | Resultado |
|---|---|
| Extrusión total | 11 851,79 → 11 851,91 mm (**+0,001 %**: el volumen se conserva) |
| Multiplicador de E por segmento | 0,75–1,25; espesor de capa 0,18–0,30 mm (nominal 0,24) |
| Pendiente de la superficie | 6,2° (límite efectivo 15°) |
| Movimientos | 243 758 → 462 742 (5 % con la velocidad limitada por Z) |
| Archivo | 7,7 → 18,3 MB; se procesa en ~7 s |
| Holgura de cabezal usada | 0,95 mm (menor que cualquiera de las tres holguras del modelo) |
| Verificación del texto | sin errores |

La rampa de 2 mm en una pieza de 4 mm es **muy brusca** y deja el espesor en el límite (1,25×); para piezas reales usa una rampa mayor (p. ej. 5 mm en el perfil de ejemplo `profiles/nonplanar_warp_arquis_volcano.json`).

**Uso:**
```bash
python3 cura/tools/nonplanar_warp.py --init-config warp.json
python3 cura/tools/nonplanar_warp.py --enable-experimental --config warp.json --input pieza.gcode --preview vista.svg
python3 cura/tools/nonplanar_warp.py --enable-experimental --config warp.json --input pieza.gcode --export pieza_ondulada.gcode --accept CODIGO
```
Parámetros: `amplitude` (pico a pico, mm; tope duro 4), `wavelength`, `function`, `center_x/y` (origen de la onda; por defecto el centro de la cama), `z_in`, `ramp`, `z_out`, `max_seg`, `max_slope_deg`, `head.*`. Vista previa de ejemplo: `img/nonplanar_ondulado_vista_previa.png`.

**Limitaciones y riesgos (sin prueba física):**
- **Rigidez no demostrada:** que la pieza resulte más rígida es la hipótesis del repositorio original; aquí **no se ha medido**. Haría falta imprimir probetas planas y onduladas y ensayarlas.
- **La pieza queda deformada** (la cara superior ondula si no defines `z_out`); para piezas rectas hay que pre-deformar el diseño o usar `z_out`.
- **Manifiesto y cancelación de objetos:** no se deforma el manifiesto ARQUIS (los estados «pendiente/imprimiendo/completado» pueden adelantarse o retrasarse hasta ~la mitad de la amplitud) y la **cancelación con `M486` no está probada con un archivo ondulado**. La cancelación visual sigue bloqueada de todos modos.
- **Aceleración de Z:** el texto no se simula con el planificador de Marlin; la velocidad de Z sí se limita por segmento.
- Solo G-code de Cura/Marlin con XYZ absoluto dentro de la pieza y sin arcos; se rechaza `G91`, `G92 X/Y/Z` y `G2/G3` dentro de la pieza.
- Soportes, brim y torres también se ondulan por encima de `z_in`; las capas por debajo de `z_in` quedan planas (adherencia a la cama intacta).
- El choque con el cabezal se calcula con el **modelo** (7,3 mm medidos; cono, hexágono y radio del ducto estimados); `head.confirmed` lo cambias tú.
- Tamaño de archivo ×2,4 y más líneas por segundo para Marlin: **no se ha comprobado** que la lectura desde la SD y el planificador lo sostengan a tus velocidades.

**Antes de imprimir:** confirma el cabezal, elige amplitud y rampa prudentes, revisa la vista previa, y haz primero una **probeta pequeña** (p. ej. un cubo de 30 mm) con el código aceptado y una primera ejecución en seco (sin filamento, mano en el paro).

## 15. Cabezal confirmado, probeta de 30 mm y script de Cura (autorizado 2026-11-19)

**Cabezal confirmado por delegación.** El propietario autoriza que `head.confirmed` lo ponga Claude. Queda `true` en `profiles/nonplanar_arquis_volcano.json`, `profiles/nonplanar_warp_arquis_volcano.json`, `profiles/nonplanar_probeta_cubo_30mm.json` y en `plugins/ARQUISPlugin/nonplanar_head.json` (la copia que viaja con el plugin; una prueba exige que coincida con el perfil). **Qué está medido y qué no:** medido por el propietario con calibre: 7,3 mm de la punta a la cara inferior del bloque y ductos a la altura del bloque. **Siguen siendo estimaciones:** la pendiente que tolera el cono (se usa **35°**, más prudente que los 40° estimados sobre el dibujo), el hexágono (radio 3,5 mm a 2,5 mm de la punta), el radio del ducto (25 mm) y el filamento de 1,75 mm. La confirmación **no** convierte esas estimaciones en medidas; solo deja que el motor trabaje con ellas. El material (temperaturas y ventilador) sigue sin asumirse.

### Probeta cúbica de 30 mm
`tools/make_probe_cube.py` genera el G-code **plano** de un cubo (2 perímetros, relleno rectilíneo al 20 %, 4 capas sólidas arriba y abajo, retracciones, 150 capas de 0,2 mm) con **tu propio inicio y final** de Cura (temperaturas 70/215 °C, `M420 S1` con tu malla, purga). **No es una laminación de Cura:** para una prueba más fiel, lamina un cubo en Cura con tu perfil y pásalo por el ondulador; el flujo es el mismo. Configuración: `profiles/nonplanar_probeta_cubo_30mm.json` (amplitud 1 mm pico a pico, λ 20 mm, onda desde 0,8 mm con 5 mm de rampa, **cara superior plana**).

```bash
python3 cura/tools/make_probe_cube.py --out probeta_cubo_30mm_plano.gcode
python3 cura/tools/nonplanar_warp.py --enable-experimental --config cura/profiles/nonplanar_probeta_cubo_30mm.json --input probeta_cubo_30mm_plano.gcode --preview vista.svg
```
Abre `vista.svg` (ejemplo: `img/nonplanar_probeta_cubo_vista_previa.png`), y **solo si estás de acuerdo** exporta con el código que imprimió (con esta versión, `570AA8AA5214`; si el tuyo difiere, usa el tuyo):
```bash
python3 cura/tools/nonplanar_warp.py --enable-experimental --config cura/profiles/nonplanar_probeta_cubo_30mm.json --input probeta_cubo_30mm_plano.gcode --export probeta_cubo_30mm_ondulada.gcode --accept 570AA8AA5214
```

| Medida de la probeta [CÁLCULO] | Resultado |
|---|---|
| Extrusión total | 3289,279 → 3289,285 mm (volumen conservado) |
| Espesor de capa | 0,18–0,22 mm (×0,90–1,10) |
| Pendiente de la superficie | 6,1° (límite efectivo 15°) |
| Holgura de cabezal usada | 0,98 mm (de 2,5 / 7,3 / 7,3 mm) |
| Archivo | 225 KB plano → **4,6 MB** (117 674 líneas) |
| Tiempo | ~43 min plano, ~44 min ondulado (sin aceleraciones: será mayor) |
| Cara superior | plana: ninguna línea de la última capa por encima de 30,000 mm |

**Primera ejecución en seco** (sin filamento, la mano en el paro): el archivo trae tu inicio, que **calienta (215 °C) y purga**; sin filamento la purga no extruye nada. Baja las temperaturas si lo prefieres. Observa que Z sigue la onda sin ruidos ni pérdida de pasos y que la boquilla no roza nada; la onda alcanza su amplitud completa hacia los 6 mm y vuelve a cero desde los 25 mm. **No hace falta llegar a los 30 mm** en el ensayo en seco: con 10 mm ya has visto Z en régimen. Después, una impresión real con filamento.

### Script de Cura (el «plugin»)
`scripts/ARQUIS_NonPlanar.py` (adaptador, como `ARQUIS_M486.py`) + el motor en `plugins/ARQUISPlugin`. **Apagado por defecto** y con las barreras de la línea de comandos reproducidas dentro de Cura:
1. `Activar el ondulado` está **apagado**: no se toca nada, ni se carga el motor.
2. Con él activado, la **primera exportación no aplica nada**: escribe la vista previa `~/arquis_nonplanar_vista.svg` y muestra un mensaje con el **código** de 12 caracteres.
3. Solo cuando escribes ese código en el ajuste «Código de vista previa aceptado» y exportas de nuevo se aplica el ondulado. **El código es estable** (2026-11-19, tras la prueba del propietario en Cura real): depende de los **ajustes del ondulado, el número de capas y la altura de la pieza**, no de cada byte del G-code. Motivo: en Cura real el G-code que recibe el script **cambia entre exportaciones aunque no cambies nada** (el propietario midió la misma «huella de ajustes» `CC0C49CE` con «huellas de entrada» distintas `323DCEF3` y `128EBD80`; en otro caso, diferencias de milésimas en las coordenadas y de 0,01 % en el filamento), así que atar el código al texto exacto lo hacía inaceptable. La primera exportación guarda en `~/arquis_nonplanar_vista.json` la firma de la pieza que mostraste (caja XY, capas, altura, filamento); al exportar con el código, se compara con **tolerancia** (±1 mm en la caja XY, ±0,05 mm de altura, ±3 % de filamento, mismas capas). Si la pieza cambia de verdad (otra posición, tamaño o cantidad de material) se pide revisar de nuevo la vista previa. **La seguridad no depende de esto:** los límites, la pendiente, el espesor, la velocidad de Z y las colisiones se calculan en **cada** exportación sobre el G-code real; el código solo registra que el propietario revisó una vista previa de esa pieza. (La línea de comandos conserva el código exacto, porque allí el archivo no cambia.)
4. Cualquier error (parámetros, estructura, cabezal, límites) devuelve el G-code **original** sin tocar, con el motivo.

**Instalación:** copia `cura/plugins/ARQUISPlugin` a la carpeta `plugins` de Cura (incluye `nonplanar_head.json`) y `cura/scripts/ARQUIS_NonPlanar.py` a `scripts`; reinicia Cura. **Orden: «ARQUIS Ondulado no planar» debe ser el ÚLTIMO de la lista**, después de «ARQUIS M486» y «ARQUIS Validation». Medido con el G-code real de seis piñones y las instancias capturadas de Cura: si el ondulado va **antes**, M486 **no se bloquea** (se insertan igual sus `M486`), pero emite avisos `W_Z_IN_EXTRUDING_MOVE` y `V_Z_EXTRUDING` («cancelar un objeto en esos tramos puede afectar a la altura»), y esa cancelación **no está probada**; el script avisa si detecta que M486 aún no se ejecutó. **Ajustes:** amplitud, longitud de onda, función, altura de inicio, rampa, cara superior plana y el código aceptado.

**Estado de validación:** el script y el motor están probados con una escena **falsa** de `Script` (14 pruebas del script, incluida la comprobación de que todos los ajustes tienen `label`, `description`, `type` y `default_value`, el fallo que cerró Cura en el script de M486). **No se ha ejecutado en Cura real**: hay que cargarlo, ver que aparece, activarlo en una pieza, comprobar el mensaje con el código y la vista previa, y exportar con el código. Hasta entonces no está validado.

## Cierre (2026-11-22)

**Etapa 5 cerrada por el propietario el 2026-11-22**, tras pruebas hechas por él en la impresora. Los resultados detallados de esas pruebas **no se han trasladado a este repositorio**: lo que consta aquí es la decisión de cierre, no una medición. El motor sigue **desactivado por defecto** y las barreras de exportación no cambian.
