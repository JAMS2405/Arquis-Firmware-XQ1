# ARQUIS Slicer Suite — Etapa 0: auditoría

Rama `claude/dreamy-allen-gc05as` (desde `main`, intacta). **No se ha modificado ningún archivo de código.** Este documento es lo único que se añade al repositorio del firmware; el plugin de Cura irá en el repositorio independiente `ARQUIS-Cura` (todavía no creado).

Todo lo marcado **[VERIFICADO]** se comprobó leyendo o ejecutando el código real (rutas y líneas indicadas). Lo marcado **[NO VERIFICADO]** es una hipótesis que hay que confirmar con G-code real o con la impresora.

## 0. Resumen ejecutivo

| Pregunta | Respuesta corta |
|---|---|
| ¿Cura da la posición de cada copia idéntica? | **Sí**, desde la escena (`SceneNode.getBoundingBox()` / `getWorldPosition()`). Lo que **no** da es un identificador estable: las copias tienen el mismo nombre y `getId()` vacío. |
| ¿El G-code distingue copias idénticas? | **No por nombre.** CuraEngine escribe `;MESH:<nombre>` y las copias comparten nombre. Hay que distinguirlas por **geometría** (caja XY del tramo de G-code frente a la caja de cada instancia en la escena). `cura-M486` las fusiona en un solo objeto. |
| ¿Marlin soporta 50/100/200 objetos? | **No tal como está.** El motor cancela como máximo **32 objetos** (máscara `uint32_t`) y tiene un **defecto de desbordamiento** con índices ≥ 128 (medido, §3.3). Hay que ampliarlo (cambio pequeño, fuera del planificador). |
| ¿Cabe el mapa proporcional en LVGL? | **Sí.** Ya se dibuja con 6 objetos LVGL fijos (≈ 1,9 KB de heap, medido en Etapa 6). La arquitectura se mantiene; se cambia el modelo de datos (tabla compacta + lista virtualizada). |
| ¿Se puede reutilizar `cura-M486`? | **No el código.** Licencia CC BY-NC-SA 4.0: prohíbe uso comercial y obliga a la misma licencia (incompatible con LGPL/GPL). Se reimplementa desde cero; se usa solo como referencia de comportamiento. |

## 1. Estado actual de Cura 5.11.0 (código real: Ultimaker/Cura, CuraEngine y Uranium, tag `5.11.0`)

| Tema | Hallazgo | Fuente |
|---|---|---|
| Python / plugins | CPython **3.12.2** empaquetado, PyQt6, plugin **API 8** | `conanfile.py:562`, `plugins/PostProcessingPlugin/plugin.json` |
| Tipo de plugin | `Extension` (menú) + script de post-procesado (`Script`, método `execute(data)`; `data` = lista de cadenas, una por capa, la 0 es la cabecera) | `UM/Extension.py`, `PostProcessingPlugin/Script.py` |
| Cuándo se post-procesa | Señal `OutputDeviceManager.writeStarted` → `PostProcessingPlugin.execute()`; marca `;POSTPROCESSED` en `gcode_list[0]` | `PostProcessingPlugin.py:51,71-110` |
| Acceso entre plugin y script | `PluginRegistry.getInstance().getPluginObject("<id>")` existe | `UM/PluginRegistry.py:959` |
| Qué envía Cura al motor | Por cada nodo: `obj.id = id(object)` (id Python **no** exportado al G-code) y `obj.name = object.getName()` | `StartSliceJob.py:502-503` |
| Orden de objetos | Recorrido `DepthFirstIterator` de la escena (modo normal) u `OneAtATimeIterator` (secuencial) | `StartSliceJob.py:348-403` |
| Copias ("Multiplicar") | `copy.deepcopy(node)`: copia nombre y transformación; **no** copia `_id` (queda `""`) | `MultiplyObjectsJob.py:64`, `CuraSceneNode.__deepcopy__` |
| Marca en el G-code | `;MESH:<mesh_name>` cuando cambia `path.mesh`; `;MESH:NONMESH` si es nulo | `CuraEngine/src/LayerPlan.cpp:3353-3358` |
| Nombre de la malla | `mesh_name_ = object.name()` (el nombre del nodo) | `ArcusCommunicationPrivate.cpp:157` |
| **Orden de los tramos en una capa** | **No es el orden de la escena**: `calculateMeshOrder` ordena las mallas por vecino más próximo desde `layer_start_x/y`, por extrusor | `FffGcodeWriter.cpp:1709-1735` |
| Origen de coordenadas | Con `machine_center_is_zero = false` el motor suma `machine_width/2`, `machine_depth/2`; y Cura convierte `y_gcode = −z_escena` (`verts[:,1] *= -1`) | `MeshGroup.cpp:99-107`, `StartSliceJob.py:497-499` |
| Conclusión de ejes | `x_gcode = x_escena + ancho/2`, `y_gcode = −z_escena + profundidad/2`. El **frente** de Cura (+z escena, hacia la cámara) queda en **Y = 0**. | derivado de lo anterior **[VERIFICADO en código; falta prueba física]** |
| Huella del objeto | `ConvexHullDecorator.getConvexHull()` / `getPrintingArea()` (polígonos en la escena) | `ConvexHullDecorator.py:88-173` |
| Instalación de scripts | Carpeta `scripts` del directorio de configuración de Cura; plugins en `.../cura/5.11/plugins` | `PostProcessingPlugin.py:139-160` |

