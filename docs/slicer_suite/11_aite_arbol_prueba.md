# AITE — Árbol de prueba con ramas en voladizo de 90° (primera prueba física)

> **Estado: G-code generado y verificado en el PC. NO probado en la impresora.** Lo que sigue describe lo que el programa *calcula*; si el plástico se comporta así lo decide esta misma prueba. No modifica el firmware, Cura ni archivos existentes (añade `cura/aite/arbol.py`, `cura/aite/gcode_check.py`, un subcomando, pruebas y `herramientas/aite/listos/`).

Pedido del propietario: un árbol de 5 cm de alto y 5 cm de ancho como máximo, tronco de 1 cm y dos ramas que salgan a **90°** (cara inferior plana, sin soportes) y que **se mantengan a 90°**. PETG, cama 70 °C, boquilla 230 °C, boquilla de 0,6 mm con ancho de línea 0,66 mm. Bajar la temperatura en la rama, ventilador al máximo, y apartar el cabezal mientras cambia la temperatura.

## 1. Qué se imprime

| Pieza | Medidas |
|---|---|
| Tronco | cuadrado de 10 × 10 mm, 50 mm de alto, con 4 anillos de borde en la primera capa |
| Rama derecha | sale hacia +X, **piso plano a z = 18,0 mm**, 20 mm de largo, 8 mm de alto, 10 mm de ancho |
| Rama izquierda | sale hacia −X, **piso plano a z = 34,05 mm**, mismas medidas |
| Ancho total | 10 + 20 + 20 = **50 mm** (en el centro de la cama, X de 125 a 175) |

Archivos listos (`herramientas/aite/listos/PETG/`):

| Archivo | Qué es | Tiempo estimado* |
|---|---|---|
| `probeta_rama90_PETG_xq1.gcode` | **Pruébala primero.** Una sola rama (derecha) sobre un tronco de 15,5 mm | 35 min |
| `arbol_voladizo90_PETG_xq1.gcode` | El árbol pedido (dos ramas, 50 mm) | 79 min |
| `probeta_rama90_PETG_xq1_SECO.gcode` | La probeta **sin extruir, sin calentar y 10 mm más arriba**: solo para ver el movimiento | 35 min |

\* Trapecios de aceleración y esquinas del firmware, **sin** las esperas de temperatura (cada cambio de 230 a 220 °C y la vuelta tardan del orden de medio minuto; hay 4 en el árbol). Es aproximado.

## 2. Cómo se mantiene el piso a 90°

El tronco se imprime con capas planas. Las **ramas se imprimen con capas inclinadas finas que bajan hacia afuera** desde la pared del tronco; el eje Z se mueve dentro de cada capa. Las puntas de todas las capas caen sobre un mismo plano horizontal (el piso), que por eso queda plano. No hay soportes y la geometría no se modifica.

Cada capa inclinada sobresale de la anterior `t / sen(α)` en horizontal. Con los valores por defecto:

| Dato | Valor |
|---|---|
| Capa inclinada `t` | 0,10 mm |
| Ancho de línea `w` | 0,66 mm |
| Inclinación `α` | 18,5° (ventana permitida 16,9° a 19,1°) |
| Saliente por capa | 0,32 mm en horizontal = **0,30 mm medido sobre la capa** |
| Apoyo de la punta sobre la capa de abajo | **55 %** del cordón (`c = 1 − (t/w)·tan(90° − α)`) |

**Por qué hace falta capa fina.** El apoyo necesario no depende de la boquilla sino de la relación `w/t` y de la inclinación. Con capa de 0,2 mm el apoyo sería del 13 %; para llegar al 50 % haría falta `α ≥ 31°`, y el cabezal solo permite 20,1° (cono único, con 1° de margen queda 19,1°). Con 0,15 mm hace falta 24,4°; con 0,12 mm, 20,0°: tampoco. Con **0,10 mm basta 16,9°**. Por eso el programa **se niega a generar** el archivo si la combinación no tiene ventana (`error: no hay ventana de inclinación…`).

**La capa de 0,10 mm es el 17 % de la boquilla** (se recomienda ≥ 25 %). Es la capa que da el ángulo, pero es el primer punto débil de esta prueba (ver §6).

El apoyo mínimo `c_min = 0,5` es un **supuesto** de partida, no una medida. Esta prueba es la medida.

### La idea de «media pared»
La semilla de dos pasadas medio cordón desplazadas que propusiste equivale al escalón de 45° de toda la vida (ángulo `atan((w/2)/t)`); por sí sola da una **pendiente**, no un piso plano. Aquí cada capa inclinada ya hace de «media pared»: se desplaza 0,30 mm de los 0,66 mm de ancho (un 45 %, apoyada el 55 %), y la inclinación de 18,5° convierte ese escalón en un piso horizontal.

