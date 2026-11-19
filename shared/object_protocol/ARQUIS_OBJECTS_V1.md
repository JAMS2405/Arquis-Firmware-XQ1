# ARQUIS_OBJECTS_V1 — manifiesto de objetos (especificación)

Estado: **implementado en Cura** (generador, lector y validador de referencia, Etapa 2) · **pendiente en el firmware** (parser y pantalla, Etapa 3). Capacidad de diseño: **30 objetos, IDs 0–29**. Todos los límites vienen de `limits.json`; esta especificación los cita pero **no los define**.

## 1. Transporte
Comentarios del propio G-code (autosuficiente: un solo archivo). Líneas ≤ 80 B (medido: 70 B), ASCII salvo los nombres (UTF-8). Fin de línea `\n` **o `\r\n`** (Cura en Windows exporta CRLF: los dos G-code reales aportados lo traen). Marlin ignora los comentarios, igual que ya hace con `;gimage:`.

```
;ARQUIS_OBJECTS=1 PID=3FA9C21B N=30             cabecera, ≤ 40 B, en los 512 B iniciales (detección rápida)
...                                              (G-code)
;ARQUIS_OBJECTS_BEGIN                            bloque, al FINAL del archivo
;ARQ:H V=1 PID=3FA9C21B N=30 BED=3000,3000 MIN=0,0 ORG=FL SEQ=0 CAP=30
;ARQ:M <idx> <nombre base, ≤ 20 B UTF-8>        tabla de nombres (los de las copias se guardan UNA vez)
;ARQ:O <id> <idx_nombre> <inst> <cx> <cy> <ancho> <prof> <rot> <tipo C|S|R> <z0> <ztop> <orden>
;ARQUIS_OBJECTS_END CRC=1A2B3C4D
;ARQUIS_TRAILER CRC=1A2B3C4D PID=3FA9C21B N=30
;SETTING_3 ...                                   (lo añade Cura DESPUÉS: unos KB)
```
- **Por qué al final:** el firmware busca `;simage:` solo en los primeros 512 B; un bloque al principio la desplazaría o dependería del orden de los scripts de post-procesado. La cabecera de 40 B es el único coste en esa zona.
- **Cómo lo localiza el firmware:** lee los **últimos 16 KB** (`TAIL_WINDOW`; tras el bloque Cura añade su `;SETTING_3`, 1,1 KB en el G-code real), busca la **última** línea `;ARQUIS_OBJECTS_BEGIN`, lee hasta `END` en trozos de 512 B, verifica CRC y trailer. Si el bloque queda fuera de la ventana: sin manifiesto (aviso), impresión normal.
- **No hay longitudes ni desplazamientos en bytes** (cambiarían con CRLF o si otro script toca el archivo). El CRC-32 (IEEE, `zlib.crc32`) cubre las líneas entre `BEGIN` y `END`, **cada una normalizada como texto sin `\r` + `\n`**.
- `PID` (identificador de impresión) = CRC-32 de `BED=…` y de las líneas M y O: cambia si cambia la cama, un nombre o una posición.

## 2. Campos
| Campo | Significado |
|---|---|
| `V` | versión del formato (1). Desconocida ⇒ se ignora el manifiesto |
| `N` | número de objetos publicados (= `M486 T<N>`; ≤ `design_capacity`) |
| `BED=w,d` · `MIN=x,y` | cama y su esquina mínima, **0,1 mm**, en coordenadas de G-code (en la mayoría de máquinas 0,0) |
| `ORG` | origen de la vista: `FL` = el origen (X0 Y0) es la esquina **frontal izquierda** (vista desde el frente: X→derecha, Y→arriba/atrás). El firmware puede reflejar ejes cambiando `ORG` sin cambiar el protocolo |
| `SEQ` | 1 = un objeto cada vez (impresión secuencial); 0 = capa a capa |
| `CAP` | capacidad de cancelación supuesta (≤ `capacity_ceiling`) |
| `id` | índice de `M486 S<id>`; 0…N−1, contiguos, únicos. **En pantalla se muestra `id + 1`** |
| `idx_nombre`, `inst` | nombre base y nº de copia (0…): «Llavero», copias 0…29. La pantalla compone «Llavero 3». Cura renombra las copias `nombre(n)`; aquí se guarda el nombre base y la copia por separado |
| `cx cy ancho prof` | centro y tamaño de la caja de la escena (lo que ve el usuario en Cura), 0,1 mm |
| `rot` | giro en grados 0–359 (informativo: el marcador es un rectángulo alineado con los ejes; el firmware **no lo guarda**) |
| `tipo` | `C` círculo, `S` cuadrado, `R` rectángulo (referencia visual por la huella de Cura; las alargadas son `R`) |
| `z0`, `ztop` | Z (0,1 mm) de la primera y de la última capa con extrusión, **medidas en el G-code** |
| `orden` | orden de impresión 0…N−1 (el de la primera extrusión en el G-code) |