**No he podido ejecutar Cura ni CuraEngine en este contenedor** (sin PyQt6/Arcus, sin binario del motor; instalar Conan + dependencias no es proporcionado). Consecuencia: los fixtures de la Etapa 1 serán G-code sintético fiel al formato leído en `LayerPlan.cpp`, **y necesito G-code real tuyo para validarlos** (ver §9).

## 2. Estado del firmware (rama `main`, código real)

| Tema | Hallazgo |
|---|---|
| Versión real de Marlin | **2.1.2.10** (`Marlin/src/inc/Version.h:28`). `MARLIN_HEX_VERSION 02010206` es la versión de configuración, no del firmware. |
| Configuración | `CANCEL_OBJECTS` y `CANCEL_OBJECTS_REPORTING` activos (`Configuration_adv.h:3881-3883`). Cama 300×300, mín. 0/0, `X_HOME_DIR`/`Y_HOME_DIR` −1, `INVERT_X_DIR`/`INVERT_Y_DIR` true. |
| Motor de cancelación | `Marlin/src/feature/cancel_object.{h,cpp}` + `gcode/feature/cancel/M486.cpp` (Marlin estándar + extensión Arquis `note_extrusion`). **Ya existe y funciona; no se creará un segundo motor.** |
| Semántica de M486 (verificada en código) | `T[n]` reinicia (limpia cancelados y cajas) y fija el total · `S<i>` activa objeto `i` (`S-1` = no cancelable) · `P<i>` cancela `i` · `U<i>` restaura · `C` cancela el activo. Se reinicia también en `startOrResumeJob()` (`MarlinCore.cpp:352`) si no hay pausa. |
| Cómo se omite un objeto | `skipping=true` → (1) `get_destination_from_command()` fuerza `destination[i]=current_position[i]` para **todos los ejes, Z incluido** (`gcode.cpp:185-192`); (2) el planificador pone `position.e = target.e` (`planner.cpp:3050`). |
| Límite del motor | `uint32_t canceled` → **32 objetos** (0‑31); `object_count`/`active_object` son `int8_t`. |
| Mapa actual | Aprende la caja XY de cada objeto **mientras se imprime** (`note_extrusion` desde `G0_G1.cpp:67-71`, arrays `int16_t[32]`×4 = 256 B). No conoce posiciones antes de imprimir cada objeto. |
| `ARQ_OBJ_MAX 16` | Solo es el tope del **modelo de la vista** (`arquis_objects_view.h:36`): arrays `x0..y1[16]`; la rejilla de fichas es 4×4 fija. No es un límite de Marlin. |
| Vista | `arquis_objects_view.cpp` (269 líneas): **6 objetos LVGL fijos** (cabecera 2, mapa 1, fichas 1, botón 1), dibujo por `design_cb`; escala uniforme `dim = max(bed_x,bed_y)`; Y invertida (fondo = frente). Confirmación antes de cancelar ya existe (`draw_cancel_object.cpp:85-93`). |
| Lectura de SD / miniaturas | `preview_gcode_prehandle()` lee **solo los primeros 512 B** de `public_buf[513]` y busca `;simage:`; `gcode_preview()` busca `;gimage:` por bloques de 400 B (`draw_ui.cpp:580-692`). **Cualquier manifiesto no puede desplazar `;simage:` fuera de los primeros 512 B.** |
| Segundo manejador de archivo | Hay precedente: `MediaFile file; card.diveToFile(...); file.open(curDir, fname, O_READ)` (`draw_ui.cpp:666-673`), sin tocar el archivo que se imprime. |
| Memoria (referencias de la Etapa 6, **no medidas hoy**) | Flash 406 800 B = 85,6 % (68 KB libres); RAM 82 372 B = 62,8 % (incluye heap y pila); heap LVGL 16 KB con ≥ 9,2 KB libres en el peor caso; pantalla de objetos ≈ 1,9 KB. Se **volverán a medir** antes de la Etapa 3. |
| Compilación en este contenedor | **Imposible hoy**: sin PlatformIO ni GCC ARM; el *core* Arduino STM32 se baja de un `.zip` de GitHub y el proxy responde 403 (`curl` → 403). PyPI y `raw.githubusercontent.com` sí funcionan (permite el simulador LVGL con `g++` del PC y PlatformIO desde PyPI, pero no el *core*). La compilación real la tendrá que hacer tu PC o un entorno con acceso. |

