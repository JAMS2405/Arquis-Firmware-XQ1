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
from typing import Any, Iterable, List, Optional, Sequence

from arquis_engines.object_engine import SceneInstance


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
        out.append(SceneInstance(i, node.getName(), x0, y0, x1, y1, group, group_name))
    return out


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
