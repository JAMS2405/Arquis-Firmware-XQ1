# SPDX-License-Identifier: LGPL-3.0-or-later
"""Diagnostico DENTRO de Cura: vuelca a un JSON lo que el plugin ve en la escena real, nodo por nodo y con los datos CRUDOS de la API de Cura.

Para que sirve: validar el plugin con Cura 5.11 REAL. Las pruebas automaticas usan nodos falsos y por eso no pueden validar la API; este volcado,
generado por Cura real, lo comprueba ``cura/tools/verify_cura_capture.py`` (recalcula la conversion de ejes con los numeros que dio Cura y la
compara con lo que capturo el plugin). Cada llamada a la API de Cura va protegida: si una falla, se anota en ``errors`` y el volcado sigue, de
modo que el informe dice exactamente QUE parte de la API no funciona.

``build_dump`` es una funcion pura (se prueba con nodos falsos y el volcado lo marca como ``source: "fake"``); solo ``capture_dump`` toca Cura.
"""
import datetime
import json
import math
import os
import platform
from typing import Any, Dict, Iterable, List, Optional

from arquis_engines.object_engine import SceneInstance
from arquis_adapters import cura_scene_adapter as scene

SCHEMA = "arquis_cura_capture_v1"


def _try(errors: List[str], what: str, fn, default=None):
    try:
        return fn()
    except Exception as e:                                         # noqa: BLE001 - se informa, no se oculta
        errors.append("%s: %s: %s" % (what, type(e).__name__, e))
        return default


def _box(b: Any) -> Optional[Dict[str, float]]:
    if b is None:
        return None
    return {k: float(getattr(b, k)) for k in ("left", "right", "front", "back", "top", "bottom")}


def node_report(index: int, node: Any, build_plate: Optional[int]) -> Dict[str, Any]:
    """Datos crudos de un nodo de la escena, cada uno con su propia proteccion."""
    err: List[str] = []
    rep: Dict[str, Any] = {"index": index, "class": type(node).__name__, "errors": err}
    rep["name"] = _try(err, "getName", node.getName)
    mesh = _try(err, "getMeshData", node.getMeshData)
    rep["has_mesh"] = mesh is not None and _try(err, "mesh.getVertices", mesh.getVertices) is not None
    rep["is_sliceable"] = bool(_try(err, "callDecoration(isSliceable)", lambda: node.callDecoration("isSliceable"), False))
    rep["is_group"] = bool(_try(err, "callDecoration(isGroup)", lambda: node.callDecoration("isGroup"), False))
    rep["is_non_printing"] = bool(_try(err, "callDecoration(isNonPrintingMesh)", lambda: node.callDecoration("isNonPrintingMesh"), False))
    rep["plate"] = _try(err, "callDecoration(getBuildPlateNumber)", lambda: node.callDecoration("getBuildPlateNumber"))
    rep["outside_buildarea"] = bool(getattr(node, "_outside_buildarea", False))
    rep["bbox_scene"] = _box(_try(err, "getBoundingBox", node.getBoundingBox))
    pos = _try(err, "getWorldPosition", node.getWorldPosition)
    rep["world_position"] = [float(pos.x), float(pos.y), float(pos.z)] if pos is not None else None
    rep["yaw_deg"] = scene._yaw_deg(node)
    parent = _try(err, "getParent", node.getParent)
    rep["parent"] = _try(err, "parent.getName", parent.getName) if parent is not None else None
    rep["parent_is_group"] = bool(_try(err, "parent.callDecoration(isGroup)", lambda: parent.callDecoration("isGroup"), False)) if parent is not None else False
    hull = _try(err, "callDecoration(getConvexHull)", lambda: node.callDecoration("getConvexHull"))
    pts = _try(err, "hull.getPoints", lambda: [(float(x), float(z)) for x, z in hull.getPoints()], []) if hull is not None else []
    rep["hull_points"] = pts
    rep["is_printable_leaf"] = bool(_try(err, "is_printable_leaf", lambda: scene._is_printable_leaf(node, build_plate), False))
    return rep


