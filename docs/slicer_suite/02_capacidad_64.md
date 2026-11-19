# Capacidad de 64 objetos — estudio de viabilidad

Decisión del propietario: **64 objetos** como objetivo inicial (uso habitual ≈ 40 piezas por trabajo). Este estudio comprueba los seis puntos pedidos **antes** de tocar el firmware. No se ha modificado ningún archivo de `Marlin/`.

**Veredicto: 64 es viable de forma segura, pero NO con el motor actual** (el firmware sin modificar cancela como máximo 32). Hace falta un cambio pequeño y acotado en `feature/cancel_object.*` y `M486.cpp` (Etapa 3). No hay motivo técnico para ir más allá de 64, y no se propone.

Leyenda: **[MEDIDO]** = ejecutado hoy con código real o prototipo (reproducible con `tests/capacity64/`) · **[CALCULADO]** = derivado de tamaños de estructura medidos · **[REFERENCIA]** = medida anterior del repositorio (Etapa 6), no repetida hoy · **[PENDIENTE]** = requiere compilar para la placa (no es posible en esta sesión: ni PlatformIO ni GCC ARM; el *core* Arduino STM32 se descarga de un `.zip` de GitHub que el proxy bloquea).

## Resumen

| # | Comprobación | Resultado | Evidencia |
|---|---|---|---|
| 1 | Motor M486 real de Marlin | **No admite 64 hoy**: máscara `uint32_t` (32 objetos) y desbordamiento por estrechamiento a `int8_t`. **Con cambios mínimos, sí** (prototipo exhaustivo sobre los 65 536 valores de `int16_t`: 0 errores) | [MEDIDO] |
| 2 | Tipos de datos y contadores | `object_count`/`active_object` son `int8_t` (rango −1…127): **64 cabe con 63 de holgura**. Hay que usar máscara de 64 bits (`1ULL`, no `_BV`) y validar el rango **antes** de estrechar | [MEDIDO] + lectura de código |
| 3 | Estructuras de almacenamiento | Registro de objeto 14 B; tabla de 64 objetos + 64 nombres = **2 436 B**; máscara + cajas aprendidas = **520 B** | [MEDIDO] (`sizeof`) |
| 4 | RAM de la MKS Robin Nano V3.1 | ≈ **+2,7 KB** en el peor caso (2,1 % de 128 KB): 62,8 % → ≈ 64,9 %; margen ≈ 46 KB | [CALCULADO] sobre [REFERENCIA]; medición real **[PENDIENTE]** |
| 5 | Manifiesto `ARQUIS_OBJECTS_V1` | **5 150 B** en el peor caso (64 objetos con 64 nombres de 20 B); **2 066 B** para 40 piezas / 3 nombres; línea más larga 72 B | [MEDIDO] |
| 6 | Interfaz LVGL de la TS35 | Dibujar N marcadores en un único objeto con `design_cb` **no asigna heap**: Δpico = 0 B para N = 0…128 con LVGL 6.1.1-MKS real. Riesgo identificado: el modelo actual de la vista se **copia** (pila) | [MEDIDO] + análisis |

## 1. Motor M486 real de Marlin 2.1.2.10

Código: `Marlin/src/feature/cancel_object.{h,cpp}`, `gcode/feature/cancel/M486.cpp`. Se compiló el `cancel_object.cpp` **real** con stubs (`tests/capacity64/run.sh`) y se barrió todo el rango que acepta `parser.value_int()` (`int16_t`):

```
MOTOR REAL: de los valores M486 32..32767:
  S<v> acaba activando OTRO objeto 0..31 : 4064 valores
  P<v> cancela un objeto equivocado      : 4064 valores
  P<v> sin efecto (objeto no cancelable) : 28672 valores
```
- **Capacidad real hoy: 32** (índices 0‑31). Un objeto ≥ 32 se imprime siempre y su `M486 P` se ignora en silencio.
- **Defecto de seguridad existente:** `M486.cpp` pasa `parser.value_int()` (16 bits) a `set_active_object(const int8_t)`, que trunca. Con índices ≥ 128 el contexto se corrompe y con índices ≥ 256 un índice puede **coincidir con otro objeto** (`S256` → objeto 0). El plugin de Cura **nunca** emite índices ≥ capacidad (≤ 63), así que no los provoca; pero el firmware debería rechazarlos.

