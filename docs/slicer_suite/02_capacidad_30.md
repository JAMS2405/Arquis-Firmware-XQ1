# Capacidad de 30 objetos — estudio de viabilidad (sustituye al de 64)

Decisión del propietario (Enmienda 2): **30 objetos simultáneos de diseño, IDs cancelables 0–29**. Ya no hay objetivo de 64 ni de 256. Este documento reemplaza a `02_capacidad_64.md` (renombrado). **No se ha modificado ningún archivo de `Marlin/`, `platformio.ini` ni `ini/`.**

Leyenda: **[MEDIDO]** ejecutado con código real o prototipo (`tests/capacity/run.sh`, `cura/tests/`) · **[CALCULADO]** derivado de `sizeof` medidos · **[REFERENCIA]** medida previa del repositorio (Etapa 6), no repetida · **[PENDIENTE]** requiere compilar para la placa (no es posible en esta sesión: sin PlatformIO ni GCC ARM, y el *core* Arduino STM32 es un `.zip` de GitHub que el proxy bloquea).

## Veredicto

1. **La máscara `uint32_t` de Marlin es segura y suficiente para 30 objetos.** El motor real (el `cancel_object.cpp` y el `M486.cpp` del repositorio, compilados **sin modificar** en el PC) cancela y restaura correctamente los 32 índices 0–31, y por tanto los 30 de diseño. Se mantiene la máscara; **no se implementa ninguna de 64 bits ni se amplía ninguna estructura del motor.**
2. **Sí hay un defecto en el motor actual que hay que cerrar** (Etapa 3, fuera de la capacidad): índices ≥ 32 no se rechazan; se truncan a `int8_t` y pueden coincidir con otro objeto. Con 30 objetos no se alcanza desde el G-code de ARQUIS (el plugin nunca emite índices ≥ 30), pero un G-code ajeno u hostil sí podría. El rechazo seguro está **diseñado y probado en prototipo**; en el firmware **no está implementado** (no se ha tocado `Marlin/`).
3. **Lo que NO se afirma:** que el firmware *de la placa* soporte 30 objetos. Está validado el **código del motor M486 compilado en el PC**; falta compilar y probar en la MKS Robin Nano V3.1.

## Resumen

| # | Comprobación | Resultado | Evidencia |
|---|---|---|---|
| 1 | Motor M486 real, IDs 0–29 | Correcto: `S`, `P`, `U`, `C`, `T30` y cancelación de cada uno de los 30; el G-code ARQUIS completo se ejecuta igual en el motor real que en el modelo (30 objetos × 2 variantes de G-code) | [MEDIDO] `cura/tests/test_real_marlin_engine.py` |
| 2 | Máscara y macros de bits | `uint32_t` + `TEST/SBI/CBI` (`1 << b`, `int` de 32 bits): con índices ≤ 29 no hay desplazamiento a signo ni UB | [MEDIDO] prototipo + lectura de `core/macros.h:115-122` |
| 3 | Contadores | `object_count`/`active_object` `int8_t` (−1…127): 30 cabe con holgura | lectura de código |
| 4 | Rechazo de índices fuera de rango | **Motor actual: NO** (S256 → objeto 0). Prototipo con rango validado y máscara `uint32_t`: 0 errores sobre los 65 536 valores de `int16_t` (196 608 comprobaciones) | [MEDIDO] |
| 5 | Estructuras | Registro de objeto 14 B; tabla de 30 objetos + 30 nombres = **1 054 B**; el estado del motor (260 B) **no cambia** | [MEDIDO] `sizeof` |
| 6 | RAM de la MKS Robin Nano V3.1 | ≈ **+1,05 KB** (0,8 % de 128 KB) en el peor caso; antes 62,8 % → ≈ 63,6 % | [CALCULADO] sobre [REFERENCIA]; medición real [PENDIENTE] |
| 7 | Manifiesto `ARQUIS_OBJECTS_V1` | **2 719 B** en el peor caso (30 nombres de 20 B); **1 827 B** con 3 nombres; línea máx. 70 B | [MEDIDO] |
| 8 | LVGL (TS35) | Δ de heap = 0 B al dibujar N marcadores en un único objeto (N = 0…128); heap de la pantalla = sus 6 objetos fijos | [MEDIDO] (resultado abajo) |
| 9 | G-code real de Cura | Los dos G-code reales procesados: +0,45 % y +0,84 % de tamaño; cancelar cualquier objeto es seguro | [MEDIDO] `cura/tests/test_real_gcode.py` |

## 1. Motor M486 real (código de Marlin 2.1.2.10, sin modificar)
`tests/capacity/build_real_engine.sh` compila `feature/cancel_object.cpp` y `gcode/feature/cancel/M486.cpp` tal cual (solo se sustituyen las cabeceras por un *prelude* mínimo; las macros de bits son copia exacta de `core/macros.h`, y una prueba lo comprueba). El analizador de parámetros sustituto devuelve `int16_t` en `value_int()`, como el de Marlin. Resultados con el barrido exhaustivo (`scan_real_engine.py`, 65 536 valores):

```
MOTOR REAL (Marlin 2.1.2.10):
  índices 0..31: 32 correctos, 0 incorrectos; los 30 de diseño (0..29) correctos: True
  fuera de 0..31: P<v> cancela OTRO objeto en 8160 valores; S<v> activa OTRO objeto en 8160; sin efecto en 57344
```
(Los 8 160 incluyen negativos; en la auditoría anterior se contaron solo los positivos, 4 064.) Además, 27 índices hostiles (0, 29, 30, 31, 32, 63, 64, 127, 128, 255, 256, 300, 32767, −1, −128, −256, −32768…) con S/P/U/C/T dan **exactamente el mismo estado** en el motor real y en el modelo `marlin_model.py` (legacy), lo que valida el modelo con el que se prueba el G-code.