## 3. Hallazgos críticos (con evidencia)

### 3.1 Las copias idénticas no se distinguen por nombre
`StartSliceJob.py:503` + `ArcusCommunicationPrivate.cpp:157` + `LayerPlan.cpp:3357`: 50 llaveros = 50 tramos `;MESH:Llavero.stl`. `cura-M486` (`MarlinCancelObject.py`) usa `meshes.index(meshname)`: **todas las copias reciben el mismo índice**. Por tanto **no sirve** para el requisito de 50 llaveros independientes.

### 3.2 No se puede deducir la instancia por el orden de los tramos
`FffGcodeWriter::calculateMeshOrder` ordena por proximidad espacial, por extrusor, y las mallas más bajas desaparecen de las capas altas. Ni "k-ésima aparición del nombre" ni orden de escena son fiables.
**Estrategia verificable propuesta (se prueba en Etapa 1/2):** para cada tramo `;MESH:` se calcula la caja XY de sus extrusiones (en capas donde solo aparece ese tramo) y se asigna a la instancia de la escena cuya caja la contiene (tolerancia ≈ 1 línea de extrusión). Asignación **uno a uno**; si hay solapes o ambigüedad → la impresión se marca **"no verificable"** y se bloquea la cancelación visual (nunca se inventa la correspondencia).
Casos que degradan a "no verificable": mallas unidas por Cura (`meshfix_union_all`), objetos solapados, modo secuencial con cajas ambiguas, grupos (un grupo de Cura = varias mallas).

### 3.3 El motor de Marlin: límite de 32 y defecto de desbordamiento (medido)
Prueba de laboratorio con el `cancel_object.cpp` **real** (emulando `M486.cpp`: `set_active_object(parser.value_int())`, conversión implícita a `int8_t`):

