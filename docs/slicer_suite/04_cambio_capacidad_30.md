# Cambio de requisitos: 30 objetos — revisión de la Etapa 1, cambios y pruebas

Fecha de la decisión: Enmienda 2 del propietario. **No se ha tocado `Marlin/`, `platformio.ini` ni `ini/`.**

## 1. Revisión previa de la Etapa 1 (antes de cambiar código)

| Parte de la Etapa 1 | ¿Cambia? | Motivo |
|---|---|---|
| `shared/object_protocol/limits.json` y `protocol_limits.py` | **Sí** | Capacidad de diseño 30; ahora hay tres valores distintos (diseño 30, techo del motor 32, límite del protocolo 128). `protocol_limits.py` pasa a **generarse** de `limits.json` (antes era una copia a mano con una prueba) |
| `M486Options.capacity` (por defecto 64, tope 128) | **Sí** | Por defecto 30; tope = techo (32) de `limits.json`; ampliar = subir el techo a propósito (`ceiling`), sin cambiar código |
| Scripts de Cura (`capacity` 64, máx. 128) | **Sí** | Por defecto 30, máximo 32; una prueba obliga a que sigan a `limits.json` |
| `marlin_model.py` (capacidad por defecto 64) | **Sí** | Defecto 30; el modelo "motor actual" (legacy) no cambia |
| Pruebas con 50/100/64 objetos | **Se conservan** | 50 y 100 objetos pasan a probar *capacidad excedida* (30 cancelables, el resto se imprime sin cancelar); 50 objetos con techo 64 prueba la **ruta de ampliación**; añadidas 30 copias idénticas (todas cancelables) |
| Pruebas de firmware actual (capacidad 32) | **Reescritas** | Ahora el caso principal es "el firmware actual sirve para los 30 de diseño" |
| Reglas R1–R5, correspondencia geométrica, validador, bloqueos | **No cambian** (ya eran independientes del tamaño) | — |
| `docs/.../02_capacidad_64.md`, `tests/capacity64/` | **Sustituidos** | `02_capacidad_30.md`, `tests/capacity/` |
| Manifiesto: `CAP=64`, registro de 14 B | **Sí** (documentos; se implementa en la Etapa 2) | `CAP=30`, registro de 12 B |

## 2. Hallazgos con G-code real de Cura 5.11 (dos archivos aportados)
Ninguna de las pruebas de la Etapa 1 había visto salida real de CuraEngine. Procesando los dos archivos:

1. **Un fallo real que las pruebas sintéticas no mostraban (corregido).** CuraEngine escribe las retracciones **dentro del tramo del objeto** y como `G0 X.. Y.. E<retraído>` (viaje con filamento). Al cancelar un objeto, esa línea se omitía entera (ni viaje ni retracción), pero la des-retracción posterior (solo E, protegida en `S-1`) sí se ejecutaba: **0,8 mm de filamento de más por cada retracción omitida**. Lo detectó la comprobación de equilibrio de filamento del modelo de Marlin. Corrección: un `G0/G1` con XY y con Z o E que no sea extrusión se parte en "Z/E en `S-1`" + "XY en el objeto" (el orden respeta que primero se retrae y luego se viaja, y que primero se viaja y luego se repone). El escáner dejó de tratar `G0 … E` como extrusión. Nuevas variantes en el generador de pruebas (`travel_e`) y un nuevo código del validador `V_E_IN_TRAVEL_IN_OBJECT`.
2. **Cura renombra las copias.** El G-code trae `;MESH:pinon.stl`, `pinon.stl(1)`, `(2)`, `(3)`: `ObjectsModel._renameNodes` cambia el nombre de los nodos de la escena. **Corrijo la auditoría:** el caso de copias con nombre idéntico es solo un caso límite (p. ej. si la lista de objetos no se ha actualizado), no lo normal. La identificación **sigue verificándose por geometría**: hay una prueba que renombra las 4 copias reales al mismo nombre y comprueba que cada una conserva su ID.
3. **Formato confirmado:** cada capa empieza con contexto implícito NONMESH (brim/soporte de árbol, sin `;MESH:`); `;MESH:NONMESH` solo aparece al final de la capa (viaje + subida de Z); el G-code final del usuario (`G1 Z5`, `G1 X0 Y310`, `M84`) va tras el último `;TIME_ELAPSED:` sin marcador — exactamente lo que el motor ya suponía (R2). En estos archivos **no hay Z dentro de los tramos de objeto** (la subida de capa está en NONMESH; el perfil no usa z-hop), así que R1 para Z no se ejerce con G-code real, pero se mantiene.
4. **Soporte y brim son NONMESH** (no se cancelan con su objeto): confirmado, se documenta como limitación.
5. Sobrecoste del G-code procesado: **+0,45 %** (3 modelos) y **+0,84 %** (4 copias).

## 3. Cambios realizados
- `limits.json` (30 / 32 / 128) + `gen_limits.py` → `protocol_limits.py` y `generated/arquis_object_limits.h` (`ARQ_OBJ_DESIGN_CAPACITY 30`, `ARQ_OBJ_MAX_ID 29`, …, con `static_assert`s para el firmware).
- Motor/scripts/modelo/pruebas actualizados (§1). Motor: partición de movimientos con E, estado de E en la reescritura, escáner.
- `tests/capacity/` (antes `capacity64/`): arnés que compila y ejecuta el **motor M486 real**, barrido exhaustivo, prototipo para 30 con `uint32_t`, tamaños, manifiesto, laboratorio LVGL.
- Documentos: `02_capacidad_30.md`, esta revisión, enmiendas 1 y 2 en la auditoría, READMEs.

## 4. Pruebas ejecutadas (resultado de esta sesión)
| Prueba | Resultado |
|---|---|
| `cd cura && python -m pytest tests` (Python 3.12 y 3.13) | **todas pasan** (ver cifras en el informe final) |
| Motor M486 **real** de Marlin, G-code ARQUIS de 30 objetos (2 variantes), cancelando cada uno: trazas idénticas al modelo | 60 casos OK |
| Motor real: 27 índices hostiles × S/P/U/C/T: idéntico al modelo | OK |
| Barrido exhaustivo del motor real (65 536 valores de `int16_t`) | 0..31 correctos; fuera de 0..31: alias en 8 160 valores (defecto actual, no corregido porque no se toca Marlin) |
| Prototipo endurecido para 30 con `uint32_t` | 196 608 comprobaciones, 0 errores |
| G-code real (3 distintos / 4 iguales): procesado, validador, cancelar cada objeto (modelo + motor real), ambas formas de trocear los datos | OK; también con los archivos completos (2 MB) en local |
| Copias reales renombradas al mismo nombre | cada copia conserva su ID |
| Escena desplazada respecto al G-code real | bloqueo, sin M486 |
| LVGL real: Δ heap al dibujar N marcadores (N hasta 128) | 0 B |

## 5. Lo que NO se afirma
- **Que la placa soporte 30 objetos.** Validado: el código del motor M486 (compilado en el PC). Sin validar: compilación y ejecución en la MKS Robin Nano V3.1, Flash/RAM reales, tiempo de redibujado.
- **Que el firmware rechace índices fuera de rango.** No lo hace hoy (S256 → objeto 0). El plugin y el validador no los emiten y el rechazo está probado en prototipo, pero la corrección real es de la Etapa 3.
- **Correspondencia con la escena real de Cura:** en las pruebas con G-code real la "escena" se deriva del propio G-code; la API de escena sigue sin ejecutarse dentro de Cura.
