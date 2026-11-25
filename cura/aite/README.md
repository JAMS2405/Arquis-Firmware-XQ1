# aite — ARQUIS Interleaved Adaptive Non-Planar Toolpath Engine (Fase 1)

Núcleo geométrico en Python puro (≥ 3.12, solo biblioteca estándar), independiente de Cura y de Marlin. Licencia LGPL-3.0-or-later.

**Fase 1:** lee STL/3MF, comprueba que son estancas, detecta voladizos (por facetas y por capas), calcula la inclinación de capa necesaria
y el déficit frente al cono del cabezal, y compara volúmenes. **No genera G-code.**

```bash
cd cura
python3 -m aite make-test-part pieza.stl
python3 -m aite analyze pieza.stl --beta-c 55
python3 -m pytest tests/aite
```

Documentación completa, decisiones y limitaciones: [`docs/slicer_suite/10_aite_fase1.md`](../../docs/slicer_suite/10_aite_fase1.md).

**Árbol de prueba (primera prueba física, ensayo E2):** `python3 -m aite arbol` genera y **verifica** el G-code de un árbol con ramas en
voladizo de 90° (capas inclinadas finas en las ramas, temperatura y ventilador por rama, cabezal apartado al cambiar de temperatura).
Es una pieza prismática, no el laminador general. Guía, resultados de la verificación y qué cambiar si falla:
[`docs/slicer_suite/11_aite_arbol_prueba.md`](../../docs/slicer_suite/11_aite_arbol_prueba.md). Archivos listos: `herramientas/aite/listos/`.

```bash
cd cura
python3 -m aite arbol --probeta --salida ../herramientas/aite/listos/PETG/   # una rama: pruébala primero
python3 -m aite arbol --salida ../herramientas/aite/listos/PETG/             # el árbol de 5 cm
```