```
M486 S31  -> activo 31  | P31 cancela: sí
M486 S32  -> activo 32  | P32 cancela: NO   (el objeto 32 se imprime siempre)
M486 S128 -> activo -128 (se trata como "sin objeto")
M486 S256 -> activo 0    | P256 CANCELA EL OBJETO 0   ← desbordamiento
M486 S300 -> activo 44
M486 T200 -> object_count = -56
M486 T300 -> object_count = 44
```
Consecuencias: (a) hoy **solo 0‑31 son cancelables**; los demás imprimen sin avisar; (b) con índices ≥ 128 hay comportamientos erróneos y con ≥ 256 un índice puede **colisionar con otro objeto** → cancelación equivocada. Esto invalida "subir ARQ_OBJ_MAX" como solución. (El laboratorio vive en el scratchpad de la sesión, no en el repositorio; se convertirá en prueba en Etapa 3.)

### 3.4 Z ignorado dentro de un objeto cancelado [NO VERIFICADO con G-code real]
Por `gcode.cpp:185-192`, un `G0/G1 X Y Z` dentro de un tramo omitido no mueve **ningún** eje. CuraEngine escribe el movimiento "subir a la siguiente altura de capa" como último viaje de la capa (`LayerPlan.cpp:3370-3379`) y el motor cree que ya está arriba. Si ese viaje cae dentro de un tramo cancelado, la capa siguiente podría imprimirse a **Z incorrecta**. `cura-M486` lo menciona como "very slow travel / z-hop fix". Hay que confirmarlo con G-code real y, si se confirma, la Etapa 1 lo trata en el post-procesado (separar el componente Z a un `M486 S-1` + `G1 Z..` previo) o, si no basta, con un cambio mínimo y acotado en `get_destination_from_command()` (omitir solo X/Y/E, no Z). **No se tocará el planificador.**

### 3.5 Otros puntos de Marlin a tener en cuenta
- Soportes y torre de purga salen como `NONMESH` (no cancelables): al cancelar un objeto, su soporte seguirá imprimiéndose **[NO VERIFICADO]**. Se documenta; no se atribuye soporte a un objeto sin evidencia.
- La cancelación afecta a lo que **aún no está en la cola**: los ≤ `BUFSIZE` (4) comandos ya analizados terminan de ejecutarse.
- Recuperación tras corte (`POWER_LOSS_RECOVERY`): el conjunto de cancelados **no se guarda**; un objeto cancelado volvería a imprimirse tras reanudar. Limitación documentada.
- `M486 T` reinicia el conjunto cancelado: el plugin lo emite **una sola vez**, antes de la primera capa.

## 4. Arquitectura definitiva

Se mantiene la propuesta del enunciado con dos ajustes: (1) el núcleo son **motores Python puros, sin importar Cura** (se prueban con `pytest` sin Cura); (2) `ARQUIS_M486.py` es un script fino que llama a esos mismos motores (no duplica lógica).

```
ARQUIS-Cura/
├── plugins/ARQUISPlugin/            # Extension de Cura (API 8)
│   ├── plugin.json  __init__.py  ARQUISPlugin.py
│   ├── adapters/                    # ÚNICA capa que importa UM.* / cura.*
│   │   ├── cura_scene_adapter.py    #   escena → [InstanceSnapshot]  (nombre, bbox, rot, huella)
│   │   ├── cura_gcode_adapter.py    #   gancho a writeStarted / gcode_dict
│   │   └── cura_profile_adapter.py  #   cama, origen, firmware destino
│   ├── engines/                     # Python puro, sin Cura
│   │   ├── object_engine.py         #   modelo de instancias, IDs estables
│   │   ├── gcode_scan_engine.py     #   tramos ;MESH:, cajas XY por tramo, Z‑top
│   │   ├── matching_engine.py       #   tramo ↔ instancia (uno a uno, con ambigüedad)
│   │   ├── m486_engine.py           #   inserta M486 T/S, sin duplicados
│   │   ├── manifest_engine.py       #   genera/valida ARQUIS_OBJECTS_V1 (+CRC)
│   │   ├── bed_map_engine.py        #   coordenadas → mapa (para la vista de Cura y para pruebas)
│   │   ├── validation_engine.py
│   │   ├── calibration_engine.py    #   (Etapa 4)
│   │   └── nonplanar_engine.py      #   (Etapa 5, desactivado por defecto)
│   ├── schemas/arquis_objects_v1.schema.json
│   ├── ui/  (Main/ObjectManager/BedPreview/Settings .qml)
│   └── tests/
├── scripts/ARQUIS_M486.py, ARQUIS_Validation.py   # PostProcessing → engines
├── profiles/MKS_Robin_Nano_XQ1/
├── docs/ (protocol.md, installation.md, compatibility.md, orientation_test.md)
└── README.md
```
Flujo de datos (diagrama de comunicación):