**Prototipo con capacidad 64** (`co64_prototype.cpp`, misma semántica que el motor real; **no** es el firmware): máscara `uint64_t`, rango validado antes de estrechar, fuera de rango ⇒ no cancelable + bandera `overflow`. Resultado exhaustivo: S y P sobre los 65 536 valores de `int16_t` → **131 072 comprobaciones, 0 errores**; cancelar/restaurar 0…63: 0 errores; `M486 T200` ⇒ `object_count = 64`, `overflow = 1`. Estado del motor: 16 B.

Cambios que implicará en el firmware (Etapa 3, no hechos): `canceled` de `uint32_t` a `uint64_t` con operaciones `1ULL<<i` (los macros `TEST/SBI/CBI` de `core/macros.h:115-122` usan `1 << b` de 32 bits: **no sirven** para bits ≥ 32); comprobación de rango en `M486.cpp` y `CancelObject::*`; arrays de cajas aprendidas de 32 a 64 entradas (+256 B).

## 2. Tipos y contadores
| Dato | Tipo actual | ¿Soporta 64? |
|---|---|---|
| `object_count`, `active_object` | `int8_t` (−128…127) | Sí (máx. 127) |
| `parser.value_int()` | `int16_t` | Sí (comprobar rango antes de estrechar) |
| Máscara de cancelados | `uint32_t` | **No** → `uint64_t` |
| `arq_obj_model_t` (vista): `canceled`, `has_box` | `uint32_t` | **No** → 64 bits o puntero a la tabla |
| Contadores del protocolo (`N`, `id`) | — | `id ≤ 127` por el `int8_t` de Marlin; el protocolo fija `CAP = 64` |
El tope absoluto sin cambiar contadores es 127 (`limits.json: max_protocol_index`); 64 deja margen sin ampliar nada innecesariamente.

## 3. Estructuras de almacenamiento (Etapa 3, propuesta) — `tests/capacity64/sizes.cpp`
```
ArqObjRec  { int16 cx, cy; uint16 w, d, ztop, rot; uint8 name_idx, type; }   = 14 B   (unidades de 0,1 mm)
ArqObjStore{ n, n_names, flags; ArqObjRec[64]; char names[64][24]; }        = 2 436 B
Cancelación: uint64 canceled + 4×int16[64] (cajas aprendidas, verificación)  = 520 B
```
Hoy el motor usa 4 B (máscara) + 256 B (cajas `[32]`) = 260 B. **Delta** ≈ 2 436 + 520 − 260 ≈ **+2,7 KB** (reservado para el peor caso: 64 nombres distintos; con 3 nombres solo se usan ≈ 1 KB, pero la reserva es estática y por tanto predecible). Sin asignación dinámica.

## 4. RAM (MKS Robin Nano V3.1, STM32F407, 128 KB contados por PlatformIO)
[REFERENCIA] Etapa 6 (`docs/arquis/ARQUIS_UI_etapa_6.md` §5): RAM 82 372 B = 62,8 % (incluye heap y pila reservados); Flash 85,6 % (68 KB libres); heap de LVGL de 16 KB con ≥ 9,2 KB libres en el peor caso.
[CALCULADO] +2,7 KB ⇒ ≈ 85,1 KB ≈ **64,9 %**; margen ≈ 46 KB. Flash: parser + CRC + vista ampliada **estimados** en 6–9 KB de los 68 KB libres (Flash ≈ 87–88 % en el peor caso, frente al 85,6 % actual; **hay que medirlo**).
[PENDIENTE] Compilar y medir (`mks_robin_nano_v3_1_usb_flash_drive_msc`) antes y después. **Criterio de parada de la Etapa 3:** Flash < 90 %, RAM < 70 %, heap LVGL libre ≥ 8 KB en el peor caso; si no se cumple, no se integra.

