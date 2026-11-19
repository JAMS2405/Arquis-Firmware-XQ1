# SPDX-License-Identifier: LGPL-3.0-or-later
"""Validador independiente del G-code ya procesado (no reutiliza el escaner del motor para sus decisiones de contexto).

Comprueba las invariantes que hacen segura la cancelacion; el motor la ejecuta sobre su propia salida antes de entregarla y
``scripts/ARQUIS_Validation.py`` la puede ejecutar sobre cualquier G-code.
"""
from typing import Dict, List, Optional, Sequence, Tuple

from .gcode_scan_engine import (FILAMENT_CMDS, LAYER_RE, MESH_PREFIX, MOVE_CMDS, TIME_PREFIX, parse_cmd)
from .matching_engine import _contains
from .object_engine import ArquisObject
from .problems import ERROR, WARNING, Problem


def validate_m486(chunks: Sequence[str], capacity: int, objects: Optional[Sequence[ArquisObject]] = None,
                  tolerance_mm: float = 1.0) -> List[Problem]:
    P: List[Problem] = []
    t_values: List[int] = []
    t_seen_before_layer = False
    layer_seen = False
    active = -1
    in_object_section = False       # tramo ;MESH:<nombre> (distinto de NONMESH): el unico sitio donde el contexto puede ser un objeto
    ids_with_extrusion = set()
    last_s_line = None
    x = y = None
    e = None
    e_abs = True
    abs_xyz = True
    by_id: Dict[int, ArquisObject] = {o.id: o for o in (objects or []) if o.cancelable}
    reported = set()

    def once(code: str, sev: str, msg: str) -> None:
        if (code, msg) not in reported:
            reported.add((code, msg))
            P.append(Problem(code, sev, msg))

    lineno = -1
    for chunk in chunks:
        for line in chunk.split("\n"):
            lineno += 1
            if line.startswith(";"):
                if LAYER_RE.match(line):
                    layer_seen = True
                    in_object_section = False            # al empezar la capa el contexto implicito es NONMESH
                elif line.startswith(MESH_PREFIX):
                    in_object_section = line[len(MESH_PREFIX):].rstrip("\r") != "NONMESH"
                elif line.startswith(TIME_PREFIX):
                    in_object_section = False
                continue
            cmd = parse_cmd(line)
            if cmd is None:
                continue
            n, w = cmd.name, cmd.words
            if n == "M486":
                if "P" in w or "U" in w or "C" in cmd.flags or "C" in w:
                    once("V_FOREIGN_M486_CMD", ERROR, "linea %d: M486 P/U/C no debe aparecer en un G-code ARQUIS" % (lineno + 1))
                if "T" in w:
                    t_values.append(int(w["T"]))
                    if layer_seen:
                        once("V_T_AFTER_LAYER", ERROR, "linea %d: M486 T despues del primer ;LAYER:" % (lineno + 1))
                    active = -1
                    last_s_line = None
                if "S" in w:
                    s = int(w["S"])
                    hi = (t_values[-1] if t_values else 0) - 1
                    if not t_values:
                        once("V_S_BEFORE_T", ERROR, "linea %d: M486 S antes de M486 T" % (lineno + 1))
                    if s < -1 or s > hi:
                        once("V_S_RANGE", ERROR, "linea %d: M486 S%d fuera de [-1, %d]" % (lineno + 1, s, hi))
                    if s == active:
                        once("V_S_REDUNDANT", ERROR, "linea %d: M486 S%d redundante (ya activo)" % (lineno + 1, s))
                    active = s
                continue
            if n == "G90": abs_xyz = True; continue
            if n == "G91": abs_xyz = False; continue
            if n == "M82": e_abs = True; continue
            if n == "M83": e_abs = False; continue
            if n == "G92":
                if "X" in w: x = w["X"]
                if "Y" in w: y = w["Y"]
                if "E" in w: e = w["E"]
                continue
            is_motion = n in MOVE_CMDS
            if n in FILAMENT_CMDS:
                if active >= 0:
                    once("V_FILAMENT_IN_OBJECT", ERROR, "linea %d: %s dentro del objeto %d (un objeto cancelado lo omitiria)" % (lineno + 1, n, active))
                continue
            if not is_motion:
                continue
            has_xy, has_z, has_e = ("X" in w or "Y" in w), "Z" in w, "E" in w
            de = 0.0
            if has_e:
                de = (w["E"] - (e if e is not None else w["E"])) if e_abs else w["E"]
                e = w["E"] if e_abs else (e or 0.0) + w["E"]
            nx = (w["X"] if abs_xyz else (x or 0.0) + w["X"]) if "X" in w else x
            ny = (w["Y"] if abs_xyz else (y or 0.0) + w["Y"]) if "Y" in w else y
            extruding = n != "G0" and has_e and has_xy and de > 1e-9
            if active >= 0:
                if not in_object_section:
                    once("V_OBJECT_IN_NONMESH", ERROR, "linea %d: movimiento bajo el objeto %d fuera de un tramo ;MESH: (inicio/fin de capa o G-code final)" % (lineno + 1, active))
                if has_z and not (extruding and has_xy):
                    once("V_Z_IN_OBJECT", ERROR, "linea %d: movimiento con Z dentro del objeto %d (un objeto cancelado perderia la altura)" % (lineno + 1, active))
                if has_e and not has_xy:
                    once("V_E_ONLY_IN_OBJECT", ERROR, "linea %d: movimiento de filamento dentro del objeto %d (descompensaria la retraccion)" % (lineno + 1, active))
                if has_e and has_xy and not extruding:
                    once("V_E_IN_TRAVEL_IN_OBJECT", ERROR, "linea %d: viaje con filamento (G0 con E o retraccion en movimiento) dentro del objeto %d: un objeto cancelado perderia la retraccion" % (lineno + 1, active))
                if has_z and has_e and has_xy and extruding:
                    once("V_Z_EXTRUDING", WARNING, "linea %d: extrusion con Z dentro del objeto %d" % (lineno + 1, active))
                if extruding:
                    ids_with_extrusion.add(active)
                    o = by_id.get(active)
                    if o is not None:
                        for px, py in ((x, y), (nx, ny)):
                            if px is not None and py is not None and not _contains(o.bbox, (px, py, px, py), tolerance_mm):
                                once("V_OUTSIDE_BBOX", ERROR, "linea %d: extrusion del objeto %d en (%.2f, %.2f), fuera de su caja" % (lineno + 1, active, px, py))
                                break
            x, y = nx, ny

    if len(t_values) != 1:
        P.append(Problem("V_T_COUNT", ERROR, "se esperaba exactamente un M486 T y hay %d" % len(t_values)))
    else:
        t = t_values[0]
        if t > capacity:
            P.append(Problem("V_T_OVER_CAPACITY", ERROR, "M486 T%d supera la capacidad %d" % (t, capacity)))
        for i in range(t):
            if i not in ids_with_extrusion:
                P.append(Problem("V_ID_UNUSED", ERROR, "el objeto %d no tiene ninguna extrusion con M486 S%d" % (i, i)))
    return P