```
 Cura (escena)                      CuraEngine                      Plugin ARQUIS                         Firmware XQ1
 ───────────────                    ───────────                     ──────────────                        ─────────────
 instancias (nombre, bbox, rot) ──► StartSliceJob ──► ;MESH:nombre ► gcode_list (capas)
        │                                                                 │
        └───────── snapshot de escena (slicingStarted/writeStarted) ──────┤
                                                                          ▼
                                            gcode_scan_engine: tramos, cajas XY, Z‑top
                                            matching_engine : tramo ⇄ instancia  (¿ambiguo? → "no verificable")
                                            m486_engine     : M486 T<N> + M486 S<id> / S-1
                                            manifest_engine : bloque ;ARQUIS_OBJECTS_BEGIN…END + CRC32
                                                                          ▼
                                                         .gcode (autosuficiente) ──SD──►  parser en streaming (512 B)
                                                                                           tabla compacta (~12 B/obj)
                                                                                           verifica: N == M486 T, caja aprendida ⊆ caja manifiesto
                                                                                           mapa proporcional + lista virtualizada
                                                                                           toque → confirmar → M486 P<id> (motor existente)
```
Correspondencia exigida: **Instancia Cura → ID ARQUIS → tramo G‑code → `M486 S<id>` → marcador TFT**. Cada eslabón se valida; si uno falla, la cancelación visual se bloquea con motivo explícito y la impresión sigue siendo normal.

## 5. Especificación del manifiesto `ARQUIS_OBJECTS_V1`

**Transporte:** comentarios ASCII del propio G‑code (autosuficiente). Líneas ≤ 120 B. Marlin ignora los comentarios (modo `;` de la cola SD), igual que ya hace con `;gimage`.
**Unidades:** enteros en **0,1 mm**, en el **sistema de coordenadas del G‑code** (el de `G1`/`M486`), sin flotantes en el firmware.
**Ubicación (propuesta, a confirmar):** bloque al **final** del archivo + línea marcadora en la cabecera.
Razón: `;simage:` debe seguir dentro de los primeros 512 B, y tras la miniatura hay ~80 KB de `;gimage`. Con el bloque al final, el firmware hace `seek` a los últimos ≤ 1 KB, localiza `;ARQUIS_TRAILER`, salta al bloque y lo lee en trozos de 512 B por un segundo manejador de archivo (sin RAM extra ni tocar la lectura de la impresión). Alternativa B: bloque tras la miniatura con barrido secuencial (más simple, más lento y frágil con otras cabeceras).

