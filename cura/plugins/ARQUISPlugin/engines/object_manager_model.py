# SPDX-License-Identifier: LGPL-3.0-or-later
"""Modelo de vista del Gestor de objetos: lista + mapa de cama, SINCRONIZADOS (una sola fuente de datos y un unico indice de seleccion).

Es Python puro: lo consume la interfaz de Cura (QML) y las pruebas. Toda la geometria sale de ``bed_map_engine`` (la misma que
ejecutara el firmware), asi lo que se ve en Cura es lo que dibujara la pantalla de la impresora.
"""
from typing import Dict, List, Optional, Sequence, Tuple

from . import bed_map_engine as bm
from . import protocol_limits as limits
from .object_engine import ArquisObject

GRID_MM = 50


def build_view_model(objects: Sequence[ArquisObject], bed_mm: Tuple[float, float, float, float], selected: int = -1,
                     view: Optional[bm.View] = None, states: Optional[Dict[int, int]] = None,
                     capacity: int = limits.DESIGN_CAPACITY, cura_bed_note: bool = True) -> dict:
    """``selected``: id del objeto seleccionado (-1 = ninguno). ``states``: estados por id (demo/pruebas); por defecto, antes de
    imprimir: Pendiente para los cancelables y No disponible para los que superan la capacidad."""
    view = view or bm.View(0, 0, 266, 266)
    bed = bm.Bed(round(bed_mm[0] * 10), round(bed_mm[1] * 10), round(bed_mm[2] * 10), round(bed_mm[3] * 10))
    markers, rows, ms = [], [], []
    for o in objects:
        x0, y0, x1, y1 = o.bbox
        cx, cy, w, d = round((x0 + x1) / 2 * 10), round((y0 + y1) / 2 * 10), max(1, round((x1 - x0) * 10)), max(1, round((y1 - y0) * 10))
        key = o.id if o.cancelable else -(o.scene_order + 1)         # los no cancelables no tienen id: clave negativa unica
        state = (states or {}).get(key, bm.PENDING if o.cancelable else bm.UNAVAILABLE)
        m = bm.marker(key, cx, cy, w, d, o.shape, bed, view)
        ms.append(m)
        label = str(bm.display_id(o.id)) if o.cancelable else "?"
        name = o.base_name + (" %d" % (o.instance + 1) if sum(1 for p in objects if p.base_name == o.base_name) > 1 else "")
        markers.append({"id": key, "label": label, "badge": bm.STATE_BADGE[state], "x0": m.x0, "y0": m.y0, "x1": m.x1, "y1": m.y1,
                        "w": m.w, "h": m.h, "shape": o.shape, "state": state, "stateText": bm.STATE_TEXT[state], "selected": key == selected,
                        "overlap": False})
        rows.append({"id": key, "label": label, "name": name, "x": round(cx / 10.0, 1), "y": round(cy / 10.0, 1),
                     "size": "%.1f x %.1f" % (w / 10.0, d / 10.0), "shape": o.shape, "state": state, "stateText": bm.STATE_TEXT[state],
                     "badge": bm.STATE_BADGE[state], "cancelable": o.cancelable, "selected": key == selected})
    touching = {i for pair in bm.overlaps(ms) for i in pair}
    for mk in markers:
        mk["overlap"] = mk["id"] in touching
    # cama y cuadricula (cada 50 mm), en pixeles
    (bx0, by1), (bx1, by0) = bm.to_px(bed.min_x, bed.min_y, bed, view), bm.to_px(bed.min_x + bed.width, bed.min_y + bed.depth, bed, view)
    grid = []
    gx = (bed.min_x // (GRID_MM * 10) + 1) * GRID_MM * 10
    while gx < bed.min_x + bed.width:
        px, _ = bm.to_px(gx, bed.min_y, bed, view)
        grid.append({"x0": px, "y0": by0, "x1": px, "y1": by1})
        gx += GRID_MM * 10
    gy = (bed.min_y // (GRID_MM * 10) + 1) * GRID_MM * 10
    while gy < bed.min_y + bed.depth:
        _, py = bm.to_px(bed.min_x, gy, bed, view)
        grid.append({"x0": bx0, "y0": py, "x1": bx1, "y1": py})
        gy += GRID_MM * 10
    over = sum(1 for o in objects if not o.cancelable)
    warnings = []
    if over:
        warnings.append("%d objetos superan la capacidad de cancelacion (%d): se imprimen pero no se podran cancelar." % (over, capacity))
    if any(mk["overlap"] for mk in markers):
        warnings.append("Algunos marcadores se tocan en pantalla: se pueden elegir desde la lista o ampliando el mapa de la impresora.")
    if cura_bed_note:
        warnings.append("Cama del perfil de Cura: %.0f x %.0f mm. El mapa solo coincide con la impresora si es la misma." % (bed_mm[2], bed_mm[3]))
    return {"bed": {"x0": bx0, "y0": by0, "x1": bx1, "y1": by1, "w": bed_mm[2], "d": bed_mm[3]}, "grid": grid, "markers": markers, "rows": rows,
            "warnings": warnings, "selected": selected,
            "summary": "%d objetos · %d cancelables (maximo %d)" % (len(objects), len(objects) - over, capacity),
            "_markers": ms}


def pick_id(vm: dict, px: int, py: int) -> int:
    """Id (o clave negativa) del marcador bajo un toque en el mapa; None si no hay (``-1`` es 'ninguno' para la seleccion)."""
    ms = vm["_markers"]
    r = bm.pick(ms, px, py)
    return r if any(m.id == r for m in ms) else None
