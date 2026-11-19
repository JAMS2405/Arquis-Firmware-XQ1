# SPDX-License-Identifier: LGPL-3.0-or-later
"""Correspondencia tramo ``;MESH:`` <-> instancia de la escena de Cura.

Las copias de un modelo comparten nombre, asi que el nombre NO identifica la instancia. Se identifica por geometria: la caja XY de
las extrusiones de un tramo debe estar contenida en la caja de exactamente UNA hoja de la escena con ese nombre. Si no hay ninguna
o hay varias, la correspondencia no es segura y se informa (nunca se adivina).
"""
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Sequence, Tuple

from .gcode_scan_engine import NONMESH, ScanResult
from .object_engine import ArquisObject, SceneInstance
from .problems import ERROR, WARNING, Problem

EPS = 0.01          # mm: tolerancia numerica de la contencion "exacta"


def _contains(outer: Sequence[float], inner: Sequence[float], tol: float) -> bool:
    return (inner[0] >= outer[0] - tol and inner[1] >= outer[1] - tol and
            inner[2] <= outer[2] + tol and inner[3] <= outer[3] + tol)


@dataclass
class MatchResult:
    section_object: Dict[int, int] = field(default_factory=dict)       # indice de tramo -> id de objeto (-1 = siempre se imprime)
    section_leaf: Dict[int, int] = field(default_factory=dict)         # indice de tramo -> scene_index de la hoja
    problems: List[Problem] = field(default_factory=list)
    sections_per_object: Dict[int, int] = field(default_factory=dict)
    tolerance_used: int = 0


def match_sections(scan: ScanResult, objects: Sequence[ArquisObject], tolerance_mm: float) -> MatchResult:
    res = MatchResult()
    leaves: Dict[str, List[Tuple[SceneInstance, ArquisObject]]] = {}
    for o in objects:
        for l in o.leaves:
            leaves.setdefault(l.name, []).append((l, o))
            if l.name == NONMESH:
                res.problems.append(Problem("E_NONMESH_NAME", ERROR,
                                            "un objeto se llama 'NONMESH': su nombre colisiona con el marcador de CuraEngine para los elementos sin malla"))

    printed_leaves = set()
    unknown: Dict[str, int] = {}

    for s in scan.sections:
        if s.kind != "mesh" or s.mesh is None:
            res.section_object[s.index] = -1
            continue
        if s.bbox is None:                         # solo viajes bajo esta malla: se ejecutan siempre
            res.section_object[s.index] = -1
            continue
        cands = leaves.get(s.mesh)
        if not cands:
            unknown[s.mesh] = unknown.get(s.mesh, 0) + 1
            res.section_object[s.index] = -1
            continue
        exact = [c for c in cands if _contains(c[0].bbox, s.bbox, EPS)]
        pick = None
        if len(exact) == 1:
            pick = exact[0]
        elif len(exact) > 1:
            res.problems.append(Problem("E_AMBIGUOUS_INSTANCE", ERROR,
                                        "capa %s: el tramo ';MESH:%s' (%s) cabe en %d instancias a la vez (objetos solapados o anidados); "
                                        "no se puede saber a cual pertenece" % (s.layer, s.mesh, _fmt(s.bbox), len(exact))))
        else:
            loose = [c for c in cands if _contains(c[0].bbox, s.bbox, tolerance_mm)]
            if len(loose) == 1:
                pick = loose[0]
                res.tolerance_used += 1
            elif len(loose) > 1:
                res.problems.append(Problem("E_AMBIGUOUS_INSTANCE", ERROR,
                                            "capa %s: el tramo ';MESH:%s' (%s) cabe, con tolerancia %.2f mm, en %d instancias; "
                                            "demasiado juntas para distinguirlas" % (s.layer, s.mesh, _fmt(s.bbox), tolerance_mm, len(loose))))
            else:
                res.problems.append(Problem("E_SECTION_OUTSIDE_INSTANCE", ERROR,
                                            "capa %s: el tramo ';MESH:%s' (%s) no esta dentro de ninguna instancia con ese nombre "
                                            "(tolerancia %.2f mm); la escena de Cura no coincide con el G-code" % (s.layer, s.mesh, _fmt(s.bbox), tolerance_mm)))
        if pick is None:
            res.section_object[s.index] = -1
            continue
        leaf, obj = pick
        res.section_object[s.index] = obj.id if obj.cancelable else -1
        res.section_leaf[s.index] = leaf.scene_index
        printed_leaves.add(leaf.scene_index)
        if obj.cancelable:
            res.sections_per_object[obj.id] = res.sections_per_object.get(obj.id, 0) + 1

    for name, n in sorted(unknown.items()):
        res.problems.append(Problem("E_UNKNOWN_MESH", ERROR,
                                    "el G-code imprime ';MESH:%s' (%d tramos) y la escena de Cura no tiene ninguna instancia con ese nombre" % (name, n)))
    for o in objects:
        for l in o.leaves:
            if l.scene_index not in printed_leaves:
                res.problems.append(Problem("E_INSTANCE_NOT_PRINTED", ERROR,
                                            "la instancia '%s' (escena #%d) no aparece en el G-code: no se puede garantizar la correspondencia"
                                            % (l.name, l.scene_index)))
    if res.tolerance_used:
        res.problems.append(Problem("W_TOLERANCE_USED", WARNING,
                                    "%d tramos solo caben en su instancia usando la tolerancia de %.2f mm" % (res.tolerance_used, tolerance_mm)))
    return res


def _fmt(b: Sequence[float]) -> str:
    return "X%.2f..%.2f Y%.2f..%.2f" % (b[0], b[2], b[1], b[3])
