# AITE — Fase 1: núcleo geométrico

> **Estado: implementado y probado en el PC (83 pruebas). Sin prueba física.** No genera G-code ni trayectorias: eso son las Fases 2 y 3. No modifica el firmware, Cura ni ningún archivo existente (solo añade `cura/aite/`, `cura/tests/aite/` y este documento).

AITE (*ARQUIS Interleaved Adaptive Non-Planar Toolpath Engine*) es la tecnología de laminado descrita en el informe técnico de la etapa 1. **No es la función No-Planar de la Etapa 5** (`09_etapa5_nonplanar.md`): aquella ondula capas sobre un mapa de alturas; AITE planifica capas inclinadas en voladizos, que un mapa de alturas no puede describir.

## 1. Qué hace la Fase 1

| Módulo del informe | Archivo | Qué hay |
|---|---|---|
| Fórmulas (D.1–D.11) | `physics.py` | Contacto entre cordones, ángulo crítico β_c, ventana de inclinación, sección del cordón, filamento por mm, velocidad límite por Z |
| Geometry Analyzer | `mesh.py`, `columns.py` | Lectores STL (binario y ASCII) y 3MF (unidades, componentes, transformaciones), comprobación de estanqueidad, volumen, columnas verticales (admiten voladizos) y pila de capas con mapas de bits |
| Overhang Detector | `overhang.py` | Por facetas (β_s = asin(max(0, −n_z))) y por capas (O_k = L_k \ (L_{k−1} ⊕ B(s_c))), con clasificación voladizo / puente / isla y profundidad |
| Adaptive Angle Solver (por región) | `overhang.py` | Inclinación necesaria = max(0, β_s − β_c + margen) y déficit si supera α_max |
| Perfil del cabezal | `head.py`, `data/head_arquis_xq1.json` | Piezas (η, ρ) del diseño real y cono único = la menor pendiente |
| Comparador de volumen | `compare.py` | V_falta y V_exceso con banda de tolerancia en XY y en Z |
| Pieza de prueba | `parts.py` | Tronco con ramas opuestas, cilindro, cuña con voladizo de ángulo exacto |
| Línea de comandos | `cli.py` | `info`, `analyze`, `head`, `make-test-part` |

Solo biblioteca estándar de Python (≥ 3.12), como `cura/plugins/ARQUISPlugin/engines`. Licencia LGPL-3.0-or-later.

## 2. Uso

Desde `cura/`:

```bash
python3 -m aite make-test-part pieza.stl                 # tronco 20x20x40 con dos ramas de 15 mm (19 000 mm3)
python3 -m aite info pieza.stl                           # estanqueidad y volumen
python3 -m aite head                                     # perfil del cabezal y cono único
python3 -m aite analyze pieza.stl --beta-c 55            # voladizos, inclinación necesaria y déficit
python3 -m aite analyze pieza.stl --strict               # código 1 si hay zonas fuera de la ventana
```

Resultado real sobre la pieza por defecto (0,9 s), con β_c = 48,4° (**supuesto**, c_min = 0,5) y el cono de 20,1°:

```
Voladizo maximo sin soporte (beta_c + alfa_max): 68.5°
Facetas ...: con voladizo > beta_c: 300.0 mm2; fuera de la ventana: 300.0 mm2; beta_s maximo 90.0°
Capas: 200, regiones sin apoyo: 2 (infactibles: 2)
   capa     z mm tipo      lados  area mm2  prof. mm  beta_s   inclin.  deficit
     75    15.00 voladizo  W        147.50     14.75     90°       42°      22°
     75    15.00 voladizo  E        147.50     14.75     90°       42°      22°
VEREDICTO: ... deficit maximo 21.5° (el modelo no se modifica).
```

Coincide con el informe: 2 × (15 × 10) mm² de cara inferior a 90°, déficit 90 − 48,4 − 20,1 = 21,5°.

## 3. Decisiones de diseño

- **El modelo nunca se modifica ni se repara.** Una malla abierta, con normales invertidas o con aristas compartidas por más de dos caras se *informa*; las normales invertidas se analizan con el signo cambiado sin tocar la malla.
- **Cono único** (decisión del propietario): α_max = la menor pendiente de todas las piezas y direcciones del cabezal = **20,1°** (borde lejano de la rueda del carro, cota conservadora). El perfil por dirección se conserva en el JSON.
- **β_c no es una constante.** Se pasa por parámetro (`--beta-c`) o se calcula con `--c-min`; el CLI avisa cuando es un supuesto. Se mide en el ensayo E2.
- **Facetas apoyadas en la cama se ignoran**: no son voladizos.
- **Columnas y mapas de bits** en vez de polígonos: un mapa de alturas no describe voladizos, y los polígonos exigirían un recortador de polígonos que Python puro no trae. Dilatar, restar y contar son operaciones sobre enteros.

