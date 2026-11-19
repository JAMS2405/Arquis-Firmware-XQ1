# SPDX-License-Identifier: LGPL-3.0-or-later
"""Prueba de orientacion fisica: 5 piezas en las zonas de la cama que el mapa debe reproducir.

Se imprime, se mira la cama DE FRENTE y se compara con el mapa de la pantalla (o de Cura):
    1 esquina frontal izquierda · 2 esquina frontal derecha · 3 centro · 4 esquina posterior izquierda · 5 esquina posterior derecha
Cada pieza tiene un tamano distinto (20, 22, 24, 26, 28 mm) para reconocerla sin numeros grabados; el mapa debe mostrar el
marcador mas pequeno en la esquina frontal izquierda, etc. Genera G-code con la forma de la salida de CuraEngine y lo pasa por el
MISMO motor (M486 + manifiesto) que cualquier impresion: asi tambien prueba la cancelacion de una pieza concreta.
Sin Cura: ``python cura/tools/make_orientation_gcode.py``.
"""
import math
from typing import List, Optional, Tuple

from .m486_engine import M486Options, M486Result, process_gcode
from .object_engine import SceneInstance

ZONES = [("Frontal izquierda", "FL"), ("Frontal derecha", "FR"), ("Centro", "C"), ("Posterior izq.", "BL"), ("Posterior der.", "BR")]
SIZES = (20.0, 22.0, 24.0, 26.0, 28.0)


def layout(bed_w: float, bed_d: float, margin: float = 30.0) -> List[Tuple[str, float, float, float]]:
    """[(nombre, x0, y0, lado)] para las 5 zonas; (x0, y0) = esquina frontal izquierda de la pieza (mm, sistema de G-code con origen FL)."""
    s = SIZES
    pos = {"FL": (margin, margin), "FR": (bed_w - margin - s[1], margin), "C": ((bed_w - s[2]) / 2, (bed_d - s[2]) / 2),
           "BL": (margin, bed_d - margin - s[3]), "BR": (bed_w - margin - s[4], bed_d - margin - s[4])}
    return [(ZONES[i][0], pos[ZONES[i][1]][0], pos[ZONES[i][1]][1], s[i]) for i in range(5)]


def build(bed_w: float = 300.0, bed_d: float = 300.0, nozzle: int = 200, bed_temp: int = 60, layers: int = 4,
          layer_h: float = 0.2, first_h: float = 0.3) -> Tuple[List[str], List[SceneInstance]]:
    pieces = layout(bed_w, bed_d)
    inst = [SceneInstance(i, name, x0, y0, x0 + s, y0 + s) for i, (name, x0, y0, s) in enumerate(pieces)]
    k = (layer_h * 0.4) / (math.pi * 0.875 ** 2)                                  # mm de filamento por mm de linea (1,75 mm)
    e = 0.0
    chunks = [";FLAVOR:Marlin\n;TIME:600\n;Layer height: %.2f\n;Generated with Cura_SteamEngine 5.11.0 (ARQUIS orientation test)\n;LAYER_COUNT:%d\n" % (layer_h, layers),
              "M140 S%d\nM104 S%d\nM190 S%d\nM109 S%d\nM82\nG28\nG90\nG92 E0\n" % (bed_temp, nozzle, bed_temp, nozzle)]
    x = y = 0.0
    for layer in range(layers):
        z = first_h + layer * layer_h
        L = [";LAYER:%d" % layer, "G92 E0"]
        e = 0.0
        order = sorted(range(5), key=lambda i: (pieces[i][2] + pieces[i][3] / 2, pieces[i][1]))
        for n, i in enumerate(order):
            name, x0, y0, s = pieces[i]
            L.append(";MESH:%s" % name)
            if n or layer:
                e -= 1.0
                L.append("G1 F2400 E%.5f" % e)
            L.append("G0 F6000 X%.3f Y%.3f Z%.3f" % (x0 + 0.2, y0 + 0.2, z + (0.4 if n else 0.0)))      # primera pieza de la capa: cambio de capa; las demas: con salto en Z
            if n:
                L.append("G0 F600 Z%.3f" % z)
            if n or layer:
                e += 1.0
                L.append("G1 F2400 E%.5f" % e)
            L.append(";TYPE:WALL-OUTER")
            inset = 0.2
            px, py = x0 + inset, y0 + inset
            while s - 2 * inset > 0.4:
                for tx, ty in ((x0 + s - inset, y0 + inset), (x0 + s - inset, y0 + s - inset), (x0 + inset, y0 + s - inset), (x0 + inset, y0 + inset + 0.4)):
                    e += math.hypot(tx - px, ty - py) * k
                    L.append("G1 F1800 X%.3f Y%.3f E%.5f" % (tx, ty, e))
                    px, py = tx, ty
                inset += 0.4
            x, y = px, py
        L.append("G0 F600 Z%.3f" % (z + layer_h))                                    # ultimo viaje: subir a la siguiente capa
        L.append(";TIME_ELAPSED:%d.0" % (layer + 1))
        chunks.append("\n".join(L) + "\n")
    chunks.append("M106 S0\nM104 S0\nM140 S0\nG91\nG1 F1800 E-3\nG1 F3000 Z10\nG90\nG1 F3000 X5 Y%d\nM84 X Y E\n;End of Gcode\n" % int(bed_d - 5))
    return chunks, inst


def make(bed_w: float = 300.0, bed_d: float = 300.0, **kw) -> M486Result:
    chunks, inst = build(bed_w, bed_d, **kw)
    return process_gcode(chunks, inst, M486Options(bed=(0.0, 0.0, bed_w, bed_d), expected_bed=(bed_w, bed_d)))