## 5. Manifiesto — `tests/capacity64/manifest_size.py`
| Caso | Tamaño |
|---|---|
| Peor caso: 64 objetos, 64 nombres de 20 B | **5 150 B** |
| 40 piezas, 3 nombres | **2 066 B** |
| Línea más larga | 72 B (≤ 96; además son comentarios) |
Lectura en streaming con el `public_buf[513]` existente y un buffer de línea de 96 B: **0 B** de RAM adicional por el parser, aparte de la tabla del punto 3. El trailer se localiza en los últimos 1 KB; el bloque son ≤ 5,2 KB.

## 6. Interfaz LVGL (TS35, 480×320)
**Medición** (`tests/capacity64/run_lvgl.sh`, LVGL 6.1.1-MKS real compilado en el host de 64 bits — los tamaños de LVGL son mayores que en la placa de 32 bits, por tanto es conservador): un solo objeto con `design_cb` que dibuja N rectángulos + etiquetas, 40 redibujados completos por caso:

```
N=  0: heap antes=2640, pico=2640 (Δ pico = 0 B)
N= 16: Δ pico = 0 B      N= 40: Δ pico = 0 B      N= 64: Δ pico = 0 B      N=128: Δ pico = 0 B
heap final = heap inicial (sin fugas)
```
Conclusión: el coste de heap de la pantalla es el de sus **6 objetos fijos** (≈ 1,9 KB en la placa, [REFERENCIA] Etapa 6) y **no depende de N**. La lista virtualizada solo dibuja las 5‑6 filas visibles.
**Riesgos reales detectados (no son de heap):**
1. `arq_objects_update()` y `build_model()` **copian** el modelo (`arq_obj_model_t t = *m`, copia estática, copia en pila). Con 64 objetos y cajas `int16×4` cada copia pasa de ≈ 130 B a ≈ 560 B. El diseño de la Etapa 3 debe leer la tabla compartida por puntero, sin copias.
2. Tiempo de redibujado: la función de dibujo recorre N marcadores por cada una de las ≈ 19 franjas de 14 px del búfer. No medido en la placa **[PENDIENTE]**; mitigación prevista: descartar marcadores fuera de la franja antes de llamar a `lv_draw_*` y redibujar solo al cambiar el modelo (ya ocurre).
3. La rejilla actual 4×4 y `ARQ_OBJ_MAX 16` se sustituyen por la lista virtualizada (no es un cambio de heap).

## 7. G-code generado (coste de 64 objetos)
Medido con el generador sintético de pruebas (secciones **cortas**, por tanto es un peor caso de proporción): 64 objetos × 25 capas → +15,4 % de tamaño (+79 KB), ≈ 246 líneas `M486` por capa. En G-code real, con cientos de movimientos por sección, el porcentaje será bastante menor. Marlin procesa `M486` sin movimiento; el coste de CPU de ≈ 250 comandos por capa es despreciable frente al tiempo de capa.

## 8. Qué pasa con el firmware actual (capacidad 32)
Probado con el modelo del motor sin modificar (`cura/tests/test_firmware_model.py`): un G-code con capacidad 64 impreso en el firmware actual **cancela correctamente los objetos 0‑31** y los objetos 32‑63 **simplemente no se pueden cancelar** (se imprimen; nunca se cancela otro objeto por error). El script de Cura permite fijar la capacidad en 32 para ese caso.

## 9. Conclusión
64 es viable y suficiente (≈ 40 piezas habituales + 60 % de holgura). Se implementa **sin ampliar ningún otro límite**. Lo que falta confirmar solo se puede medir al compilar para la placa: Flash/RAM reales, tiempo de redibujado y pila. Hasta entonces las cifras marcadas [CALCULADO]/[PENDIENTE] son estimaciones, no resultados.