## 4. Qué verifican las pruebas (`cura/tests/aite/`)

| Archivo | Comprueba |
|---|---|
| `test_aite_physics.py` | Las cifras del informe: c(β) de la tabla de D.5, β_c = 48,4 / 53,5 / 57,6°, α_max de cada pieza del cabezal, deficit 21,5°, velocidad de trayecto, errores de dominio |
| `test_aite_mesh.py` | Estanqueidad, volumen exacto, normales hacia fuera, ida y vuelta STL/3MF, malla abierta / invertida / duplicada / degenerada, archivos truncados, con NaN o mal formados, convención de matriz de 3MF |
| `test_aite_parts.py` | Volumen exacto de la cruz (19 000 mm³), cilindro (< 0,1 % del ideal), ángulo exacto de las cuñas, dimensiones incoherentes rechazadas |
| `test_aite_columns.py` | Volumen por columnas (exacto con celda alineada; ≤ 1 % en cilindro), columnas con varios intervalos, mallas abiertas, mapas de bits (dilatación, conjuntos, componentes) |
| `test_aite_overhang.py` | Dos regiones en voladizo en la capa 15 de la pieza pequeña, lados anclados, ángulo 90° asociado, déficit 15° con α = 20° y viabilidad con 40°; cuña de 45° sin regiones, de 70° factible, de 80° con déficit 5°; puente e isla |
| `test_aite_head_compare_cli.py` | Cono del perfil = 20,1°, margen térmico solo en piezas calientes, comparador (faltante, exceso, tolerancia XY y Z), CLI de extremo a extremo y códigos de salida |

**Puerta de la Fase 1 del informe** («volúmenes de sólidos analíticos con error ≤ 1 %»): cumplida. Cubo y cruz salen exactos; el cilindro, 0,1 % en la malla y ≤ 1 % por columnas.

## 5. Limitaciones conocidas

- **Resolución XY uniforme.** Con una celda que no encaja en las aristas el contorno se cuantiza: 1,3 % de error de volumen en la pieza pequeña con celda de 0,4 mm (prueba `test_cross_part_volume_error_depends_on_grid_alignment`). Con celda alineada es exacto.
- **El detector por capas muestrea en la altura media de cada capa**; un voladizo menor que una capa puede no verse. El de facetas no tiene esa limitación.
- **La asociación región ↔ β_s** usa una retícula de puntos sobre cada faceta (paso = celda) y una tolerancia de ±1 celda y ±1 capa; una región sin faceta cercana queda con β_s desconocido (`?`), nunca inventado.
- **El solucionador de ángulo es por región**, no un campo Φ suavizado (D.4): eso es la Fase 3.
- **Sin colisión continua del cabezal** contra lo ya impreso (Fase 2); solo la pendiente máxima.
- **Rendimiento:** pieza por defecto (19 000 mm³): 0,9 s; pieza mayor (tronco 40 × 40 × 60 con ramas de 30 mm, 120 000 mm³, 140 800 triángulos): 3,7 s de análisis. Son piezas con caras planas; una malla orgánica con millones de triángulos no se ha probado, y el informe ya prevé una estructura dispersa o un núcleo nativo si hace falta.
- **3MF:** no soporta componentes en archivos externos; los objetos que no son `model` se ignoran.

## 6. Hallazgo de la inspección (no es de AITE)

Dos pruebas **anteriores** del módulo fallan por la misma causa: `test_machine_limits_are_exactly_marlin_configuration_h` y `test_machine_profile_is_exactly_what_the_firmware_is_configured_for`. Sus expresiones regulares buscan `#define X` al inicio de línea y `Marlin/Configuration.h` ahora define esos valores dentro de `#ifndef` (commit a415386 y c5b5cfd). Además, el motor no planar sigue con Z a 5 mm/s y 100 mm/s² mientras el firmware dice 45 mm/s y 150 mm/s². AITE no depende de ninguna de las dos; **no se han tocado** a la espera de tu permiso.

## 7. Siguiente paso (Fase 2)

Simulador de depósito (cordones virtuales sobre la rejilla, que rellenarán `compare.py`) y detector de colisión por barrido continuo (avance conservador, D.10). Puerta: detecta el 100 % de choques construidos a propósito y coincide con la fuerza bruta en 1000 casos aleatorios.

Pendientes del propietario que **no** bloquean la Fase 2: ensayos E2 (β_c real), E3, E4 y E5; confirmar con calibre la altura de la rueda (14,5 mm) y del conducto derecho (8,25 mm).
