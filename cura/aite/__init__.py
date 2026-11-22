# SPDX-License-Identifier: LGPL-3.0-or-later
"""AITE - ARQUIS Interleaved Adaptive Non-Planar Toolpath Engine (Fase 1: nucleo geometrico).

Paquete independiente de Cura y de Marlin, en Python puro (solo biblioteca estandar), como ``cura/plugins/ARQUISPlugin/engines``.
Diseno y justificacion: informe tecnico AITE (etapa 1) y ``docs/slicer_suite/10_aite_fase1.md``.

Fase 1 (esta): lector de mallas STL/3MF, comprobacion de que son estancas, generador de la pieza de prueba, campo de columnas y capas,
detector de voladizos (por facetas y por capas), solucionador de angulo por region, perfil del cabezal y comparador de volumen.
NO genera G-code ni trayectorias: eso son las Fases 2 y 3.
"""

__version__ = "0.1.0-fase1"