def build_dump(nodes: Iterable[Any], width: float, depth: float, center_is_zero: bool, build_plate: Optional[int], env: Dict[str, Any],
               source: str) -> Dict[str, Any]:
    """``source``: "cura" (nodos reales) o "fake" (pruebas): la herramienta de verificacion NUNCA da por validado Cura con "fake"."""
    nodes = list(nodes)
    errors: List[str] = []
    reports = [node_report(i, n, build_plate) for i, n in enumerate(nodes)]
    instances: List[SceneInstance] = _try(errors, "instances_from_nodes", lambda: scene.instances_from_nodes(nodes, width, depth, center_is_zero, build_plate), [])
    return {
        "schema": SCHEMA, "source": source, "created": datetime.datetime.now().astimezone().isoformat(timespec="seconds"),
        "arquis_plugin": env.get("plugin_version", ""), "cura": env.get("cura", {}), "python": platform.python_version(),
        "machine": {"width": width, "depth": depth, "center_is_zero": center_is_zero, **env.get("machine", {})},
        "build_plate": build_plate, "nodes": reports,
        "instances": [{"scene_index": i.scene_index, "name": i.name, "bbox": [i.x0, i.y0, i.x1, i.y1], "group": i.group, "group_name": i.group_name,
                       "shape": i.shape, "rot": i.rot, "z1": i.z1} for i in instances],
        "bed": list(scene.bed_from_machine(width, depth, center_is_zero)), "errors": errors,
    }


def _plugin_version() -> str:
    try:
        with open(os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "plugin.json"), encoding="utf-8") as f:
            return str(json.load(f).get("version", ""))
    except Exception:                                                       # noqa: BLE001
        return ""


def capture_dump() -> Dict[str, Any]:
    """Lee la escena activa de Cura REAL. Solo se puede llamar dentro de Cura."""
    from UM.Scene.Iterator.DepthFirstIterator import DepthFirstIterator   # noqa: PLC0415 (solo existe dentro de Cura)
    from cura.CuraApplication import CuraApplication                       # noqa: PLC0415

    app = CuraApplication.getInstance()
    errors: List[str] = []
    stack = app.getGlobalContainerStack()
    if stack is None:
        raise RuntimeError("no hay impresora activa en Cura")
    prop = lambda k: stack.getProperty(k, "value")                          # noqa: E731
    width, depth = float(prop("machine_width")), float(prop("machine_depth"))
    center_zero = bool(prop("machine_center_is_zero"))
    version = _try(errors, "Application.getVersion", app.getVersion, "")
    plate = _try(errors, "getMultiBuildPlateModel", lambda: app.getMultiBuildPlateModel().activeBuildPlate)
    root = app.getController().getScene().getRoot()
    env = {"plugin_version": _plugin_version(), "cura": {"version": version, "application": type(app).__name__},
           "machine": {"name": _try(errors, "stack.getName", stack.getName, ""), "definition_id": _try(errors, "definition.getId", lambda: stack.definition.getId(), ""),
                       "height": _try(errors, "machine_height", lambda: float(prop("machine_height"))),
                       "print_sequence": _try(errors, "print_sequence", lambda: str(prop("print_sequence")))}}
    d = build_dump(DepthFirstIterator(root), width, depth, center_zero, plate, env, source="cura")
    d["errors"] = errors + d["errors"]
    return d


def save_dump(dump: Dict[str, Any], path: Optional[str] = None) -> str:
    path = path or os.path.join(os.path.expanduser("~"), "arquis_cura_captura.json")
    with open(path, "w", encoding="utf-8") as f:
        json.dump(dump, f, indent=1, ensure_ascii=False, default=lambda o: None if isinstance(o, float) and math.isnan(o) else str(o))
    return path
