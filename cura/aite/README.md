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
