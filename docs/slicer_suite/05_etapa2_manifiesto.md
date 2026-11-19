# Etapa 2 — Gestor de objetos y manifiesto `ARQUIS_OBJECTS_V1` (módulo de Cura)

Estado: **implementado y probado en el PC** (G-code real de Cura 5.11 incluido). **No probado dentro de Cura** (la ventana del gestor y la API de escena) **ni en la impresora**. No se ha tocado `Marlin/`, `platformio.ini` ni `ini/`. Capacidad de diseño: 30 objetos (`04_cambio_capacidad_30.md`).

Supuestos que tomé por defecto (las dos decisiones que quedaron sin responder en la auditoría): **manifiesto al final del archivo con marca en la cabecera** (con G-code real se comprobó que Cura añade después su `;SETTING_3`, de ahí la ventana de 16 KB) y licencia **sin decidir** (no se ha añadido ningún archivo de licencia).

## 1. Qué hay
| Pieza | Archivo | Función |
|---|---|---|
| Captura de la escena | `adapters/cura_scene_adapter.py` | posición real de cada instancia (cajas en coordenadas de G-code), **giro**, **forma** (círculo/cuadrado/rectángulo por la huella convexa), **altura**, grupos, cama de la impresora |
| Objetos e IDs | `engines/object_engine.py` | `split_name` (Cura renombra copias `nombre(n)`: nombre base + nº de copia), `classify_shape`, IDs contiguos 0–29 |
| Mapa de cama | `engines/bed_map_engine.py` | coordenadas→píxeles, tamaño y forma de marcadores, zoom, selección, solapes, estados — **aritmética entera**, la misma que hará el firmware |
| Manifiesto | `engines/manifest_engine.py` | generar, serializar (con CRC), leer con la ventana de búsqueda del firmware y validar |
| Sincronía G-code↔manifiesto | `engines/validation_engine.py` | `gcode_object_stats`, `validate_manifest_vs_gcode` |
| Integración | `engines/m486_engine.py` | tras el M486 verificado, mide cada objeto **en el G-code ya procesado**, genera el manifiesto, lo relee y lo contrasta; si algo falla lo **omite** (el M486 sigue) |
| Gestor de objetos (Cura) | `ui/` + `ARQUISPlugin.py` | menú **ARQUIS ▸ Gestor de objetos…**: mapa de la cama y lista **sincronizados** |
| Prueba de orientación | `engines/orientation_test.py`, `tools/make_orientation_gcode.py` | 5 piezas en las 5 zonas, con M486 y manifiesto |
| Comprobador de archivos | `tools/arquis_check.py` | verifica un `.gcode` ya exportado como lo vería el firmware |
| Contrato | `shared/object_protocol/` | `ARQUIS_OBJECTS_V1.md`, `limits.json`, esquema JSON, `test_vectors/` (417 casos), cabecera C++ generada |

## 2. Cumplimiento de los requisitos
- **Posición proporcional a la cama física**: `to_px` con escala única; ejemplo del enunciado verificado; **nunca se mueve un objeto** (el marcador mínimo de 22 px crece alrededor de su centro real). Orientación: X→derecha, **Y→arriba (atrás)**, frente abajo, si `ORG=FL`. Regla única, probada con la prueba de 5 zonas (`test_orientation_test_pieces_are_in_the_five_zones_and_the_map_agrees`).
- **Copias idénticas independientes**: 30 copias de «Llavero» → 30 entradas (`Llavero 1…30`), un solo nombre en la tabla, IDs 0–29, posiciones propias. Con G-code real: las 4 copias de `pinon.stl` salen como `pinon 1…4` con 4 posiciones distintas.
- **Lista ↔ mapa sincronizados**: un único índice de selección en el backend; seleccionar en la lista resalta el marcador y al revés (probado con Qt real). *Ficha, confirmación de cancelación, zoom del mapa y lista virtualizada son de la pantalla de la impresora (Etapa 3)*; en Cura el mapa es una vista previa sin zoom.
- **Estados**: Pendiente / En impresión / Completado / Cancelado / No disponible, con **texto e insignia** (`""`, `>`, `+`, `x`, `?`), no solo color. Reglas en el protocolo; en Cura solo se ven Pendiente y No disponible (no hay impresión).
- **Capacidad**: más de 30 objetos → los sobrantes no se publican ni se numeran; Cura avisa. IDs fuera de 0–29 se rechazan en el esquema, el parser y el validador.
- **Integridad**: CRC-32, PID, trailer, cabecera, N, IDs, coordenadas, versión, cama, órdenes, Z, nombres; sincronía con el G-code (mismo `M486 T`, mismos ids, extrusiones dentro de su caja, mismas Z y orden). **26 vectores** de manifiesto (válidos, G-code antiguo, 15 tipos de corrupción) que el firmware debe reproducir.
- **Compatibilidad**: G-code sin manifiesto o con manifiesto inválido imprime normalmente; un firmware sin parser ignora los comentarios.