# --------------------------------------------------------------------------------------------------------------------------------
# Estadisticas por objeto medidas EN el G-code ya procesado, y validacion cruzada con el manifiesto
# --------------------------------------------------------------------------------------------------------------------------------
from dataclasses import dataclass


@dataclass
class ObjStat:
    bbox: List[float]          # [x0, y0, x1, y1] de las extrusiones (mm)
    z0: float                  # Z de la primera extrusion
    ztop: float                # Z de la ultima extrusion
    first: int                 # numero de orden de la primera extrusion (para el orden de impresion)
    last: int
    n: int


def gcode_object_stats(chunks: Sequence[str]) -> Dict[int, ObjStat]:
    """Por id de M486 S: caja XY, Z inicial/final y orden de aparicion de sus extrusiones (id >= 0)."""
    out: Dict[int, ObjStat] = {}
    active = -1
    x = y = z = e = None
    e_abs, abs_xyz = True, True
    k = 0
    for chunk in chunks:
        for line in chunk.split("\n"):
            if line.startswith(";"):
                continue
            cmd = parse_cmd(line)
            if cmd is None:
                continue
            n, w = cmd.name, cmd.words
            if n == "M486" and "S" in w:
                active = int(w["S"])
            elif n == "G90": abs_xyz = True
            elif n == "G91": abs_xyz = False
            elif n == "M82": e_abs = True
            elif n == "M83": e_abs = False
            elif n == "G92":
                if "X" in w: x = w["X"]
                if "Y" in w: y = w["Y"]
                if "Z" in w: z = w["Z"]
                if "E" in w: e = w["E"]
            elif n in MOVE_CMDS:
                de = 0.0
                if "E" in w:
                    de = (w["E"] - (e if e is not None else w["E"])) if e_abs else w["E"]
                    e = w["E"] if e_abs else (e or 0.0) + w["E"]
                nx = (w["X"] if abs_xyz else (x or 0.0) + w["X"]) if "X" in w else x
                ny = (w["Y"] if abs_xyz else (y or 0.0) + w["Y"]) if "Y" in w else y
                nz = (w["Z"] if abs_xyz else (z or 0.0) + w["Z"]) if "Z" in w else z
                if active >= 0 and n != "G0" and ("X" in w or "Y" in w) and "E" in w and de > 1e-9:
                    st = out.get(active)
                    pts = [p for p in ((x, y), (nx, ny)) if p[0] is not None and p[1] is not None]
                    k += 1
                    if st is None:
                        st = out[active] = ObjStat([pts[-1][0], pts[-1][1], pts[-1][0], pts[-1][1]], nz if nz is not None else 0.0,
                                                   nz if nz is not None else 0.0, k, k, 0)
                    for px, py in pts:
                        st.bbox = [min(st.bbox[0], px), min(st.bbox[1], py), max(st.bbox[2], px), max(st.bbox[3], py)]
                    if nz is not None:
                        st.z0 = min(st.z0, nz)
                        st.ztop = max(st.ztop, nz)
                    st.last, st.n = k, st.n + 1
                x, y, z = nx, ny, nz
    return out