## 3. Temperatura, ventilador y apartar el cabezal

Para cada rama:

1. El tronco sube con capas planas a 230 °C hasta el nivel del piso.
2. **Aparcar**: retrae 1 mm, sube 3 mm sobre lo impreso y se va a X = 220, Y = 150 (70 mm del centro, fuera del árbol).
3. `M106 S255` (ventilador al máximo), `M104 S220` y `M109 R220` (espera también al **enfriar**; `M109 S` solo espera al calentar).
4. Cebado lento de 2 mm de filamento (limpia lo que supuró), retrae y vuelve a la pieza.
5. La rama y las capas del tronco que la acompañan se imprimen a **220 °C con ventilador al 100 %**, a 20 mm/s.
6. **Al terminar la rama**: aparca otra vez, `M104 S230` + `M109 S230`, ventilador vuelve a 100/255 y sigue el tronco.

Los valores se cambian con `--temp-voladizo 215`, `--ventilador-voladizo`, `--sin-aparcar` (cambia la temperatura sin apartarse). El firmware confirma que `M109 R` existe (`Marlin/src/gcode/temp/M104_M109.cpp`).

## 4. Por qué el tronco sube «intercalado» con la rama

La pared del tronco tiene que existir hasta la altura donde cada capa inclinada la toca, y la boquilla no puede pasar a 0,3 mm de una pared más alta que su punta. Por eso dentro de la rama las **capas del tronco son más finas (0,15 mm)** y se imprimen justo antes de la capa inclinada que las necesita: la pared nunca queda más de ~0,1 mm por encima de la punta. Cada intercalado cuesta una retracción, un desplazamiento y un cebado (unos 50 por rama). Es el origen de parte del tiempo extra.

La alternativa (subir el tronco entero antes) no sirve: con el tronco 8 mm por encima de la punta, el cabezal choca (§5).

## 5. Qué comprobó el programa (leyendo el texto del G-code)

`aite.gcode_check.verificar` no usa el plan interno: parsea el archivo línea a línea. Resultado del árbol completo:

| Comprobación | Resultado |
|---|---|
| Órdenes, límites de cama (margen 10 mm), Z ≤ 450, velocidades, temperaturas, extrusión solo en caliente, termina en `M501` | sin problemas |
| Velocidad máxima de Z pedida | 13,2 mm/s (máximo del firmware 45) |
| **Holgura mínima del cabezal** contra todo lo ya impreso, en cada instante | **1,59 mm** (con el perfil del dibujo real: bloque 8,0 mm a ρ ≤ 16,1; conducto 8,25 mm a ρ ≤ 20,15; rueda 14,5 mm) |
| Desplazamientos que arrastren la boquilla por dentro de lo impreso | ninguno |
| Volumen depositado / árbol pedido (con borde) | 1,028 (con flujo 0,95 queda 0,98) |
| Sección central (plano y = centro), huecos grandes | 0,06 % (el mayor 0,16 mm²); huecos entre cordones 1,1 % (normales) |
| Material que sobra fuera del árbol pedido | 0,21 % |
| **Planitud del piso** (material más bajo de cada columna frente al plano pedido) | **de −0,024 a +0,076 mm** en ambas ramas |

Imágenes de la sección simulada: [`img/aite_arbol_seccion_simulada.png`](img/aite_arbol_seccion_simulada.png), [`img/aite_rama_seccion_simulada.png`](img/aite_rama_seccion_simulada.png) (rojo = piso pedido, azul = pared del tronco) y [`img/aite_punta_seccion_simulada.png`](img/aite_punta_seccion_simulada.png) (dientes de ~0,1 mm).

El verificador **detecta lo que debe** (pruebas): un choque del cabezal (alfa = 30° con rama de 14 mm), un desplazamiento por dentro de lo impreso, una pared alta junto a la boquilla, órdenes no permitidas, coordenadas fuera de límites, extrusión sin calentar, un archivo sin `M501`, capas borradas, y la versión en seco con órdenes de calentar.

## 6. Qué NO comprueba y dónde puede fallar (por orden de riesgo)