## 3. Hallazgos de esta etapa con G-code real
1. **Los archivos exportados en Windows llevan CRLF.** El CRC y el parser se definen sobre líneas normalizadas, sin longitudes ni offsets en bytes (se probó el mismo archivo con LF y CRLF).
2. **Cura añade el bloque `;SETTING_3` después de lo que genera un plugin** (lo escribe `GCodeWriter` al exportar). Por eso el firmware debe buscar el manifiesto en una ventana final de 16 KB (el de los archivos reales mide 1,1 KB). Si algún día el perfil fuese enorme (> ~13 KB) el bloque saldría de la ventana: se degrada a «sin mapa», nunca a un error; `tools/arquis_check.py` lo detecta en el archivo exportado.
3. **El perfil de Cura de los G-code de ejemplo (CR-10 Max, 450 × 450 mm) no es el de tu máquina (300 × 300).** Un mapa con la cama equivocada pondría los marcadores en otro sitio: el script compara con la cama del firmware (ajustable, por defecto 300 × 300) y, si no coincide, **omite el mapa** conservando el M486; el firmware repetirá la comprobación.

## 4. Pruebas (Python 3.12; también 3.13)
| Resultado | Condición |
|---|---|
| **196 pasan, 2 omitidas** | `cd cura && python -m pytest tests` |
| **198 pasan** | además `ARQUIS_REAL_GCODE_DIR=<carpeta con los 2 G-code completos>` |
| 193 pasan, 5 omitidas | Python 3.13 sin PyQt6 (3 de interfaz) y sin los archivos completos |
Contenido nuevo de esta etapa: manifiesto (24), mapa (14), vectores compartidos (3), esquema (12), interfaz Qt (3: QML sin avisos, sincronía lista↔mapa, 30 copias, imágenes), prueba de orientación y herramientas (3), adaptador (+3), scripts (+1). Las imágenes de la interfaz, generadas por el propio test: `docs/slicer_suite/img/gestor_objetos_estados.png` y `gestor_objetos_30_iguales.png`.
Además: `python3 shared/object_protocol/gen_limits.py --check`, `python3 cura/tools/gen_shared_vectors.py --check`, `tests/capacity/run.sh`.

## 5. Limitaciones y lo que NO está verificado
1. **No se ha ejecutado dentro de Cura**: ni `ObjectManager.qml` (`UM.Dialog`, `Cura.PrimaryButton`…), ni el menú, ni `capture_scene()`/`capture_bed()`, ni la llamada del script. El contenido de la ventana (`ObjectManagerView.qml`, `BedPreview.qml`) sí se cargó con Qt 6.12 real sin avisos; el envoltorio de Cura solo se ha revisado contra el patrón del propio plugin de post-procesado de Cura 5.11.
2. **El firmware no lee nada todavía** (Etapa 3). Los vectores compartidos y la cabecera C++ están preparados.
3. **Orientación física sin verificar.** Hasta que imprimas la prueba de orientación, `ORG=FL` es un supuesto sobre tu máquina. Pasos: `python3 cura/tools/make_orientation_gcode.py --bed 300x300 --out prueba_orientacion.gcode`, imprímela, mírala **desde el frente** y comprueba: la pieza más pequeña (20 mm) en la esquina frontal izquierda, 22 mm frontal derecha, 24 mm centro, 26 mm posterior izquierda, 28 mm posterior derecha. Si no coincide, hay que reflejar el eje en `ORG`, no «arreglarlo» en los datos.
4. Las pruebas con G-code real derivan la «escena» del propio G-code (no ejercitan la correspondencia con la escena real de Cura).
5. La forma (círculo/cuadrado/rectángulo) es una referencia visual por la huella de Cura; en impresión secuencial la huella de Cura incluye el espacio del cabezal, así que puede salir algo más grande.
6. `z0/ztop` en los G-code **reducidos** del repositorio son los de las capas conservadas, no los del archivo completo (con los completos salen correctos y las pruebas pasan).
7. Soporte de árbol y brim siguen siendo NONMESH: no se cancelan con su objeto.

## 6. Cómo validarlo
1. `cd cura && pip install -r requirements-dev.txt && python -m pytest tests` (opcional: `pip install PyQt6` para las pruebas de interfaz).
2. Instalar en Cura 5.11 (`cura/README.md`): menú **ARQUIS ▸ Gestor de objetos…** con 30 copias de un STL (Multiplicar) → mapa y lista; selecciona en uno y mira el otro.
3. Post-procesado **ARQUIS M486** con «Incluir el mapa de objetos» (cama del firmware 300 × 300): exporta y ejecuta `python3 cura/tools/arquis_check.py archivo.gcode --bed 300x300` → debe decir `manifiesto: OK` y `CORRECTO`.
4. Prueba de orientación física (arriba).
5. Cambia el perfil a otra cama (p. ej. 450 × 450) y comprueba que el aviso omite el mapa pero deja el M486.
