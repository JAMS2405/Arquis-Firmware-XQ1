# SPDX-License-Identifier: LGPL-3.0-or-later
"""Escena de Cura -> lista de ``SceneInstance`` en coordenadas del G-code.

API de Cura 5.11.0 / Uranium usadas (todas leidas en el codigo fuente, tag 5.11.0):
  * ``UM.Scene.Iterator.DepthFirstIterator.DepthFirstIterator(root)``                      recorrido de la escena
  * ``node.getName()``, ``node.getMeshData()``, ``node.getBoundingBox()``                  (SceneNode; AxisAlignedBox: left/right/front/back)
  * ``node.callDecoration("isSliceable" | "isNonPrintingMesh" | "isGroup" | "getBuildPlateNumber")``
  * ``getattr(node, "_outside_buildarea", False)``                                         mismo filtro que StartSliceJob.py:379-391
  * ``CuraApplication.getInstance().getMultiBuildPlateModel().activeBuildPlate``
  * ``global_stack.getProperty("machine_width" | "machine_depth" | "machine_center_is_zero" | "print_sequence", "value")``

Conversion de ejes (StartSliceJob.py:497-499 y CuraEngine MeshGroup.cpp:99-107):
  escena: X derecha, Y arriba, Z hacia el FRENTE de la maquina.   G-code: x = X + ancho/2,  y = -Z + profundidad/2  (si el origen no es el centro).
"""
import math
from typing import Any, Iterable, List, Optional, Sequence, Tuple

from arquis_engines.object_engine import SceneInstance, classify_shape


def bbox_to_gcode(left: float, right: float, front: float, back: float, width: float, depth: float, center_is_zero: bool):
    """Caja de la escena (left/right en X, back/front en Z) -> (x0, y0, x1, y1) en coordenadas de G-code."""
    ox = 0.0 if center_is_zero else width / 2.0
    oy = 0.0 if center_is_zero else depth / 2.0
    return (left + ox, -front + oy, right + ox, -back + oy)       # front (mayor Z) -> menor Y


def _is_printable_leaf(node: Any, build_plate: Optional[int]) -> bool:
    """Mismo criterio que StartSliceJob (modo no secuencial): hoja con malla, rebanable, en la placa activa, dentro del area."""
    mesh = node.getMeshData()
    if not (node.callDecoration("isSliceable") and mesh is not None and mesh.getVertices() is not None):
        return False
    if node.callDecoration("isNonPrintingMesh"):
        return False                                   # mallas modificadoras / de soporte: no producen su propio ;MESH:
    if build_plate is not None and node.callDecoration("getBuildPlateNumber") != build_plate:
        return False
    if getattr(node, "_outside_buildarea", False):
        return False
    return True


def _yaw_deg(node: Any) -> int:
    """Giro alrededor de la vertical (Y de la escena = Z del G-code), en grados 0..359; el sentido coincide con el del G-code visto desde arriba."""
    try:
        q = node.getWorldOrientation()
        yaw = math.atan2(2.0 * (q.w * q.y + q.x * q.z), 1.0 - 2.0 * (q.y * q.y + q.z * q.z))
        return int(round(math.degrees(yaw))) % 360
    except Exception:
        return 0


def _shape(node: Any, box: Any, width: float, depth: float, center_is_zero: bool) -> str:
    """Forma de referencia del marcador (C/S/R) a partir de la huella convexa de Cura; sin huella, de la caja."""
    ox = 0.0 if center_is_zero else width / 2.0
    oy = 0.0 if center_is_zero else depth / 2.0
    try:
        hull = node.callDecoration("getConvexHull")
        pts = [(float(x) + ox, -float(z) + oy) for x, z in hull.getPoints()] if hull is not None else []
        if len(pts) >= 3:
            return classify_shape(pts)
    except Exception:
        pass
    w, d = box.right - box.left, box.front - box.back
    return "S" if w > 0 and abs(w - d) <= 0.12 * max(w, d) else "R"


def instances_from_nodes(nodes: Iterable[Any], width: float, depth: float, center_is_zero: bool,
                         build_plate: Optional[int] = None) -> List[SceneInstance]:
    """``nodes`` en el orden de recorrido de la escena. Funcion pura: se prueba con nodos falsos."""
    out: List[SceneInstance] = []
    nodes = list(nodes)
    ids = {id(n): i for i, n in enumerate(nodes)}
    for i, node in enumerate(nodes):
        if not _is_printable_leaf(node, build_plate):
            continue
        box = node.getBoundingBox()
        if box is None:
            continue
        x0, y0, x1, y1 = bbox_to_gcode(box.left, box.right, box.front, box.back, width, depth, center_is_zero)
        group = None
        group_name = None
        parent = node.getParent()
        if parent is not None and parent.callDecoration("isGroup"):
            group = ids.get(id(parent), -1 - i)         # el grupo puede no estar en la lista (no es hoja): id propio estable
            if group < 0:
                group = 1_000_000 + (id(parent) % 1_000_000)
            group_name = parent.getName()
        out.append(SceneInstance(i, node.getName(), x0, y0, x1, y1, group, group_name,
                                 shape=_shape(node, box, width, depth, center_is_zero), rot=_yaw_deg(node), z1=max(0.0, float(box.top))))
    return out


def bed_from_machine(width: float, depth: float, center_is_zero: bool) -> Tuple[float, float, float, float]:
    """Cama en coordenadas de G-code: (min_x, min_y, ancho, profundo) mm."""
    return (-width / 2.0, -depth / 2.0, width, depth) if center_is_zero else (0.0, 0.0, width, depth)


def capture_bed() -> Tuple[float, float, float, float]:
    """Cama de la impresora activa de Cura (machine_width/depth/center_is_zero)."""
    from cura.CuraApplication import CuraApplication                       # noqa: PLC0415
    stack = CuraApplication.getInstance().getGlobalContainerStack()
    if stack is None:
        raise RuntimeError("no hay impresora activa")
    return bed_from_machine(float(stack.getProperty("machine_width", "value")), float(stack.getProperty("machine_depth", "value")),
                            bool(stack.getProperty("machine_center_is_zero", "value")))


def capture_scene() -> List[SceneInstance]:
    """Lee la escena activa de Cura. Solo se puede llamar dentro de Cura."""
    from UM.Scene.Iterator.DepthFirstIterator import DepthFirstIterator   # noqa: PLC0415 (solo existe dentro de Cura)
    from cura.CuraApplication import CuraApplication                       # noqa: PLC0415

    app = CuraApplication.getInstance()
    stack = app.getGlobalContainerStack()
    if stack is None:
        raise RuntimeError("no hay impresora activa")
    width = float(stack.getProperty("machine_width", "value"))
    depth = float(stack.getProperty("machine_depth", "value"))
    center_zero = bool(stack.getProperty("machine_center_is_zero", "value"))
    plate = app.getMultiBuildPlateModel().activeBuildPlate
    root = app.getController().getScene().getRoot()
    return instances_from_nodes(DepthFirstIterator(root), width, depth, center_zero, plate)