Objetos con `id ≥ capacidad` **no se publican** (la pantalla no inventa objetos que no puede cancelar); Cura avisa de cuántos son.

## 3. Estados en pantalla (texto **e** insignia, no solo color)
| Estado | Texto | Insignia | Regla |
|---|---|---|---|
| Pendiente | Pendiente | — | capa a capa: `z_print < z0` · secuencial: su orden > el del activo |
| En impresión | Imprimiendo | `>` | capa a capa: `z0 ≤ z_print ≤ ztop` · secuencial: es el activo |
| Completado | Completado | `+` | capa a capa: `z_print > ztop` · secuencial: su orden < el del activo |
| Cancelado | Cancelado | `x` | en la máscara de cancelados de Marlin |
| No disponible | No disponible | `?` | manifiesto no verificado (`UNTRUSTED`) o id fuera de capacidad |
`z_print` = Z de la **última extrusión** (no la del salto en Z). Funciones: `bed_map_engine.object_state` (vectores en `test_vectors/bed_map_v1.json`).

## 4. Mapa de la cama (aritmética entera, igual en C++)
`px = X0 + W/2 + div_round(S·zoom·(x − cx), max(ancho, fondo))`, `py = Y0 + H/2 − div_round(S·zoom·(y − cy), max(ancho, fondo))` con `S = min(W,H) − 16` y `div_round(a,b) = floor((2a+b)/(2b))`. Escala **única** (la cama se ve proporcionada). Tamaño del marcador proporcional con mínimo de 22 px alrededor de su **centro real**: **nunca se desplaza un objeto**. Selección por toque: gana el marcador de menor área. Zoom ×1/×2/×4 conservando el centro. Vectores exactos en `test_vectors/bed_map_v1.json` (391 casos).

## 5. Validaciones obligatorias (Cura y firmware; mismo resultado en `test_vectors/manifest_v1.json`, 26 casos)
Versión desconocida · `N` ≠ filas · IDs duplicados / fuera de rango / ≥ capacidad · coordenadas fuera de cama · tamaño ≤ 0 · `z0 > ztop` · rotación > 359 · nombre > 20 B o índice inexistente · órdenes que no son permutación · `CAP` > techo · CRC · trailer (CRC/PID/N) · cabecera ≠ bloque · cama del manifiesto ≠ cama de la máquina. **Cualquier fallo ⇒ sin mapa ni cancelación visual; la impresión normal no se ve afectada.** Sin manifiesto (G-code antiguo) no es un error.
Verificación en ejecución (Etapa 3, coste cero en el camino caliente): `N` = el `M486 T` recibido, y la caja XY aprendida de cada objeto (`note_extrusion`) dentro de la del manifiesto (+ margen); si no, `UNTRUSTED`.

## 6. Compatibilidad
Versionado `ARQUIS_OBJECTS_V1`; un lector que no conozca la versión ignora el manifiesto. Cura declara `V=1`. Un firmware sin parser (el actual) ignora los comentarios: el G-code funciona igual (M486 incluido).
Archivos: `limits.json` (límites), `arquis_objects_v1.schema.json` (forma lógica), `test_vectors/` (conformidad), `generated/arquis_object_limits.h` (constantes C++).