```
;ARQUIS_OBJECTS=1 PID=3FA9C21B N=50                 ← cabecera, ≤ 40 B, DENTRO de los 512 B iniciales (detección rápida)
...
;ARQUIS_OBJECTS_BEGIN
;ARQ:H V=1 PID=3FA9C21B N=50 BED=3000,3000 MIN=0,0 ORG=FL U=0.1mm CAP=256
;ARQ:M 0 Llavero                                    ← tabla de nombres (≤ 24 B, UTF‑8)
;ARQ:M 1 Soporte
;ARQ:O 0 0 0 350 420 185 240 0 R 1500             ← id nombre inst cx cy ancho prof rot tipo ztop
;ARQ:O 1 0 1 1100 420 185 240 0 R 1500
;ARQUIS_OBJECTS_END CRC=1A2B3C4D
;ARQUIS_TRAILER OFF=0001234567 LEN=00003210 CRC=1A2B3C4D PID=3FA9C21B   ← última línea del archivo (campos de ancho fijo)
```
- `id` = índice `M486 S<id>` (0…N‑1, contiguos, único por instancia). `inst` = 0..k del mismo modelo. `tipo`: `C` círculo, `S` cuadrado, `R` rectángulo (derivado de la huella convexa; solo referencia visual). `rot` en grados (informativo). `ztop` = altura superior del objeto (0,1 mm) para el estado "Completado".
- `CRC` = CRC‑32 de los bytes entre `BEGIN` y `END` (sin incluir esas líneas). `PID` = CRC‑32 de (nombres+cajas+cama) → identifica la impresión.
- Tamaño ≈ 35 B/objeto: 50 → ≈ 1,8 KB; 200 → ≈ 7 KB.
- `CAP` = capacidad de cancelación que el plugin supone del firmware; los objetos con `id ≥ CAP` **no reciben `M486 S`** (van como `S-1`) y se publican con estado inicial "No disponible".
- **Validaciones (plugin y firmware):** versión desconocida → ignorar; `N` ≠ filas; IDs duplicados o fuera de rango; coordenadas fuera de cama; `CRC`/`PID` no coinciden; trailer sin bloque. Cualquier fallo ⇒ sin mapa ni cancelación visual, impresión normal intacta.
- **Verificación en ejecución (coste cero en el camino caliente):** al abrir la pantalla, el firmware compara `N` con el `M486 T` recibido y que la caja XY **aprendida** de cada objeto (`note_extrusion`, ya existente) esté dentro de la caja del manifiesto (+margen). Si no coincide ⇒ `UNTRUSTED`: se bloquea la cancelación visual y se vuelve al modo clásico (lista numerada, sin posiciones inventadas).
- Esquema JSON Schema (`arquis_objects_v1.schema.json`) en el plugin para la validación del lado Cura.

## 6. Orientación y coordenadas (sin invertir ejes por intuición)

| Elemento | Valor / regla | Estado |
|---|---|---|
| Origen G‑code | esquina **frontal‑izquierda** de la placa de Cura (`machine_center_is_zero=false`) | verificado en código de Cura/Engine |
| Cama XQ1 | 0…300 × 0…300 (`Configuration.h:1727-1735`), `MIN` = 0,0 | verificado en configuración |
| Mapa actual | X → derecha; Y hacia arriba; "Frente" abajo | verificado en `arquis_objects_view.cpp` |
| ¿Y = 0 es el **frente físico** de tu máquina? | depende de cómo está montada y de cómo la defines en el perfil de Cura | **[NO VERIFICADO]** |

**Prueba de orientación física** (Etapa 2, archivo `orientation_test.gcode` + manifiesto): 5 cuadrados de 20 mm en frontal‑izq., frontal‑der., centro, posterior‑izq., posterior‑der. numerados 1‑5 y una "L" de referencia; se imprime con la cama a la vista y se compara con el mapa de la TFT. Hasta pasar esa prueba el perfil se declara `orientation: unverified`. El modelo ya incluye el parámetro (`ORG`) y el firmware puede reflejar ejes sin cambiar el protocolo.

## 7. Plan de capacidades (cinco capacidades separadas)