**Prototipo endurecido para 30** (`co30_prototype.cpp`, no es el firmware): mantiene `uint32_t` y las macros de Marlin, valida `0 <= v < 30` **antes** de estrechar, fuera de rango ⇒ el objeto no es cancelable y se marca `overflow`. Exhaustivo en `S`, `P` y `U`: 196 608 comprobaciones, 0 errores; `M486 T200` → `object_count = 30`, `overflow = 1`.

## 2. Tipos y contadores
| Dato | Tipo actual | ¿Soporta 30? |
|---|---|---|
| Máscara de cancelados | `uint32_t` | **Sí** (bits 0–29; techo 32) |
| `object_count`, `active_object` | `int8_t` | Sí |
| `parser.value_int()` | `int16_t` | Sí, comprobando rango **antes** de estrechar |
| Cajas aprendidas (`bbox_*[32]`) | `int16_t[32]` ×4 | Sí (ya existen, 256 B) |
| Vista (`arq_obj_model_t`): `canceled`, `has_box`, `ARQ_OBJ_MAX 16` | `uint32_t`, 16 | `uint32_t` basta; `ARQ_OBJ_MAX` pasa a 30 (Etapa 3) |

**Ampliar en el futuro** (requisito: conservar la posibilidad): todo límite sale de `shared/object_protocol/limits.json` (`design_capacity`, `capacity_ceiling`, `protocol_max_objects`). Subir la capacidad de diseño hasta el techo (32) es cambiar un número y regenerar; superar 32 exige cambiar el firmware (máscara de 64 bits) **y** subir `capacity_ceiling`; el motor de Cura admite el nuevo techo sin cambios de código (`M486Options(ceiling=…)`, probado con 50 objetos / techo 64). El límite absoluto del protocolo es 128 (`int8_t`).

## 3. Estructuras y RAM (optimizadas para 30)
```
ArqObjRec  { int16 cx, cy; uint16 w, d, z0, ztop; uint8 name_idx, type_order; } = 14 B   (0,1 mm; la rotación del manifiesto no se guarda)
ArqObjStore{ n, n_names, flags; ArqObjRec[30]; char names[30][21]; }         = 1 054 B  (peor caso: 30 nombres distintos)
Motor: máscara uint32 + cajas aprendidas [32]                                =  260 B  (sin cambios)
```
Optimizaciones respecto al estudio de 64: se descarta la rotación y se empaqueta tipo+orden en un byte (el registro incluye `z0` y `orden` para los estados), nombres de 21 B en vez de 24, y **ninguna ampliación del motor** (−2,7 KB → +1,05 KB). Todo estático, sin asignación dinámica.
[REFERENCIA] Etapa 6: RAM 82 370 B = 62,8 %; Flash 85,6 % (68 KB libres); heap LVGL ≥ 9,2 KB libres en el peor caso. [CALCULADO] +1 054 B ⇒ ≈ 63,6 %; Flash del parser + CRC + vista estimado en 4–6 KB (≈ 86,5–87 %). **Criterio de parada de la Etapa 3:** Flash < 90 %, RAM < 70 %, heap LVGL libre ≥ 8 KB en el peor caso, medidos compilando; si no se cumple, no se integra.

## 4. Manifiesto (`tests/capacity/manifest_size.py`)
Peor caso 2 719 B (30 objetos con 30 nombres de 20 B) con el formato final; típico 1 827 B; línea más larga 70 B. Lectura en streaming con el `public_buf[513]` existente y 96 B de línea: 0 B de RAM adicional aparte de la tabla.

## 5. LVGL (TS35, 480×320) — mapa y lista sincronizados con memoria constante
`tests/capacity/run_lvgl.sh` (LVGL 6.1.1-MKS real, host de 64 bits → conservador): un solo objeto con `design_cb` que dibuja N rectángulos + etiquetas; 40 redibujados completos por caso.
```
N=  0: heap antes=2640, pico=2640 (Δ = 0 B)
N= 16 / 30 / 32 / 128: Δ pico = 0 B en todos; heap final = heap inicial (sin fugas)
```
Conclusión: el heap de la pantalla es el de sus 6 objetos fijos (≈ 1,9 KB en la placa [REFERENCIA]) y **no depende de N**. Mapa y lista comparten **una sola tabla** y un único índice de selección (sin copias): se invalida un único objeto LVGL al cambiar la selección.
Riesgos que se mantienen para la Etapa 3: (1) el modelo actual se copia (pila): con 30 objetos ≈ 130 → ≈ 300 B por copia; el diseño leerá la tabla por puntero; (2) el tiempo de redibujado en la placa no está medido [PENDIENTE]; mitigación: descartar marcadores fuera de la franja antes de dibujar; (3) la rejilla 4×4 y `ARQ_OBJ_MAX 16` se sustituyen por lista virtualizada.

## 6. G-code generado
Medido con **G-code real** de Cura 5.11 (`cura/tests/data`, completos en local): +0,45 % (3 modelos) y +0,84 % (4 copias) de tamaño; ≈ 1 070–1 810 líneas `M486` en 25 capas. Con el G-code sintético (secciones muy cortas) el peor caso fue +15 %.

## 7. Compatibilidad con el firmware actual
El G-code de 30 objetos **funciona con el firmware actual** (índices 0–29 < 32): probado con el motor real compilado en el PC. Si en el futuro se sube el techo en Cura sin ampliar el firmware, los objetos ≥ 32 son inertes (se imprimen) y **nunca se cancela otro** (`test_expansion_gcode_on_current_firmware_is_inert_beyond_31_never_wrong`).
