# SPDX-License-Identifier: LGPL-3.0-or-later
"""Carga G-code REAL de CuraEngine (tests/data) y deriva una 'escena' a partir de el.

OJO: la escena derivada sale del propio G-code (union de las cajas de cada tramo + margen), asi que NO prueba la correspondencia
escena<->G-code (eso necesita la escena real de Cura); prueba que el G-code REAL se procesa y que cancelar cualquier objeto es seguro.
"""
import gzip
import os
import re

from arquis_engines.gcode_scan_engine import scan_gcode
from arquis_engines.object_engine import SceneInstance

DATA = os.path.join(os.path.dirname(os.path.abspath(__file__)), "data")


def load_reduced(name: str) -> str:
    with gzip.open(os.path.join(DATA, name + "_reducido.gcode.gz"), "rt", encoding="utf-8") as f:
        return f.read()


def load_full(name_file: str):
    d = os.environ.get("ARQUIS_REAL_GCODE_DIR")
    if not d:
        return None
    p = os.path.join(d, name_file)
    return open(p, encoding="utf-8").read() if os.path.exists(p) else None


def split_like_cura(text: str):
    """[prefijo, capa0, capa1, ..., final]: cada capa empieza en ';LAYER:' y termina en su ';TIME_ELAPSED:'."""
    lines = text.split("\n")
    starts = [i for i, l in enumerate(lines) if re.match(r"^;LAYER:-?\d+\s*$", l)]
    ends = [i for i, l in enumerate(lines) if l.startswith(";TIME_ELAPSED:")]
    chunks = ["\n".join(lines[:starts[0]]) + "\n"]
    for s, e in zip(starts, ends):
        chunks.append("\n".join(lines[s:e + 1]) + "\n")
    chunks.append("\n".join(lines[ends[-1] + 1:]))
    assert "".join(chunks) == text
    return chunks


def derive_scene(chunks, margin: float = 0.3, rename=None):
    sc = scan_gcode(chunks)
    boxes = {}
    for s in sc.sections:
        if s.kind == "mesh" and s.mesh and s.bbox:
            b = boxes.setdefault(s.mesh, list(s.bbox))
            b[0], b[1], b[2], b[3] = min(b[0], s.bbox[0]), min(b[1], s.bbox[1]), max(b[2], s.bbox[2]), max(b[3], s.bbox[3])
    return [SceneInstance(i, rename(n) if rename else n, b[0] - margin, b[1] - margin, b[2] + margin, b[3] + margin)
            for i, (n, b) in enumerate(boxes.items())]