1. **Apoyo real del cordón (`c_min`).** 0,5 es un supuesto. Si el plástico necesita más, el piso se hunde en la punta. Es lo que mide esta prueba.
2. **Capa de 0,10 mm con boquilla de 0,6 mm.** Mucha presión y poco caudal (1,3 mm³/s). Puede salir subextruido o con roce. Pruébalo con `--flujo 1.0` o `1.05`.
3. **Punta inclinada sobre la capa de abajo.** La boquilla es vertical y la capa está a 18,5°: en el lado de subida, la capa de abajo sube 0,10 a 0,15 mm dentro del radio de la boquilla (0,3–0,45 mm × tan 18,5°), que es tanto como la propia capa (0,10). El borde plano de la boquilla puede rozar o pisar hasta ~0,05 mm. La boquilla puntiaguda (tipo K1) lo reduce. Si la punta roza o arrastra: `--elevar-z 0.03` a `0.05`.
4. **Combado de la punta.** Un cordón de 0,1 mm enfría rápido, pero la punta cuelga 0,3 mm sin apoyo debajo. Ayudan: menos temperatura (`--temp-voladizo 215`), menos velocidad (`--v-incl 15`), más inclinación (hasta 19,1°).
5. **Cabezal.** El cono sale de **dibujos**, no de calibre: la altura de la rueda (14,5 mm) y del conducto derecho (8,25 mm) están pendientes de confirmar, y los 8 mm de holgura se midieron con **una** boquilla. La holgura mínima de 1,59 mm es contra ese modelo.
6. **Roce con la pared del tronco.** El centro del primer cordón de cada capa inclinada queda a 0,24 mm de la pared, y la pared puede pasar hasta ~0,1 mm por encima de la punta.
7. **Enfriar el tronco 30 s antes de la rama** puede bajar la adhesión de las primeras capas inclinadas.
8. **Física del Z en movimiento** (E4): Z a 150 mm/s² limita la aceleración del trayecto a 473 mm/s² en las capas inclinadas. El programa pone `M204 P500 T500` ahí y vuelve a 1000 en el tronco (`M501` restaura al final).

## 7. Cómo probar (orden recomendado)

1. **Copia a la SD** el archivo (raíz, no la carpeta `assets`).
2. *(Opcional, 35 min)* `probeta…_SECO.gcode`: el cabezal recorre todo sin extruir, 10 mm por encima de la cama. Mira que los viajes de aparcar a X = 220 no choquen con nada tuyo y que los Z se muevan suave.
3. **`probeta_rama90_PETG_xq1.gcode`**, vigilando las capas inclinadas con la mano en el botón de parada. Lo que importa: que la punta no arrastre ni roce, que el hilo salga continuo y que el piso quede recto.
4. Mide con calibre y haz foto de perfil: **planitud del piso** a lo largo de los 20 mm y **cuánto cuelga la punta**. Anota la temperatura y el flujo.
5. Si la probeta sale, imprime **`arbol_voladizo90_PETG_xq1.gcode`**.

## 8. Si falla, qué cambiar

| Síntoma | Qué probar |
|---|---|
| El piso se hunde / cae la punta | `--temp-voladizo 215`, `--v-incl 15`, `--alfa 19.0` (máximo permitido 19,1), `--capa-incl 0.08` |
| Cordones separados / huecos | `--flujo 1.0` o `1.05` |
| La boquilla roza o arrastra la capa de abajo | `--elevar-z 0.04` |
| La punta se levanta (curl) | subir temperatura de la rama (`--temp-voladizo 225`), menos ventilador (`--ventilador-voladizo 180`) |
| Mala unión con el tronco | `--traslape 0.15`, `--temp-voladizo 225` |
| Quieres explorar capa más gruesa | `--c-min 0.4 --capa-incl 0.12` (acepta 40 % de apoyo) |

Regenerar con otros valores (desde `cura/`):

```bash
python3 -m aite arbol --salida ../herramientas/aite/listos/PETG/ --temp-voladizo 215 --alfa 19
python3 -m aite arbol --probeta --salida ../herramientas/aite/listos/PETG/ --elevar-z 0.04
python3 -m aite arbol --probeta --seco --salida ../herramientas/aite/listos/PETG/
```

El programa **no escribe el archivo** si no pasa la verificación (código de salida 2).

## 9. Limitaciones del código

- Es una pieza prismática (cajas): el tronco y las ramas son rectángulos. No es todavía el laminador general (Fases 2 y 3 del informe: simulador de depósito, colisión continua y campo de capas Φ para un STL cualquiera).
- Las primeras 2–3 capas inclinadas (rincón pared-piso) no caben y dejan una cuña de ~0,4 mm sin material en esa esquina.
- La holgura del cabezal es **omnidireccional y conservadora**: ignora de qué lado está cada pieza.
- Los ensayos E3, E4 y E5 del informe (anclaje al tronco, Z en movimiento, caudal con inclinación) siguen pendientes; esta prueba cubre E2 (apoyo mínimo).

Pruebas: `cura/tests/aite/test_aite_arbol.py` (39). Las dos pruebas anteriores que fallan (`test_machine_limits…`, `test_machine_profile…`, ver `10_aite_fase1.md` §6) siguen igual y no se han tocado.