| Capacidad | Hoy | Propuesta |
|---|---|---|
| Motor Marlin (cancelar) | 32 (`uint32_t`), `int8_t`, defecto ≥128 | `CANCEL_OBJECTS_MAX` configurable (propuesta **256**), máscara `uint8_t[N/8]` (32 B), índices `int16_t`; **rechazar** (no envolver) todo índice fuera de rango y avisar |
| Metadatos (manifiesto) | no existe | tabla en RAM: 12 B/obj + 16 nombres×24 B ≈ **3 KB para 200** (estimación; se medirá) |
| Visualización | 16 (fichas 4×4) | todos los de la tabla: mapa (un solo `design_cb`, sin widget por objeto) + **lista virtualizada** (solo filas visibles, 5‑6) |
| Selección | por pulsación en ficha/rect | por ID (lista) y por toque en mapa con tolerancia; zoom ×1/×2/×4 con desplazamiento; objetos solapados se seleccionan desde la lista |
| Cancelación real | 0‑31 | 0‑(N_MAX‑1); el resto aparece "No disponible" |

LVGL: se conservan los 6 objetos fijos (+2 botones de zoom/desplazamiento); heap esperado ≈ 2‑2,5 KB (se medirá en el simulador y en la placa). No se añade widget por objeto.

## 8. Archivos

**Firmware — se modificarán (Etapa 3, commits pequeños):** `Marlin/src/feature/cancel_object.{h,cpp}` (capacidad y rangos), `Marlin/src/gcode/feature/cancel/M486.cpp` (rechazar índices fuera de rango), `Marlin/src/lcd/extui/mks_ui/arquis_objects_view.{h,cpp}` (modelo/lista/zoom), `draw_cancel_object.cpp` (modelo desde la tabla), punto de enganche al iniciar el trabajo para leer el manifiesto; **nuevos:** `arquis_manifest.{h,cpp}` (parser en streaming + tabla + CRC), pruebas en `herramientas/pruebas/`. Posible cambio mínimo en `gcode.cpp:185-192` solo si el punto 3.4 se confirma.
**No se tocan:** planificador, stepper, temperatura, `G0_G1.cpp` (el gancho `note_extrusion` se conserva tal cual), cola/lectura de SD durante la impresión, LVGL, `Configuration.h` (salvo un `#define` de capacidad en `Configuration_adv.h` si se aprueba), perfiles existentes.
**ARQUIS‑Cura (repositorio nuevo):** todo lo del §4.

## 9. Riesgos

| # | Riesgo | Impacto | Mitigación |
|---|---|---|---|
| R1 | Copias idénticas no distinguibles | Cancelar la pieza equivocada | Emparejamiento por geometría uno a uno; ambigüedad ⇒ bloqueo con motivo |
| R2 | Z perdido en tramo cancelado (§3.4) | Capas a altura errónea | Confirmar con G‑code real; separar Z en post‑procesado; fallback: cambio mínimo en Marlin |
| R3 | Índices ≥ 128/256 en Marlin (§3.3) | Cancelación equivocada | Rechazar fuera de rango en Marlin y en el plugin (`CAP`) |
| R4 | Sin Cura/CuraEngine ni compilación ARM aquí | Pruebas solo parciales | Motores puros + `pytest`; G‑code real aportado por ti; compilación y pruebas físicas en tu equipo |
| R5 | Otro script de post‑procesado altera el G‑code después | Manifiesto/offsets obsoletos | Trailer con búsqueda en los últimos 1 KB, CRC, bloque sin offsets absolutos entre secciones; `ARQUIS_Validation.py` como último paso |
| R6 | Flash 85,6 % | Falta de espacio | Parser + vista estimados 6‑9 KB de 68 KB; se mide antes y después |
| R7 | Heap LVGL (≥ 9,2 KB libres) | Reinicios | Sin widgets por objeto; medida en el simulador con LVGL real |
| R8 | Lectura de SD del manifiesto durante una impresión | Parones | Se lee **una vez**, al iniciar el trabajo, con segundo manejador; el trailer son ≤ 1 KB + bloque ≤ 7 KB |
| R9 | PLR no conserva cancelados | Objeto cancelado reaparece tras un corte | Documentado; posible mejora futura |
| R10 | Licencia de `cura-M486` | Contaminación de licencia | No se copia código; reimplementación independiente; se acredita como referencia |
| R11 | Miniaturas MKS (`;simage`) dependen de los primeros 512 B | El manifiesto las rompería | Cabecera de ≤ 40 B y bloque al final |
| R12 | Soportes/torre de purga son `NONMESH` | Soporte de objeto cancelado se imprime | Documentado; no se atribuye sin evidencia |