def print_order(stats: Dict[int, ObjStat]) -> Dict[int, int]:
    """Orden de impresion 0..N-1 = orden de la primera extrusion de cada objeto."""
    return {i: r for r, i in enumerate(sorted(stats, key=lambda i: stats[i].first))}


def validate_manifest_vs_gcode(chunks: Sequence[str], manifest, tolerance_mm: float = 1.0) -> List[Problem]:
    """El manifiesto debe describir EXACTAMENTE el G-code: mismo numero de objetos que ``M486 T``, mismos ids, cada extrusion dentro de
    su caja, mismas alturas y mismo orden de impresion. Si falla, la cancelacion visual no es segura."""
    P: List[Problem] = []
    stats = gcode_object_stats(chunks)
    t_vals = [int(parse_cmd(l).words["T"]) for c in chunks for l in c.split("\n")
              if not l.startswith(";") and (parse_cmd(l) is not None and parse_cmd(l).name == "M486" and "T" in parse_cmd(l).words)]
    if len(t_vals) != 1 or t_vals[0] != len(manifest.objects):
        P.append(Problem("V_MANIFEST_T", ERROR, "M486 T (%s) no coincide con N del manifiesto (%d)" % (t_vals, len(manifest.objects))))
    if set(stats) != {o.id for o in manifest.objects}:
        P.append(Problem("V_MANIFEST_IDS", ERROR, "los ids con extrusion en el G-code (%s) no coinciden con los del manifiesto" % sorted(stats)))
        return P
    order = print_order(stats)
    tol = tolerance_mm * UNIT_MM
    for o in manifest.objects:
        st = stats[o.id]
        x0, x1, y0, y1 = o.cx - o.w / 2.0, o.cx + o.w / 2.0, o.cy - o.d / 2.0, o.cy + o.d / 2.0
        if (st.bbox[0] * UNIT_MM < x0 - tol or st.bbox[2] * UNIT_MM > x1 + tol or st.bbox[1] * UNIT_MM < y0 - tol or st.bbox[3] * UNIT_MM > y1 + tol):
            P.append(Problem("V_MANIFEST_BBOX", ERROR, "objeto %d: sus extrusiones salen de la caja del manifiesto" % o.id))
        if abs(round(st.z0 * UNIT_MM) - o.z0) > 1 or abs(round(st.ztop * UNIT_MM) - o.ztop) > 1:
            P.append(Problem("V_MANIFEST_Z", ERROR, "objeto %d: Z del manifiesto (%d..%d) distinta de la del G-code (%d..%d)"
                             % (o.id, o.z0, o.ztop, round(st.z0 * UNIT_MM), round(st.ztop * UNIT_MM))))
        if order[o.id] != o.order:
            P.append(Problem("V_MANIFEST_ORDER", ERROR, "objeto %d: orden de impresion %d distinto del del G-code (%d)" % (o.id, o.order, order[o.id])))
    if manifest.seq:                                              # un objeto cada vez: sus extrusiones no se intercalan con las de otro
        spans = sorted((s.first, s.last, i) for i, s in stats.items())
        for (a0, a1, ai), (b0, b1, bi) in zip(spans, spans[1:]):
            if b0 < a1:
                P.append(Problem("V_MANIFEST_SEQ", ERROR, "SEQ=1 pero los objetos %d y %d se intercalan" % (ai, bi)))
    return P


UNIT_MM = 10.0