## 10. Plan de pruebas

**Plugin (pytest, sin Cura):** 3 objetos distintos · 10 · 50 iguales · 100 · 200 (superando `CAP` ⇒ "No disponible") · instancias duplicadas (mismo nombre, posiciones distintas) · orden de tramos permutado · objetos solapados (⇒ bloqueo) · torre de purga/skirt/brim (`NONMESH`) · retracciones y z‑hop · M486 sin duplicados ni índices inválidos · manifiesto: esquema, CRC, versión, IDs duplicados, fuera de cama, truncado, corrupto · validador G‑code↔manifiesto · idempotencia (procesar dos veces) · conservación de todos los parámetros originales.
**Cura real (en tu PC):** Cura 5.11.0 arranca con el plugin; la interfaz original intacta; escena con copias ("Multiplicar") → IDs y posiciones esperados; G‑code final.
**Firmware (host, `g++`):** parser en streaming con trozos de 512 B y límites (truncado, CRC, N=0, N=200, líneas largas) · motor de cancelación ampliado: 0…N‑1, −1, N, 127, 128, 255, 256, 300 (el laboratorio de §3.3 pasa a ser regresión) · simulador LVGL real: 3/10/50/100/200 objetos, 300 ciclos de abrir/cerrar sin fugas, heap medido, selección, zoom, confirmación · compilación completa con PlatformIO en tu PC: Flash/RAM antes y después.
**Físicas (solo las dará por válidas tu confirmación):** orientación con 5 piezas; 50 llaveros idénticos identificados uno a uno; seleccionar una pieza en la TFT, confirmar su ID y comprobar que Marlin cancela exactamente esa; verificar Z tras cancelar el último objeto de una capa; sin reinicios ni bloqueos, temperaturas estables.

## 11. Propuesta para la Etapa 1 (ARQUIS M486 Engine, solo en `ARQUIS-Cura`)

1. Crear el repositorio `ARQUIS-Cura` (estructura vacía + docs + `pytest`) — **sin tocar el firmware**.
2. `gcode_scan_engine` (tramos `;MESH:`, cajas XY, Z‑top, detección de `NONMESH`) y `matching_engine` (uno a uno, ambigüedad explícita).
3. `m486_engine`: `M486 T<N>` una vez tras `;LAYER_COUNT:`; `M486 S<id>` en cada `;MESH:` verificado; `M486 S-1` en `NONMESH`; sin duplicados; idempotente; separación del Z si R2 se confirma.
4. `scripts/ARQUIS_M486.py` (PostProcessing) como adaptador fino.
5. Pruebas de §10 con fixtures sintéticos + los G‑code reales que aportes.
**Qué necesito de ti para validar la Etapa 1:** (a) un G‑code real de Cura 5.11 con 3 objetos distintos; (b) otro con 4 copias del mismo STL + brim/soporte/torre si los usas; (c) tu perfil de máquina de Cura (`.curaprofile`/definición) y cómo está orientada físicamente la cama.

## 12. Decisiones que necesito de ti

1. **Ubicación del manifiesto:** ¿bloque al final + marcador en cabecera (recomendado) o tras la miniatura?
2. **Capacidad de cancelación en Marlin:** ¿256 (recomendado; ≈ 40 B de RAM) u otro valor?
3. **Licencia del plugin:** ¿LGPL‑3.0‑or‑later (compatible con Cura y con el script de post‑procesado de Cura) o GPL‑3.0?
4. **Repositorio `ARQUIS-Cura`:** no tengo permiso para crear repositorios en GitHub desde esta sesión. ¿Lo creas vacío y me lo añades, o lo preparo en local y lo subes tú?
